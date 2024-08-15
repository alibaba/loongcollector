// Copyright 2021 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package httpserver

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"net/http"
	"syscall"
	"testing"
	"time"

	"github.com/stretchr/testify/require"
	"gotest.tools/assert"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	pluginmanager "github.com/alibaba/ilogtail/pluginmanager"
	_ "github.com/alibaba/ilogtail/plugins/extension/default_decoder"
)

type ContextTest struct {
	pluginmanager.ContextImp

	Logs []string
}

func (p *ContextTest) LogWarnf(alarmType string, format string, params ...interface{}) {
	p.Logs = append(p.Logs, alarmType+":"+fmt.Sprintf(format, params...))
}

func (p *ContextTest) LogErrorf(alarmType string, format string, params ...interface{}) {
	p.Logs = append(p.Logs, alarmType+":"+fmt.Sprintf(format, params...))
}

func (p *ContextTest) LogWarn(alarmType string, kvPairs ...interface{}) {
	fmt.Println(alarmType, kvPairs)
}

func (p *ContextTest) GetExtension(name string, cfg any) (pipeline.Extension, error) {
	creator, ok := pipeline.Extensions[name]
	if !ok {
		return nil, fmt.Errorf("extension not found")
	}
	extension := creator()
	config, err := json.Marshal(cfg)
	if err != nil {
		return nil, err
	}
	err = json.Unmarshal(config, extension)
	if err != nil {
		return nil, err
	}
	extension.Init(&p.ContextImp)
	return extension, nil
}

func newInputWithOpts(format string, option func(input *ServiceHTTP)) (*ServiceHTTP, error) {
	ctx := &ContextTest{}
	ctx.ContextImp.InitContext("a", "b", "c")
	input := &ServiceHTTP{
		ReadTimeoutSec:     10,
		ShutdownTimeoutSec: 5,
		MaxBodySize:        64 * 1024 * 1024,
		Address:            ":0",
		Format:             format,
		Decoder:            "ext_default_decoder",
	}
	if option != nil {
		option(input)
	}
	_, err := input.Init(ctx)
	return input, err
}

func newInput(format string) (*ServiceHTTP, error) {
	return newInputWithOpts(format, nil)
}

type mockLog struct {
	tags   map[string]string
	fields map[string]string
}

type mockCollector struct {
	logs    []*mockLog
	rawLogs []*protocol.Log
}

func (c *mockCollector) AddData(
	tags map[string]string, fields map[string]string, t ...time.Time) {
	c.AddDataWithContext(tags, fields, nil, t...)
}

func (c *mockCollector) AddDataArray(
	tags map[string]string, columns []string, values []string, t ...time.Time) {
	c.AddDataArrayWithContext(tags, columns, values, nil, t...)
}

func (c *mockCollector) AddRawLog(log *protocol.Log) {
	c.AddRawLogWithContext(log, nil)
}

func (c *mockCollector) AddDataWithContext(
	tags map[string]string, fields map[string]string, ctx map[string]interface{}, t ...time.Time) {
	c.logs = append(c.logs, &mockLog{tags, fields})
}

func (c *mockCollector) AddDataArrayWithContext(
	tags map[string]string, columns []string, values []string, ctx map[string]interface{}, t ...time.Time) {
}

func (c *mockCollector) AddRawLogWithContext(log *protocol.Log, ctx map[string]interface{}) {
	c.rawLogs = append(c.rawLogs, log)
}

var textFormatProm = `# HELP http_requests_total The total number of HTTP requests.
# TYPE http_requests_total counter
http_requests_total{method="post",code="200"} 1027 1395066363000
http_requests_total{method="post",code="400"}    3 1395066363000

# Escaping in label values:
msdos_file_access_time_seconds{path="C:\\DIR\\FILE.TXT",error="Cannot find file:\n\"FILE.TXT\""} 1.458255915e9

# Minimalistic line:
metric_without_timestamp_and_labels 12.47

# A weird metric from before the epoch:
something_weird{problem="division by zero"} +Inf -3982045

# A histogram, which has a pretty complex representation in the text format:
# HELP http_request_duration_seconds A histogram of the request duration.
# TYPE http_request_duration_seconds histogram
http_request_duration_seconds_bucket{le="0.05"} 24054
http_request_duration_seconds_bucket{le="0.1"} 33444
http_request_duration_seconds_bucket{le="0.2"} 100392
http_request_duration_seconds_bucket{le="0.5"} 129389
http_request_duration_seconds_bucket{le="1"} 133988
http_request_duration_seconds_bucket{le="+Inf"} 144320
http_request_duration_seconds_sum 53423
http_request_duration_seconds_count 144320

# Finally a summary, which has a complex representation, too:
# HELP telemetry_requests_metrics_latency_microseconds A summary of the response latency.
# TYPE telemetry_requests_metrics_latency_microseconds summary
telemetry_requests_metrics_latency_microseconds{quantile="0.01"} 3102
telemetry_requests_metrics_latency_microseconds{quantile="0.05"} 3272
telemetry_requests_metrics_latency_microseconds{quantile="0.5"} 4773
telemetry_requests_metrics_latency_microseconds{quantile="0.9"} 9001
telemetry_requests_metrics_latency_microseconds{quantile="0.99"} 76656
telemetry_requests_metrics_latency_microseconds_sum 1.7560473e+07
telemetry_requests_metrics_latency_microseconds_count 2693
`

var textFormatInflux = `
# integer value
cpu value=1i

# float value
cpu_load value=1

cpu_load value=1.0

cpu_load value=1.2

# boolean value
error fatal=true

# string value
event msg="logged out"

# multiple values
cpu load=10,alert=true,reason="value above maximum threshold"

cpu,host=server01,region=uswest value=1 1434055562000000000
cpu,host=server02,region=uswest value=3 1434055562000010000
temperature,machine=unit42,type=assembly internal=32,external=100 1434055562000000035
temperature,machine=unit143,type=assembly internal=22,external=130 1434055562005000035
cpu,host=server\ 01,region=uswest value=1,msg="all systems nominal"
cpu,host=server\ 01,region=us\,west value_int=1i
`

func sendRequest(bodyToSend string, port int) error {
	url := fmt.Sprintf("http://localhost:%d/notes", port)

	var jsonStr = []byte(bodyToSend)
	req, err := http.NewRequest("POST", url, bytes.NewBuffer(jsonStr))
	if err != nil {
		return err
	}
	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	fmt.Println("response Status:", resp.Status)
	fmt.Println("response Headers:", resp.Header)
	body, _ := io.ReadAll(resp.Body)
	fmt.Println("response Body:", string(body))
	return nil
}

func TestInputPrometheus(t *testing.T) {

	input, err := newInput("prometheus")
	require.NoError(t, err)
	collector := &mockCollector{}
	err = input.Start(collector)
	require.NoError(t, err)
	port := input.listener.Addr().(*net.TCPAddr).Port
	defer func() {
		require.NoError(t, input.Stop())
	}()

	err = sendRequest(textFormatProm, port)
	require.NoError(t, err)
	err = sendRequest(textFormatProm, port)
	require.NoError(t, err)

	time.Sleep(time.Second * 2)

	assert.Equal(t, 40, len(collector.rawLogs))
	for _, log := range collector.rawLogs {
		fmt.Println(log.String())
	}

}

func TestInputInfluxDB(t *testing.T) {
	input, err := newInput("influx")
	require.NoError(t, err)
	collector := &mockCollector{}
	err = input.Start(collector)
	require.NoError(t, err)
	port := input.listener.Addr().(*net.TCPAddr).Port

	defer func() {
		require.NoError(t, input.Stop())
	}()

	err = sendRequest(textFormatInflux, port)
	require.NoError(t, err)
	err = sendRequest(textFormatInflux, port)
	require.NoError(t, err)

	time.Sleep(time.Second * 2)

	assert.Equal(t, 30, len(collector.rawLogs))
	for _, log := range collector.rawLogs {
		fmt.Println(log.String())
	}
}

func TestInputInfluxDBWithFieldsExtend(t *testing.T) {
	input, err := newInputWithOpts("influx", func(input *ServiceHTTP) {
		input.FieldsExtend = true
	})
	require.NoError(t, err)
	collector := &mockCollector{}
	err = input.Start(collector)
	require.NoError(t, err)
	port := input.listener.Addr().(*net.TCPAddr).Port

	defer func() {
		require.NoError(t, input.Stop())
	}()

	err = sendRequest(textFormatInflux, port)
	require.NoError(t, err)
	err = sendRequest(textFormatInflux, port)
	require.NoError(t, err)

	time.Sleep(time.Second * 2)

	assert.Equal(t, 36, len(collector.rawLogs))
	for _, log := range collector.rawLogs {
		fmt.Println(log.String())
	}
}

func TestUnlinkUnixSock(t *testing.T) {
	const sockPath = "test_service_http_server_unlink_unix_sock.run"
	_ = syscall.Unlink(sockPath)
	listener, err := net.Listen("unix", sockPath)
	require.NoError(t, err)
	defer listener.Close()

	ctx := &ContextTest{}
	ctx.ContextImp.InitContext("a", "b", "c")
	input := &ServiceHTTP{
		Address:        "unix://" + sockPath,
		Format:         "influx",
		UnlinkUnixSock: true,
		Decoder:        "ext_default_decoder",
	}
	_, err = input.Init(ctx)
	require.NoError(t, err)

	collector := &mockCollector{}
	require.NoError(t, input.Start(collector))
	require.NoError(t, input.Stop())
}

func sendRequestWithParams(bodyToSend string, port int, headers map[string]string, queries map[string]string) error {
	url := fmt.Sprintf("http://localhost:%d/notes", port)

	var jsonStr = []byte(bodyToSend)
	req, err := http.NewRequest("POST", url, bytes.NewBuffer(jsonStr))
	if err != nil {
		return err
	}
	for key, value := range headers {
		req.Header.Add(key, value)
	}

	query := req.URL.Query()
	for key, value := range queries {
		query.Add(key, value)
	}
	req.URL.RawQuery = query.Encode()

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	fmt.Println("response Status:", resp.Status)
	fmt.Println("response Headers:", resp.Header)
	body, _ := io.ReadAll(resp.Body)
	fmt.Println("response Body:", string(body))
	return nil
}

func TestInputWithRequestParams(t *testing.T) {
	input, err := newInput("raw")
	require.NoError(t, err)
	input.HeaderParamPrefix = "_header_prefix_"
	input.HeaderParams = []string{"Test-A", "Test-B"}
	input.QueryParamPrefix = "_query_prefix_"
	input.QueryParams = []string{"db"}

	inputCtx := helper.NewObservePipelineConext(10)
	err = input.StartService(inputCtx)
	require.NoError(t, err)

	port := input.listener.Addr().(*net.TCPAddr).Port
	mockHeader := map[string]string{"Test-A": "a", "Test-B": "b"}
	mockQuery := map[string]string{"db": "test"}
	err = sendRequestWithParams(textFormatInflux, port, mockHeader, mockQuery)
	require.NoError(t, err)
	err = sendRequestWithParams(textFormatInflux, port, mockHeader, mockQuery)
	require.NoError(t, err)

	time.Sleep(time.Second * 2)
	res := inputCtx.Collector().ToArray()
	assert.Equal(t, 2, len(res))
	for _, groupEvents := range res {
		for _, key := range input.HeaderParams {
			assert.Equal(t, mockHeader[key], groupEvents.Group.Metadata.Get(input.HeaderParamPrefix+key))
		}
		for _, key := range input.QueryParams {
			assert.Equal(t, mockQuery[key], groupEvents.Group.Metadata.Get(input.QueryParamPrefix+key))
		}
	}

}

func TestInputWithRequestParamsWithoutPrefix(t *testing.T) {
	input, err := newInput("raw")
	require.NoError(t, err)
	input.HeaderParams = []string{"Test-A", "Test-B"}
	input.QueryParams = []string{"db"}

	inputCtx := helper.NewObservePipelineConext(10)
	err = input.StartService(inputCtx)
	require.NoError(t, err)

	port := input.listener.Addr().(*net.TCPAddr).Port
	mockHeader := map[string]string{"Test-A": "a", "Test-B": "b"}
	mockQuery := map[string]string{"db": "test"}
	err = sendRequestWithParams(textFormatInflux, port, mockHeader, mockQuery)
	require.NoError(t, err)
	err = sendRequestWithParams(textFormatInflux, port, mockHeader, mockQuery)
	require.NoError(t, err)

	time.Sleep(time.Second * 2)
	res := inputCtx.Collector().ToArray()
	assert.Equal(t, 2, len(res))
	for _, groupEvents := range res {
		for _, key := range input.HeaderParams {
			assert.Equal(t, mockHeader[key], groupEvents.Group.Metadata.Get(key))
		}
		for _, key := range input.QueryParams {
			assert.Equal(t, mockQuery[key], groupEvents.Group.Metadata.Get(key))
		}
	}

}

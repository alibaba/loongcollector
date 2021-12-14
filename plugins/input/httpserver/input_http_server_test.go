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
	"fmt"
	"io/ioutil"
	"net"
	"net/http"
	"syscall"
	"testing"
	"time"

	"github.com/alibaba/ilogtail/pkg/protocol"
	pluginmanager "github.com/alibaba/ilogtail/pluginmanager"

	"github.com/stretchr/testify/require"

	"gotest.tools/assert"
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

func newInput(format string) (*ServiceHTTP, error) {
	ctx := &ContextTest{}
	ctx.ContextImp.InitContext("a", "b", "c")
	input := &ServiceHTTP{
		ReadTimeoutSec:     10,
		ShutdownTimeoutSec: 5,
		MaxBodySize:        64 * 1024 * 1024,
		Address:            ":0",
		Format:             format,
	}
	_, err := input.Init(&ctx.ContextImp)
	return input, err
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
	c.logs = append(c.logs, &mockLog{tags, fields})
}

func (c *mockCollector) AddDataArray(
	tags map[string]string, columns []string, values []string, t ...time.Time) {
}

func (c *mockCollector) AddRawLog(log *protocol.Log) {
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
	body, _ := ioutil.ReadAll(resp.Body)
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
	}
	_, err = input.Init(&ctx.ContextImp)
	require.NoError(t, err)

	collector := &mockCollector{}
	require.NoError(t, input.Start(collector))
	require.NoError(t, input.Stop())
}

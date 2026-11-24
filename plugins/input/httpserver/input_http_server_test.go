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
	"net/http/httptest"
	"syscall"
	"testing"
	"time"

	"github.com/golang/snappy"
	"github.com/prometheus/prometheus/prompb"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/encoding/protowire"
	"gotest.tools/assert"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	pluginmanager "github.com/alibaba/ilogtail/pluginmanager"
	_ "github.com/alibaba/ilogtail/plugins/extension/default_decoder"
	"github.com/alibaba/ilogtail/plugins/test"
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

func collectRequestEvents(t *testing.T, input *ServiceHTTP, body []byte, status int) []*models.PipelineGroupEvents {
	t.Helper()
	ctx := helper.NewObservePipelineContext(10)
	input.collector = ctx.Collector()
	request := httptest.NewRequest(http.MethodPost, "/notes?example_query=preserved", bytes.NewReader(body))
	response := httptest.NewRecorder()
	input.ServeHTTP(response, request)
	require.Equal(t, status, response.Code)
	if input.Format == "sls" && status == http.StatusOK {
		require.NotEmpty(t, response.Header().Get("x-log-requestid"))
	}
	return ctx.Collector().ToArray()
}

func TestInputPrometheus(t *testing.T) {
	input, err := newInput("prometheus")
	require.NoError(t, err)
	groups := collectRequestEvents(t, input, []byte(textFormatProm), http.StatusOK)
	count := 0
	for _, group := range groups {
		for _, event := range group.Events {
			require.IsType(t, &models.Metric{}, event)
			count++
		}
	}
	require.Equal(t, 20, count)
}

func TestInputPrometheusRemoteWrite(t *testing.T) {
	input, err := newInput("prometheus")
	require.NoError(t, err)
	input.collector = helper.NewObservePipelineContext(10).Collector()
	write := &prompb.WriteRequest{Timeseries: []prompb.TimeSeries{{
		Labels:  []prompb.Label{{Name: "__name__", Value: "requests"}},
		Samples: []prompb.Sample{{Timestamp: 1700000000123, Value: 10}},
	}}}
	body, err := write.Marshal()
	require.NoError(t, err)
	request := httptest.NewRequest(http.MethodPost, "/write?db=metrics", bytes.NewReader(snappy.Encode(nil, body)))
	request.Header.Set("Content-Encoding", "snappy")
	request.Header.Set("Content-Type", "application/x-protobuf")
	response := httptest.NewRecorder()
	input.ServeHTTP(response, request)
	require.Equal(t, http.StatusOK, response.Code)
	groups := input.collector.ToArray()
	require.Len(t, groups, 1)
	require.Equal(t, "metrics", groups[0].Group.Metadata.Get("db"))
	require.Len(t, groups[0].Events, 1)
	require.IsType(t, &models.Metric{}, groups[0].Events[0])
	metric := groups[0].Events[0].(*models.Metric)
	require.Equal(t, "requests", metric.GetName())
	require.Equal(t, uint64(1700000000123000000), metric.GetTimestamp())
}

func TestInputRawLog(t *testing.T) {
	input, err := newInputWithOpts(" RAW ", func(input *ServiceHTTP) {
		input.QueryParams = []string{"example_query"}
	})
	require.NoError(t, err)
	for _, body := range []string{"raw\x00\xff", ""} {
		before := time.Now().Unix()
		groups := collectRequestEvents(t, input, []byte(body), http.StatusNoContent)
		after := time.Now().Unix()
		require.Len(t, groups, 1)
		require.Equal(t, map[string]string{"example_query": "preserved"}, groups[0].Group.Metadata.Iterator())
		require.Len(t, groups[0].Events, 1)
		require.IsType(t, &models.Log{}, groups[0].Events[0])
		event := groups[0].Events[0].(*models.Log)
		require.Equal(t, body, event.Contents.Get(models.ContentKey))
		log, err := helper.TransferLogEventToPB(event)
		require.NoError(t, err)
		require.Len(t, log.Contents, 1)
		require.Equal(t, models.ContentKey, string(log.Contents[0].Key))
		require.Equal(t, body, string(log.Contents[0].Value))
		require.Zero(t, log.Timestamp%uint64(time.Second))
		require.GreaterOrEqual(t, log.Timestamp, uint64(before)*uint64(time.Second))
		require.LessOrEqual(t, log.Timestamp, uint64(after)*uint64(time.Second))
	}
}

func TestInputRawWithCustomDecoder(t *testing.T) {
	const customDecoder = "ext_http_raw_test_decoder"
	pipeline.AddExtensionCreator(customDecoder, pipeline.Extensions[defaultDecoder])
	t.Cleanup(func() { delete(pipeline.Extensions, customDecoder) })
	input, err := newInputWithOpts("raw", func(input *ServiceHTTP) {
		input.Decoder = customDecoder
	})
	require.NoError(t, err)
	groups := collectRequestEvents(t, input, []byte("payload"), http.StatusNoContent)
	require.Len(t, groups, 1)
	require.Equal(t, []models.PipelineEvent{models.ByteArray("payload")}, groups[0].Events)
}

func TestInputInfluxDB(t *testing.T) {
	input, err := newInput("influx")
	require.NoError(t, err)
	groups := collectRequestEvents(t, input, []byte(textFormatInflux), http.StatusNoContent)
	require.Len(t, groups, 1)
	require.Len(t, groups[0].Events, 13)
	for _, event := range groups[0].Events {
		require.IsType(t, &models.Metric{}, event)
	}
}

func TestInputInfluxDBWithFieldsExtend(t *testing.T) {
	input, err := newInputWithOpts("influx", func(input *ServiceHTTP) {
		input.FieldsExtend = true
	})
	require.NoError(t, err)
	groups := collectRequestEvents(t, input, []byte(textFormatInflux), http.StatusNoContent)
	require.Len(t, groups, 1)
	require.Len(t, groups[0].Events, 18)
	for _, event := range groups[0].Events {
		require.IsType(t, &models.Log{}, event)
	}
}

func TestInputStatsD(t *testing.T) {
	input, err := newInput("statsd")
	require.NoError(t, err)
	body := []byte("requests:5|c|#service:api\ninvalid:1|g|\nlatency:3.5|g\n")
	groups := collectRequestEvents(t, input, body, http.StatusNoContent)
	require.Len(t, groups, 1)
	require.Len(t, groups[0].Events, 2)
	counter := groups[0].Events[0].(*models.Log)
	require.Equal(t, "requests", counter.GetIndices().Get("__name__"))
	require.Equal(t, "5", counter.GetIndices().Get("__value__"))
	require.Equal(t, "service#$#api", counter.GetIndices().Get("__labels__"))
	require.NotEmpty(t, counter.GetIndices().Get("__time_nano__"))
	gauge := groups[0].Events[1].(*models.Log)
	require.Equal(t, "latency", gauge.GetIndices().Get("__name__"))
	require.Equal(t, "3.5", gauge.GetIndices().Get("__value__"))
}

func TestInputSLS(t *testing.T) {
	input, err := newInputWithOpts("sls", func(input *ServiceHTTP) {
		input.QueryParams = []string{"example_query"}
	})
	require.NoError(t, err)
	nanos := uint32(987654321)
	body := test.AppendSLSWireLog(nil, 1700000000, &nanos,
		[2]string{"content", "message"}, [2]string{"node", "field-not-tag"}, [2]string{"__tag__:as_content", "literal"})
	body = test.AppendSLSWireLog(body, 1700000001, nil, [2]string{"empty", ""})
	for i, value := range []string{"original-logstore", "access", "sender", "sender-id"} {
		body = protowire.AppendString(protowire.AppendTag(body, protowire.Number(i+2), protowire.BytesType), value)
	}
	body = test.AppendSLSWireTag(body, "node", "edge")
	groups := collectRequestEvents(t, input, body, http.StatusOK)
	require.Len(t, groups, 1)
	require.Equal(t, map[string]string{"node": "edge", pipeline.TopicTagKey: "access", "__machine_uuid__": "sender-id"}, groups[0].Group.GetTags().Iterator())
	require.Equal(t, map[string]string{"source": "sender", "example_query": "preserved", "sls.category": "original-logstore"}, groups[0].Group.GetMetadata().Iterator())
	require.Empty(t, groups[0].Group.GetDestinationLogstore())
	require.Len(t, groups[0].Events, 2)

	log := groups[0].Events[0].(*models.Log)
	require.Equal(t, uint64(1700000000987654321), log.Timestamp)
	require.Equal(t, map[string]interface{}{"content": "message", "node": "field-not-tag", "__tag__:as_content": "literal"}, log.GetIndices().Iterator())
	require.Empty(t, log.GetTags().Iterator())
	require.Equal(t, uint64(1700000001000000000), groups[0].Events[1].GetTimestamp())
	require.Equal(t, map[string]interface{}{"empty": ""}, groups[0].Events[1].(*models.Log).GetIndices().Iterator())

	require.Empty(t, collectRequestEvents(t, input, []byte{0x0a}, http.StatusBadRequest))
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

	require.NoError(t, input.StartService(helper.NewNoopPipelineContext()))
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

	inputCtx := helper.NewObservePipelineContext(10)
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

	inputCtx := helper.NewObservePipelineContext(10)
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

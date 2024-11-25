// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      grpc://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package opentelemetry

import (
	"fmt"
	"net/http"
	"strconv"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.opentelemetry.io/collector/pdata/plog/plogotlp"
	"go.opentelemetry.io/collector/pdata/pmetric/pmetricotlp"
	"go.opentelemetry.io/collector/pdata/ptrace/ptraceotlp"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/plugins/test"
)

func TestOtlpGRPC_Logs_V1(t *testing.T) {
	endpointGrpc := test.GetAvailableLocalAddress(t)
	input, err := newInput(true, false, endpointGrpc, "")
	assert.NoError(t, err)

	queueSize := 10
	collector := &helper.LocalCollector{}
	input.Start(collector)
	t.Cleanup(func() {
		require.NoError(t, input.Stop())
	},
	)

	cc, err := grpc.Dial(endpointGrpc, grpc.WithTransportCredentials(insecure.NewCredentials()))
	assert.NoError(t, err)
	defer func() {
		assert.NoError(t, cc.Close())
	}()

	for i := 0; i < queueSize; i++ {
		err = exportLogs(cc, GenerateLogs(i+1))
		assert.NoError(t, err)
	}

	count := 0
	pos := 0
	for count < queueSize {
		count++
		for i := pos; i < pos+count; i++ {
			log := collector.Logs[i]
			assert.Equal(t, "time_unix_nano", log.Contents[0].Key)
			assert.Equal(t, "severity_number", log.Contents[1].Key)
			assert.Equal(t, "severity_text", log.Contents[2].Key)
			assert.Equal(t, "content", log.Contents[3].Key)
			assert.Equal(t, fmt.Sprintf("log body %d", i-pos), log.Contents[3].Value)
			assert.Equal(t, "resources", log.Contents[4].Key)
			assert.Equal(t, "{\"resource-attr\":\"resource-attr-val-1\"}", log.Contents[4].Value)
		}
		pos += count
	}
}

func TestOtlpGRPC_Metrics_V1(t *testing.T) {
	config.LoongcollectorGlobalConfig.EnableSlsMetricsFormat = true
	endpointGrpc := test.GetAvailableLocalAddress(t)
	input, err := newInput(true, false, endpointGrpc, "")
	assert.NoError(t, err)

	queueSize := 10
	collector := &helper.LocalCollector{}
	input.Start(collector)
	t.Cleanup(func() {
		require.NoError(t, input.Stop())
	},
	)

	cc, err := grpc.Dial(endpointGrpc, grpc.WithTransportCredentials(insecure.NewCredentials()))
	assert.NoError(t, err)
	defer func() {
		assert.NoError(t, cc.Close())
	}()

	for i := 0; i < queueSize; i++ {
		err = exportMetrics(cc, GenerateMetrics(i+1))
		assert.NoError(t, err)
	}

	types := make(map[string]int, 16)
	for _, log := range collector.Logs {
		assert.Equal(t, "__name__", log.Contents[0].Key)
		assert.Equal(t, "__labels__", log.Contents[2].Key)
		types[log.Contents[0].Value]++
	}
	fmt.Printf("%v\n", types)
	println(types["gauge_int"])
	assert.Equal(t, 26, types["gauge_int"])
	assert.Equal(t, 22, types["gauge_double"])
	assert.Equal(t, 18, types["sum_int"])
	assert.Equal(t, 14, types["sum_double"])
	assert.Equal(t, 8, types["summary_count"])
	assert.Equal(t, 8, types["summary_sum"])
	assert.Equal(t, types["summary_count"]/2, types["summary"])
	assert.Equal(t, 6, types["histogram_max"])
	assert.Equal(t, 6, types["histogram_min"])
	assert.Equal(t, 6, types["histogram_exemplars"])
	assert.Equal(t, types["histogram_max"]*2, types["histogram_bucket"])
	assert.Equal(t, types["histogram_max"]*2, types["histogram_count"])
	assert.Equal(t, types["histogram_max"]*2, types["histogram_sum"])
	assert.Equal(t, 5, types["exponential_histogram_max"])
	assert.Equal(t, 5, types["exponential_histogram_min"])
	assert.Equal(t, 5, types["exponential_histogram_exemplars"])
	assert.Equal(t, types["exponential_histogram_max"]*(5+7), types["exponential_histogram_bucket"])
	assert.Equal(t, types["exponential_histogram_max"]*2, types["exponential_histogram_count"])
	assert.Equal(t, types["exponential_histogram_max"]*2, types["exponential_histogram_sum"])
}

func TestOtlpGRPC_Metrics_V1_Compress(t *testing.T) {
	endpointGrpc := test.GetAvailableLocalAddress(t)
	input, err := newInput(true, false, endpointGrpc, "", setCompression("gzip"), setDecompression("gzip"))
	assert.NoError(t, err)

	queueSize := 10
	collector := &helper.LocalCollector{}
	input.Start(collector)
	t.Cleanup(func() {
		require.NoError(t, input.Stop())
	},
	)

	cc, err := grpc.Dial(endpointGrpc, grpc.WithTransportCredentials(insecure.NewCredentials()),
		grpc.WithCompressor(grpc.NewGZIPCompressor()),
		grpc.WithDecompressor(grpc.NewGZIPDecompressor()),
	)
	assert.NoError(t, err)
	defer func() {
		assert.NoError(t, cc.Close())
	}()

	for i := 0; i < queueSize; i++ {
		err = exportMetrics(cc, GenerateMetrics(i+1))
		assert.NoError(t, err)
	}

	types := make(map[string]int, 0)
	for _, log := range collector.Logs {
		assert.Equal(t, "__name__", log.Contents[0].Key)
		assert.Equal(t, "__labels__", log.Contents[2].Key)
		types[log.Contents[0].Value]++
	}
	assert.Equal(t, 26, types["gauge_int"])
	assert.Equal(t, 22, types["gauge_double"])
	assert.Equal(t, 18, types["sum_int"])
	assert.Equal(t, 14, types["sum_double"])
	assert.Equal(t, 8, types["summary_count"])
	assert.Equal(t, 8, types["summary_sum"])
	assert.Equal(t, types["summary_count"]/2, types["summary"])
	assert.Equal(t, 6, types["histogram_max"])
	assert.Equal(t, 6, types["histogram_min"])
	assert.Equal(t, 6, types["histogram_exemplars"])
	assert.Equal(t, types["histogram_max"]*2, types["histogram_bucket"])
	assert.Equal(t, types["histogram_max"]*2, types["histogram_count"])
	assert.Equal(t, types["histogram_max"]*2, types["histogram_sum"])
	assert.Equal(t, 5, types["exponential_histogram_max"])
	assert.Equal(t, 5, types["exponential_histogram_min"])
	assert.Equal(t, 5, types["exponential_histogram_exemplars"])
	assert.Equal(t, types["exponential_histogram_max"]*(5+7), types["exponential_histogram_bucket"])
	assert.Equal(t, types["exponential_histogram_max"]*2, types["exponential_histogram_count"])
	assert.Equal(t, types["exponential_histogram_max"]*2, types["exponential_histogram_sum"])
}

func TestOtlpGRPC_Trace_V1(t *testing.T) {
	endpointGrpc := test.GetAvailableLocalAddress(t)
	input, err := newInput(true, false, endpointGrpc, "")
	assert.NoError(t, err)

	queueSize := 10
	collector := &helper.LocalCollector{}
	input.Start(collector)
	t.Cleanup(func() {
		require.NoError(t, input.Stop())
	},
	)

	cc, err := grpc.Dial(endpointGrpc, grpc.WithTransportCredentials(insecure.NewCredentials()))
	assert.NoError(t, err)
	defer func() {
		assert.NoError(t, cc.Close())
	}()

	for i := 0; i < queueSize; i++ {
		err = exportTraces(cc, GenerateTraces(i+1))
		assert.NoError(t, err)
	}

	starttm := time.Date(2020, 2, 11, 20, 26, 12, 321, time.UTC)
	endtm := time.Date(2020, 2, 11, 20, 26, 13, 789, time.UTC)
	traceID := "0102030405060708090a0b0c0d0e0f10"
	spanID := "1112131415161718"
	count := 0
	pos := 0
	for count < queueSize {
		count++
		for i := pos; i < pos+count; i++ {
			log := collector.Logs[i]
			assert.Equal(t, int64(log.Time), endtm.Unix())
			assert.Equal(t, "host", log.Contents[0].Key)
			assert.Equal(t, "service", log.Contents[1].Key)
			assert.Equal(t, "resource", log.Contents[2].Key)
			assert.Equal(t, "{\"resource-attr\":\"resource-attr-val-1\"}", log.Contents[2].Value)
			assert.Equal(t, "otlp.name", log.Contents[3].Key)
			assert.Equal(t, "otlp.version", log.Contents[4].Key)
			assert.Equal(t, "traceID", log.Contents[5].Key)
			assert.Equal(t, "spanID", log.Contents[6].Key)
			assert.Equal(t, "parentSpanID", log.Contents[7].Key)
			assert.Equal(t, "kind", log.Contents[8].Key)
			assert.Equal(t, "name", log.Contents[9].Key)
			assert.Equal(t, "links", log.Contents[10].Key)
			assert.Equal(t, "logs", log.Contents[11].Key)
			if (i-pos)%2 == 0 {
				assert.Equal(t, traceID, log.Contents[5].Value)
				assert.Equal(t, spanID, log.Contents[6].Value)
				assert.Equal(t, "operationA", log.Contents[9].Value)
				assert.Equal(t, "[{\"attribute\":{\"span-event-attr\":\"span-event-attr-val\"},\"name\":\"event-with-attr\",\"time\":1581452773000000123},{\"attribute\":{},\"name\":\"event\",\"time\":1581452773000000123}]", log.Contents[11].Value)
			} else {
				assert.Equal(t, []byte{}, []byte(log.Contents[5].Value))
				assert.Equal(t, []byte{}, []byte(log.Contents[6].Value))
				assert.Equal(t, "operationB", log.Contents[9].Value)
				assert.Equal(t, "[{\"attribute\":{\"span-link-attr\":\"span-link-attr-val\"},\"spanID\":\"\",\"traceID\":\"\"},{\"attribute\":{},\"spanID\":\"\",\"traceID\":\"\"}]", log.Contents[10].Value)
			}
			assert.Equal(t, "traceState", log.Contents[12].Key)
			assert.Equal(t, "start", log.Contents[13].Key)
			assert.Equal(t, strconv.FormatInt(starttm.UnixMicro(), 10), log.Contents[13].Value)
			assert.Equal(t, "end", log.Contents[14].Key)
			assert.Equal(t, strconv.FormatInt(endtm.UnixMicro(), 10), log.Contents[14].Value)
			assert.Equal(t, "duration", log.Contents[15].Key)
			assert.Equal(t, "attribute", log.Contents[16].Key)
			assert.Equal(t, "statusCode", log.Contents[17].Key)
			assert.Equal(t, "statusMessage", log.Contents[18].Key)
		}
		pos += count
	}
}

func TestOtlpHTTP_Logs_V1(t *testing.T) {
	endpointHTTP := test.GetAvailableLocalAddress(t)
	input, err := newInput(false, true, "", endpointHTTP)
	assert.NoError(t, err)

	queueSize := 10
	collector := &helper.LocalCollector{}
	input.Start(collector)
	t.Cleanup(func() {
		require.NoError(t, input.Stop())
	})

	client := &http.Client{}
	url := fmt.Sprintf("http://%s/v1/logs", endpointHTTP)

	for i := 0; i < queueSize; i++ {
		req := plogotlp.NewExportRequestFromLogs(GenerateLogs(i + 1))
		err = httpExport(client, req, url, i%2 == 0)
		assert.NoError(t, err)
	}

	count := 0
	pos := 0
	for count < queueSize {
		count++
		for i := pos; i < pos+count; i++ {
			log := collector.Logs[i]
			assert.Equal(t, "time_unix_nano", log.Contents[0].Key)
			assert.Equal(t, "severity_number", log.Contents[1].Key)
			assert.Equal(t, "severity_text", log.Contents[2].Key)
			assert.Equal(t, "content", log.Contents[3].Key)
			assert.Equal(t, fmt.Sprintf("log body %d", i-pos), log.Contents[3].Value)
			assert.Equal(t, "resources", log.Contents[4].Key)
			assert.Equal(t, "{\"resource-attr\":\"resource-attr-val-1\"}", log.Contents[4].Value)
		}
		pos += count
	}
}

func TestOtlpHTTP_Metrics_V1(t *testing.T) {
	endpointHTTP := test.GetAvailableLocalAddress(t)
	input, err := newInput(false, true, "", endpointHTTP)
	assert.NoError(t, err)

	queueSize := 10
	collector := &helper.LocalCollector{}
	input.Start(collector)
	t.Cleanup(func() {
		require.NoError(t, input.Stop())
	})

	client := &http.Client{}
	url := fmt.Sprintf("http://%s/v1/metrics", endpointHTTP)

	for i := 0; i < queueSize; i++ {
		req := pmetricotlp.NewExportRequestFromMetrics(GenerateMetrics(i + 1))
		err = httpExport(client, req, url, i%2 == 0)
		assert.NoError(t, err)
	}

	types := make(map[string]int, 0)
	for _, log := range collector.Logs {
		assert.Equal(t, "__name__", log.Contents[0].Key)
		assert.Equal(t, "__labels__", log.Contents[2].Key)
		types[log.Contents[0].Value]++
	}
	assert.Equal(t, 26, types["gauge_int"])
	assert.Equal(t, 22, types["gauge_double"])
	assert.Equal(t, 18, types["sum_int"])
	assert.Equal(t, 14, types["sum_double"])
	assert.Equal(t, 8, types["summary_count"])
	assert.Equal(t, 8, types["summary_sum"])
	assert.Equal(t, types["summary_count"]/2, types["summary"])
	assert.Equal(t, 6, types["histogram_max"])
	assert.Equal(t, 6, types["histogram_min"])
	assert.Equal(t, 6, types["histogram_exemplars"])
	assert.Equal(t, types["histogram_max"]*2, types["histogram_bucket"])
	assert.Equal(t, types["histogram_max"]*2, types["histogram_count"])
	assert.Equal(t, types["histogram_max"]*2, types["histogram_sum"])
	assert.Equal(t, 5, types["exponential_histogram_max"])
	assert.Equal(t, 5, types["exponential_histogram_min"])
	assert.Equal(t, 5, types["exponential_histogram_exemplars"])
	assert.Equal(t, types["exponential_histogram_max"]*(5+7), types["exponential_histogram_bucket"])
	assert.Equal(t, types["exponential_histogram_max"]*2, types["exponential_histogram_count"])
	assert.Equal(t, types["exponential_histogram_max"]*2, types["exponential_histogram_sum"])
}

func TestOtlpHTTP_Trace_V1(t *testing.T) {
	endpointHTTP := test.GetAvailableLocalAddress(t)
	input, err := newInput(false, true, "", endpointHTTP)
	assert.NoError(t, err)

	queueSize := 10
	collector := &helper.LocalCollector{}
	input.Start(collector)
	t.Cleanup(func() {
		require.NoError(t, input.Stop())
	})

	client := &http.Client{}
	url := fmt.Sprintf("http://%s/v1/traces", endpointHTTP)

	for i := 0; i < queueSize; i++ {
		req := ptraceotlp.NewExportRequestFromTraces(GenerateTraces(i + 1))
		err = httpExport(client, req, url, i%2 == 0)
		assert.NoError(t, err)
	}

	starttm := time.Date(2020, 2, 11, 20, 26, 12, 321, time.UTC)
	endtm := time.Date(2020, 2, 11, 20, 26, 13, 789, time.UTC)
	traceID := "0102030405060708090a0b0c0d0e0f10"
	spanID := "1112131415161718"
	count := 0
	pos := 0
	for count < queueSize {
		count++
		for i := pos; i < pos+count; i++ {
			log := collector.Logs[i]
			assert.Equal(t, int64(log.Time), endtm.Unix())
			assert.Equal(t, "host", log.Contents[0].Key)
			assert.Equal(t, "service", log.Contents[1].Key)
			assert.Equal(t, "resource", log.Contents[2].Key)
			assert.Equal(t, "{\"resource-attr\":\"resource-attr-val-1\"}", log.Contents[2].Value)
			assert.Equal(t, "otlp.name", log.Contents[3].Key)
			assert.Equal(t, "otlp.version", log.Contents[4].Key)
			assert.Equal(t, "traceID", log.Contents[5].Key)
			assert.Equal(t, "spanID", log.Contents[6].Key)
			assert.Equal(t, "parentSpanID", log.Contents[7].Key)
			assert.Equal(t, "kind", log.Contents[8].Key)
			assert.Equal(t, "name", log.Contents[9].Key)
			assert.Equal(t, "links", log.Contents[10].Key)
			assert.Equal(t, "logs", log.Contents[11].Key)
			if (i-pos)%2 == 0 {
				assert.Equal(t, traceID, log.Contents[5].Value)
				assert.Equal(t, spanID, log.Contents[6].Value)
				assert.Equal(t, "operationA", log.Contents[9].Value)
				assert.Equal(t, "[{\"attribute\":{\"span-event-attr\":\"span-event-attr-val\"},\"name\":\"event-with-attr\",\"time\":1581452773000000123},{\"attribute\":{},\"name\":\"event\",\"time\":1581452773000000123}]", log.Contents[11].Value)
			} else {
				assert.Equal(t, []byte{}, []byte(log.Contents[5].Value))
				assert.Equal(t, []byte{}, []byte(log.Contents[6].Value))
				assert.Equal(t, "operationB", log.Contents[9].Value)
				assert.Equal(t, "[{\"attribute\":{\"span-link-attr\":\"span-link-attr-val\"},\"spanID\":\"\",\"traceID\":\"\"},{\"attribute\":{},\"spanID\":\"\",\"traceID\":\"\"}]", log.Contents[10].Value)
			}
			assert.Equal(t, "traceState", log.Contents[12].Key)
			assert.Equal(t, "start", log.Contents[13].Key)
			assert.Equal(t, strconv.FormatInt(starttm.UnixMicro(), 10), log.Contents[13].Value)
			assert.Equal(t, "end", log.Contents[14].Key)
			assert.Equal(t, strconv.FormatInt(endtm.UnixMicro(), 10), log.Contents[14].Value)
			assert.Equal(t, "duration", log.Contents[15].Key)
			assert.Equal(t, "attribute", log.Contents[16].Key)
			assert.Equal(t, "statusCode", log.Contents[17].Key)
			assert.Equal(t, "statusMessage", log.Contents[18].Key)
		}
		pos += count
	}
}

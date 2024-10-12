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
	"testing"

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
		assert.Error(t, err, "does_not_support_otlptraces")
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
		assert.Error(t, err, "does_not_support_otlptraces")
	}
}

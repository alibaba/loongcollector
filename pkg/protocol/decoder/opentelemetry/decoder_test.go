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

package opentelemetry

import (
	"encoding/json"
	"fmt"
	"math"
	"net/http"
	"strconv"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.opentelemetry.io/collector/pdata/plog"
	"go.opentelemetry.io/collector/pdata/plog/plogotlp"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.opentelemetry.io/collector/pdata/ptrace"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/common"
	"github.com/alibaba/ilogtail/pkg/protocol/otlp"
)

var textFormat = `{"resourceLogs":[{"resource":{"attributes":[{"key":"service.name","value":{"stringValue":"OtlpExporterExample"}},{"key":"telemetry.sdk.language","value":{"stringValue":"java"}},{"key":"telemetry.sdk.name","value":{"stringValue":"opentelemetry"}},{"key":"telemetry.sdk.version","value":{"stringValue":"1.18.0"}}]},"scopeLogs":[{"scope":{"name":"io.opentelemetry.example"},"logRecords":[{"timeUnixNano":"1663904182348000000","severityNumber":9,"severityText":"INFO","body":{"stringValue":"log body1"},"attributes":[{"key":"k1","value":{"stringValue":"v1"}},{"key":"k2","value":{"stringValue":"v2"}}],"traceId":"","spanId":""},{"timeUnixNano":"1663904182348000000","severityNumber":9,"severityText":"INFO","body":{"stringValue":"log body2"},"attributes":[{"key":"k1","value":{"stringValue":"v1"}},{"key":"k2","value":{"stringValue":"v2"}}],"traceId":"","spanId":""}]}]}]}`

func TestNormal(t *testing.T) {
	httpReq, _ := http.NewRequest("Post", "", nil)
	httpReq.Header.Set("Content-Type", jsonContentType)
	decoder := &Decoder{Format: common.ProtocolOTLPLogV1}

	logs, err := decoder.Decode([]byte(textFormat), httpReq, nil)
	assert.Nil(t, err)
	assert.Equal(t, len(logs), 2)
	log := logs[1]
	assert.Equal(t, int(log.Time), 1663904182)
	for _, cont := range log.Contents {
		if cont.Key == "attributes" {
			assert.NotEmpty(t, cont.Value)
		} else if cont.Key == "resources" {
			assert.NotEmpty(t, cont.Value)
		}
	}
	data, _ := json.Marshal(logs)
	fmt.Printf("%s\n", string(data))
	data = []byte("")
	fmt.Printf("empty = [%s]\n", string(data))
}

func TestConvertOtlpLogV1(t *testing.T) {
	logs := plog.NewLogs()
	resourceLogs := logs.ResourceLogs()

	resourceLog := resourceLogs.AppendEmpty()
	resource := resourceLog.Resource()
	resource.Attributes().PutStr("serviceName", "test-service")

	scopeLog := resourceLog.ScopeLogs().AppendEmpty()
	scopeLog.Scope().Attributes().PutStr("scope_key1", "scope_value1")

	now := pcommon.NewTimestampFromTime(time.Now())

	logRecord := scopeLog.LogRecords().AppendEmpty()
	logRecord.SetTimestamp(now)
	logRecord.Body().SetStr("test-message")
	logRecord.Attributes().PutInt("attr1", 123)
	logRecord.Attributes().PutBool("attr2", true)

	otlpLogReq := plogotlp.NewExportRequestFromLogs(logs)

	result, err := ConvertOtlpLogV1(otlpLogReq.Logs())
	if err != nil {
		t.Errorf("Error: %v", err)
		return
	}

	// Check if the result is not nil
	assert.NotNil(t, result)

	// Check if the number of logs is correct
	assert.Equal(t, 1, len(result))

	// Check if the contents of the log are correct
	log1 := result[0]
	assert.Equal(t, uint32(now/1e9), log1.Time)

	assert.Equal(t, "time_unix_nano", log1.Contents[0].Key)
	assert.Equal(t, strconv.FormatInt(int64(now), 10), log1.Contents[0].Value)

	assert.Equal(t, "severity_number", log1.Contents[1].Key)
	assert.Equal(t, "0", log1.Contents[1].Value)

	assert.Equal(t, "severity_text", log1.Contents[2].Key)
	assert.Equal(t, "", log1.Contents[2].Value)

	assert.Equal(t, "content", log1.Contents[3].Key)
	assert.Equal(t, "test-message", log1.Contents[3].Value)

	// Check if the attribute is correct
	assert.Equal(t, "attributes", log1.Contents[4].Key)

	expectedAttr := make(map[string]interface{})
	expectedAttr["attr1"] = float64(123)
	expectedAttr["attr2"] = true

	expectedAttrBytes, err := json.Marshal(expectedAttr)
	if err != nil {
		t.Errorf("Error: %v", err)
		return
	}
	assert.Equal(t, string(expectedAttrBytes), log1.Contents[4].Value)

	// Check if the resources is correct
	assert.Equal(t, "resources", log1.Contents[5].Key)

	expectedRes := make(map[string]interface{})
	expectedRes["serviceName"] = "test-service"

	expectedResBytes, err := json.Marshal(expectedRes)
	if err != nil {
		t.Errorf("Error: %v", err)
		return
	}
	assert.Equal(t, string(expectedResBytes), log1.Contents[5].Value)
}

func TestDecoder_Decode_Logs(t *testing.T) {
	// complcated case
	encoder := &plog.JSONMarshaler{}
	jsonBuf, err := encoder.MarshalLogs(logsOTLP)
	assert.NoError(t, err)
	httpReq, _ := http.NewRequest("Post", "", nil)
	httpReq.Header.Set("Content-Type", jsonContentType)
	decoder := &Decoder{Format: common.ProtocolOTLPLogV1}

	logs, err := decoder.Decode(jsonBuf, httpReq, nil)
	assert.NoError(t, err)
	assert.Equal(t, 1, len(logs))

	for _, log := range logs {
		assert.Equal(t, 6, len(log.Contents))
		assert.Equal(t, "time_unix_nano", log.Contents[0].Key)

		assert.Equal(t, "severity_number", log.Contents[1].Key)
		assert.Equal(t, "17", log.Contents[1].Value)

		assert.Equal(t, "severity_text", log.Contents[2].Key)
		assert.Equal(t, "Error", log.Contents[2].Value)

		assert.Equal(t, "content", log.Contents[3].Key)
		assert.Equal(t, "hello world", log.Contents[3].Value)

		assert.Equal(t, "attributes", log.Contents[4].Key)
		expectedAttr := make(map[string]interface{})
		expectedAttr["sdkVersion"] = "1.0.1"
		expectedAttrBytes, err := json.Marshal(expectedAttr)
		if err != nil {
			t.Errorf("Error: %v", err)
			return
		}
		assert.Equal(t, string(expectedAttrBytes), log.Contents[4].Value)

		assert.Equal(t, "resources", log.Contents[5].Key)
		expectedRes := make(map[string]interface{})
		expectedRes["host.name"] = "testHost"
		expectedResBytes, err := json.Marshal(expectedRes)
		if err != nil {
			t.Errorf("Error: %v", err)
			return
		}
		assert.Equal(t, string(expectedResBytes), log.Contents[5].Value)
	}
}

func TestDecoder_Decode_MetricsUntyped(t *testing.T) {
	var metricsJSON = `{"resourceMetrics":[{"resource":{"attributes":[{"key":"host.name","value":{"stringValue":"testHost"}}]},"scopeMetrics":[{"scope":{"name":"name","version":"version"},"metrics":[{"name":"testMetric"}]}]}]}`
	httpReq, _ := http.NewRequest("Post", "", nil)
	httpReq.Header.Set("Content-Type", jsonContentType)
	decoder := &Decoder{Format: common.ProtocolOTLPMetricV1}

	logs, err := decoder.Decode([]byte(metricsJSON), httpReq, nil)
	assert.Nil(t, err)
	assert.Equal(t, 1, len(logs))

	for _, log := range logs {
		assert.Equal(t, "__name__", log.Contents[0].Key)
		assert.Equal(t, "testMetric", log.Contents[0].Value)
		assert.Equal(t, "__labels__", log.Contents[1].Key)
		assert.Equal(t, "Empty", log.Contents[1].Value)
		assert.Equal(t, "__time_nano__", log.Contents[2].Key)
		assert.Equal(t, "__value__", log.Contents[3].Key)
		assert.Equal(t, "", log.Contents[3].Value)
	}
}

func TestDecoder_Decode_MetricsAll(t *testing.T) {
	config.LoongcollectorGlobalConfig.EnableSlsMetricsFormat = true
	type args struct {
		md func() pmetric.Metrics
	}

	tests := []struct {
		name string
		args args
	}{
		{
			name: "sum",
			args: args{
				md: metricsSumOTLPFull,
			}},
		{
			name: "gauge",
			args: args{
				md: metricsGaugeOTLPFull,
			},
		},
		{
			name: "Histogram",
			args: args{
				md: metricsHistogramOTLPFull,
			},
		},
		{
			name: "ExponentialHistogram",
			args: args{
				md: metricsExponentialHistogramOTLPFull,
			},
		},
		{
			name: "Summary",
			args: args{
				md: metricsSummaryOTLPFull,
			},
		},
	}

	encoder := &pmetric.JSONMarshaler{}
	httpReq, _ := http.NewRequest("Post", "", nil)
	httpReq.Header.Set("Content-Type", jsonContentType)
	decoder := &Decoder{Format: common.ProtocolOTLPMetricV1}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			m := tt.args.md()
			jsonBuf, err := encoder.MarshalMetrics(m)
			assert.NoError(t, err)
			logs, err := decoder.Decode(jsonBuf, httpReq, nil)
			assert.NoError(t, err)

			expectedLogsLen := func() int {
				count := 0
				for i := 0; i < m.ResourceMetrics().Len(); i++ {
					resourceMetric := m.ResourceMetrics().At(i)
					for j := 0; j < resourceMetric.ScopeMetrics().Len(); j++ {
						metrics := resourceMetric.ScopeMetrics().At(j).Metrics()
						for k := 0; k < metrics.Len(); k++ {
							metric := metrics.At(k)
							switch metric.Type() {
							case pmetric.MetricTypeGauge:
								count += metric.Gauge().DataPoints().Len()
								for i := 0; i < metric.Gauge().DataPoints().Len(); i++ {
									count += metric.Gauge().DataPoints().At(i).Exemplars().Len()
								}
							case pmetric.MetricTypeSum:
								count += metric.Sum().DataPoints().Len()
								for i := 0; i < metric.Sum().DataPoints().Len(); i++ {
									count += metric.Sum().DataPoints().At(i).Exemplars().Len()
								}
							case pmetric.MetricTypeSummary:
								for l := 0; l < metric.Summary().DataPoints().Len(); l++ {
									dataPoint := metric.Summary().DataPoints().At(l)
									count += 2
									count += dataPoint.QuantileValues().Len()
								}
							case pmetric.MetricTypeHistogram:
								for l := 0; l < metric.Histogram().DataPoints().Len(); l++ {
									dataPoint := metric.Histogram().DataPoints().At(l)
									count += dataPoint.Exemplars().Len()
									if dataPoint.HasSum() {
										count++
									}
									if dataPoint.HasMin() {
										count++
									}
									if dataPoint.HasMax() {
										count++
									}
									count += dataPoint.BucketCounts().Len() + 1
								}
							case pmetric.MetricTypeExponentialHistogram:
								for l := 0; l < metric.ExponentialHistogram().DataPoints().Len(); l++ {
									dataPoint := metric.ExponentialHistogram().DataPoints().At(l)
									count += dataPoint.Exemplars().Len()
									if dataPoint.HasSum() {
										count++
									}
									if dataPoint.HasMin() {
										count++
									}
									if dataPoint.HasMax() {
										count++
									}
									count += dataPoint.Negative().BucketCounts().Len() + dataPoint.Positive().BucketCounts().Len() + 4
								}
							}
						}
					}
				}
				return count
			}()
			assert.Equal(t, expectedLogsLen, len(logs))

			metric := m.ResourceMetrics().At(0).ScopeMetrics().At(0).Metrics().At(0)
			switch metric.Type() {
			case pmetric.MetricTypeGauge:
				assert.Equal(t, "__name__", logs[0].Contents[0].Key)
				assert.Equal(t, "test_gauge_exemplars", logs[0].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[0].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|service_name#$#testService|service_name#$#testService|spanId#$#1112131415161718|string#$#value|traceId#$#0102030405060708090a0b0c0d0e0f10", logs[0].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[0].Contents[1].Key)
				assert.Equal(t, "__value__", logs[0].Contents[3].Key)
				assert.Equal(t, "99.3", logs[0].Contents[3].Value)

				assert.Equal(t, "__name__", logs[1].Contents[0].Key)
				assert.Equal(t, "test_gauge", logs[1].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[1].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|service_name#$#testService|string#$#value", logs[1].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[1].Contents[1].Key)
				assert.Equal(t, "__value__", logs[1].Contents[3].Key)
				assert.Equal(t, "10.2", logs[1].Contents[3].Value)
			case pmetric.MetricTypeSum:
				assert.Equal(t, "__name__", logs[0].Contents[0].Key)
				assert.Equal(t, "test_sum_exemplars", logs[0].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[0].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|otlp_metric_aggregation_temporality#$#Cumulative|otlp_metric_ismonotonic#$#true|service_name#$#testService|service_name#$#testService|spanId#$#1112131415161718|string#$#value|traceId#$#0102030405060708090a0b0c0d0e0f10", logs[0].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[0].Contents[1].Key)
				assert.Equal(t, "__value__", logs[0].Contents[3].Key)
				assert.Equal(t, "99.3", logs[0].Contents[3].Value)

				assert.Equal(t, "__name__", logs[1].Contents[0].Key)
				assert.Equal(t, "test_sum", logs[1].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[1].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|otlp_metric_aggregation_temporality#$#Cumulative|otlp_metric_ismonotonic#$#true|service_name#$#testService|string#$#value", logs[1].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[1].Contents[1].Key)
				assert.Equal(t, "__value__", logs[1].Contents[3].Key)
				assert.Equal(t, "100", logs[1].Contents[3].Value)

				assert.Equal(t, "__name__", logs[2].Contents[0].Key)
				assert.Equal(t, "test_sum", logs[2].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[2].Contents[2].Key)
				assert.Equal(t, "bool#$#false|bytes#$#YmFy|double#$#2.2|host_name#$#testHost|int#$#2|otlp_metric_aggregation_temporality#$#Cumulative|otlp_metric_ismonotonic#$#true|service_name#$#testService|string#$#value2", logs[2].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[2].Contents[1].Key)
				assert.Equal(t, "__value__", logs[2].Contents[3].Key)
				assert.Equal(t, "50", logs[2].Contents[3].Value)
			case pmetric.MetricTypeSummary:
				assert.Equal(t, "__name__", logs[0].Contents[0].Key)
				assert.Equal(t, "test_summary_sum", logs[0].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[0].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|service_name#$#testService|string#$#value", logs[0].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[0].Contents[1].Key)
				assert.Equal(t, "__value__", logs[0].Contents[3].Key)
				assert.Equal(t, "1000", logs[0].Contents[3].Value)

				assert.Equal(t, "__name__", logs[1].Contents[0].Key)
				assert.Equal(t, "test_summary_count", logs[1].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[1].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|service_name#$#testService|string#$#value", logs[1].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[1].Contents[1].Key)
				assert.Equal(t, "__value__", logs[1].Contents[3].Key)
				assert.Equal(t, "100", logs[1].Contents[3].Value)

				assert.Equal(t, "__name__", logs[2].Contents[0].Key)
				assert.Equal(t, "test_summary", logs[2].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[2].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|quantile#$#0.5|service_name#$#testService|string#$#value", logs[2].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[2].Contents[1].Key)
				assert.Equal(t, "__value__", logs[2].Contents[3].Key)
				assert.Equal(t, "1.2", logs[2].Contents[3].Value)
			case pmetric.MetricTypeHistogram:
				assert.Equal(t, "__name__", logs[0].Contents[0].Key)
				assert.Equal(t, "test_Histogram_sum", logs[0].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[0].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|otlp_metric_aggregation_temporality#$#Cumulative|otlp_metric_histogram_type#$#Histogram|service_name#$#testService|string#$#value", logs[0].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[0].Contents[1].Key)
				assert.Equal(t, "__value__", logs[0].Contents[3].Key)
				assert.Equal(t, fmt.Sprint(metric.Histogram().DataPoints().At(0).Sum()), logs[0].Contents[3].Value)

				assert.Equal(t, "__name__", logs[1].Contents[0].Key)
				assert.Equal(t, "test_Histogram_min", logs[1].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[1].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|otlp_metric_aggregation_temporality#$#Cumulative|otlp_metric_histogram_type#$#Histogram|service_name#$#testService|string#$#value", logs[1].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[1].Contents[1].Key)
				assert.Equal(t, "__value__", logs[1].Contents[3].Key)
				assert.Equal(t, fmt.Sprint(metric.Histogram().DataPoints().At(0).Min()), logs[1].Contents[3].Value)

				assert.Equal(t, "__name__", logs[2].Contents[0].Key)
				assert.Equal(t, "test_Histogram_max", logs[2].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[2].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|otlp_metric_aggregation_temporality#$#Cumulative|otlp_metric_histogram_type#$#Histogram|service_name#$#testService|string#$#value", logs[2].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[2].Contents[1].Key)
				assert.Equal(t, "__value__", logs[2].Contents[3].Key)
				assert.Equal(t, fmt.Sprint(metric.Histogram().DataPoints().At(0).Max()), logs[2].Contents[3].Value)

				assert.Equal(t, "__name__", logs[3].Contents[0].Key)
				assert.Equal(t, "test_Histogram_count", logs[3].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[3].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|otlp_metric_aggregation_temporality#$#Cumulative|otlp_metric_histogram_type#$#Histogram|service_name#$#testService|string#$#value", logs[3].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[3].Contents[1].Key)
				assert.Equal(t, "__value__", logs[3].Contents[3].Key)
				assert.Equal(t, fmt.Sprint(metric.Histogram().DataPoints().At(0).Count()), logs[3].Contents[3].Value)

				assert.Equal(t, "__name__", logs[4].Contents[0].Key)
				assert.Equal(t, "test_Histogram_exemplars", logs[4].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[4].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|otlp_metric_aggregation_temporality#$#Cumulative|otlp_metric_histogram_type#$#Histogram|service_name#$#testService|service_name#$#testService|spanId#$#1112131415161718|string#$#value|traceId#$#0102030405060708090a0b0c0d0e0f10", logs[4].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[4].Contents[1].Key)
				assert.Equal(t, "__value__", logs[4].Contents[3].Key)
				assert.Equal(t, "99.3", logs[4].Contents[3].Value)

				assert.Equal(t, "__name__", logs[5].Contents[0].Key)
				assert.Equal(t, "test_Histogram_bucket", logs[5].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[5].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|le#$#10|otlp_metric_aggregation_temporality#$#Cumulative|otlp_metric_histogram_type#$#Histogram|service_name#$#testService|string#$#value", logs[5].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[5].Contents[1].Key)
				assert.Equal(t, "__value__", logs[5].Contents[3].Key)
				assert.Equal(t, "1", logs[5].Contents[3].Value)

				assert.Equal(t, "__name__", logs[6].Contents[0].Key)
				assert.Equal(t, "test_Histogram_bucket", logs[6].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[6].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|le#$#100|otlp_metric_aggregation_temporality#$#Cumulative|otlp_metric_histogram_type#$#Histogram|service_name#$#testService|string#$#value", logs[6].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[6].Contents[1].Key)
				assert.Equal(t, "__value__", logs[6].Contents[3].Key)
				assert.Equal(t, "2", logs[6].Contents[3].Value)

				assert.Equal(t, "__name__", logs[7].Contents[0].Key)
				assert.Equal(t, "test_Histogram_bucket", logs[7].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[7].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|le#$#+Inf|otlp_metric_aggregation_temporality#$#Cumulative|otlp_metric_histogram_type#$#Histogram|service_name#$#testService|string#$#value", logs[7].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[7].Contents[1].Key)
				assert.Equal(t, "__value__", logs[7].Contents[3].Key)
				assert.Equal(t, "4", logs[7].Contents[3].Value)
			case pmetric.MetricTypeExponentialHistogram:
				assert.Equal(t, "__name__", logs[0].Contents[0].Key)
				assert.Equal(t, "test_ExponentialHistogram_sum", logs[0].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[0].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|otlp_metric_aggregation_temporality#$#Cumulative|otlp_metric_histogram_type#$#ExponentialHistogram|service_name#$#testService|string#$#value", logs[0].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[0].Contents[1].Key)
				assert.Equal(t, "__value__", logs[0].Contents[3].Key)
				assert.Equal(t, fmt.Sprint(metric.ExponentialHistogram().DataPoints().At(0).Sum()), logs[0].Contents[3].Value)

				assert.Equal(t, "__name__", logs[1].Contents[0].Key)
				assert.Equal(t, "test_ExponentialHistogram_min", logs[1].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[1].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|otlp_metric_aggregation_temporality#$#Cumulative|otlp_metric_histogram_type#$#ExponentialHistogram|service_name#$#testService|string#$#value", logs[1].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[1].Contents[1].Key)
				assert.Equal(t, "__value__", logs[1].Contents[3].Key)
				assert.Equal(t, fmt.Sprint(metric.ExponentialHistogram().DataPoints().At(0).Min()), logs[1].Contents[3].Value)

				assert.Equal(t, "__name__", logs[2].Contents[0].Key)
				assert.Equal(t, "test_ExponentialHistogram_max", logs[2].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[2].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|otlp_metric_aggregation_temporality#$#Cumulative|otlp_metric_histogram_type#$#ExponentialHistogram|service_name#$#testService|string#$#value", logs[2].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[2].Contents[1].Key)
				assert.Equal(t, "__value__", logs[2].Contents[3].Key)
				assert.Equal(t, fmt.Sprint(metric.ExponentialHistogram().DataPoints().At(0).Max()), logs[2].Contents[3].Value)

				assert.Equal(t, "__name__", logs[3].Contents[0].Key)
				assert.Equal(t, "test_ExponentialHistogram_count", logs[3].Contents[0].Value)
				assert.Equal(t, "__labels__", logs[3].Contents[2].Key)
				assert.Equal(t, "bool#$#true|bytes#$#Zm9v|double#$#1.1|host_name#$#testHost|int#$#1|otlp_metric_aggregation_temporality#$#Cumulative|otlp_metric_histogram_type#$#ExponentialHistogram|service_name#$#testService|string#$#value", logs[3].Contents[2].Value)
				assert.Equal(t, "__time_nano__", logs[3].Contents[1].Key)
				assert.Equal(t, "__value__", logs[3].Contents[3].Key)
				assert.Equal(t, fmt.Sprint(metric.ExponentialHistogram().DataPoints().At(0).Count()), logs[3].Contents[3].Value)
			}
		})
	}
}

func TestConvertOtlpLogsToGroupEvents(t *testing.T) {
	plogs := plog.NewLogs()
	rsLogs := plogs.ResourceLogs().AppendEmpty()
	rsLogs.Resource().Attributes().PutStr("meta_attr1", "attr_value1")
	rsLogs.Resource().Attributes().PutStr("meta_attr2", "attr_value2")
	scopeLog := rsLogs.ScopeLogs().AppendEmpty()
	scopeLog.Scope().Attributes().PutStr("scope_key1", "scope_value1")
	scopeLog.Scope().Attributes().PutStr(otlp.TagKeyScopeVersion, "")        // skip
	scopeLog.Scope().Attributes().PutStr(otlp.TagKeyScopeName, "scope_name") // keep

	logRecord := scopeLog.LogRecords().AppendEmpty()
	logRecord.Body().SetStr("some log message")
	logRecord.SetSeverityNumber(plog.SeverityNumberInfo)
	logRecord.SetTimestamp(1234567890)
	logRecord.Attributes().PutStr("process", "process_name")
	logRecord.SetTraceID(pcommon.TraceID([16]byte{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}))
	logRecord.SetSpanID(pcommon.SpanID([8]byte{1, 2, 3, 4, 5, 6, 7, 8}))

	groupEventsSlice, err := ConvertOtlpLogsToGroupEvents(plogs)

	// Check the results
	assert.Nil(t, err)
	assert.NotNil(t, groupEventsSlice)
	assert.Equal(t, 1, len(groupEventsSlice))
	group := groupEventsSlice[0].Group
	assert.NotNil(t, group)
	assert.Equal(t, 2, group.Metadata.Len())
	assert.Equal(t, "attr_value1", group.Metadata.Get("meta_attr1"))
	assert.Equal(t, "attr_value2", group.Metadata.Get("meta_attr2"))

	assert.Equal(t, 1+2, group.Tags.Len())
	assert.Equal(t, "scope_value1", group.Tags.Get("scope_key1"))

	events := groupEventsSlice[0].Events
	assert.Equal(t, 1, len(events))
	actualLog := events[0].(*models.Log)
	assert.Equal(t, models.EventTypeLogging, events[0].GetType())
	assert.Equal(t, []byte("some log message"), actualLog.GetBody())
	assert.Equal(t, "Info", actualLog.Level)
	assert.Equal(t, "0102030405060708", actualLog.SpanID)
	assert.Equal(t, "0102030405060708090a0b0c0d0e0f10", actualLog.TraceID)
	assert.Equal(t, uint64(1234567890), actualLog.Timestamp)
	assert.Equal(t, "process_name", actualLog.Tags.Get("process"))
}

func TestDecoder_DecodeV2_Logs(t *testing.T) {
	// complcated case
	encoder := &plog.JSONMarshaler{}
	jsonBuf, err := encoder.MarshalLogs(logsOTLP)
	assert.NoError(t, err)
	httpReq, _ := http.NewRequest("Post", "", nil)
	httpReq.Header.Set("Content-Type", jsonContentType)
	decoder := &Decoder{Format: common.ProtocolOTLPLogV1}

	pipelineGrouptEventsSlice, err := decoder.DecodeV2(jsonBuf, httpReq)
	assert.NoError(t, err)
	assert.Equal(t, 1, len(pipelineGrouptEventsSlice))
	otlpResLogs := logsOTLP.ResourceLogs().At(0)

	for i, groupEvents := range pipelineGrouptEventsSlice {
		resource := groupEvents.Group.Metadata
		assert.Equal(t, "testHost", resource.Get("host.name"))

		scopeAttributes := groupEvents.Group.Tags
		assert.False(t, scopeAttributes.Contains(otlp.TagKeyScopeVersion))
		assert.Equal(t, "name", scopeAttributes.Get(otlp.TagKeyScopeName))

		otlpLogs := otlpResLogs.ScopeLogs().At(i).LogRecords()
		assert.Equal(t, 1, len(groupEvents.Events))

		for j, event := range groupEvents.Events {
			otlpLog := otlpLogs.At(j)
			name := event.GetName()
			assert.Equal(t, logEventName, name)

			eventType := event.GetType()
			assert.Equal(t, models.EventTypeLogging, eventType)
			log, ok := event.(*models.Log)
			assert.True(t, ok)
			assert.Equal(t, otlpLog.TraceID().String(), log.TraceID)
			assert.Equal(t, otlpLog.SpanID().String(), log.SpanID)

			otlpLog.Attributes().Range(
				func(k string, v pcommon.Value) bool {
					assert.True(t, log.Tags.Contains(k))
					return true
				},
			)
		}
	}
}

func TestDecoder_DecodeV2_MetricsUntyped(t *testing.T) {
	var metricsJSON = `{"resourceMetrics":[{"resource":{"attributes":[{"key":"host.name","value":{"stringValue":"testHost"}}]},"scopeMetrics":[{"scope":{"name":"name","version":"version"},"metrics":[{"name":"testMetric"}]}]}]}`
	httpReq, _ := http.NewRequest("Post", "", nil)
	httpReq.Header.Set("Content-Type", jsonContentType)
	decoder := &Decoder{Format: common.ProtocolOTLPMetricV1}

	pipelineGrouptEventsSlice, err := decoder.DecodeV2([]byte(metricsJSON), httpReq)
	assert.Nil(t, err)
	assert.Equal(t, 1, len(pipelineGrouptEventsSlice))

	for _, groupEvents := range pipelineGrouptEventsSlice {
		assert.Equal(t, 1, len(groupEvents.Events))
		for _, event := range groupEvents.Events {
			name := event.GetName()
			assert.Equal(t, "testMetric", name)
			eventType := event.GetType()
			assert.Equal(t, models.EventTypeMetric, eventType)
			metric, ok := event.(*models.Metric)
			assert.True(t, ok)
			assert.Equal(t, models.MetricTypeUntyped, metric.MetricType)
		}
	}
}

func TestDecoder_DecodeV2_MetricsAll(t *testing.T) {
	type args struct {
		md func() pmetric.Metrics
	}

	tests := []struct {
		name string
		args args
	}{
		{
			name: "sum",
			args: args{
				md: metricsSumOTLPFull,
			}},
		{
			name: "gauge",
			args: args{
				md: metricsGaugeOTLPFull,
			},
		},
		{
			name: "Histogram",
			args: args{
				md: metricsHistogramOTLPFull,
			},
		},
		{
			name: "ExponentialHistogram",
			args: args{
				md: metricsExponentialHistogramOTLPFull,
			},
		},
		{
			name: "Summary",
			args: args{
				md: metricsSummaryOTLPFull,
			},
		},
	}

	encoder := &pmetric.JSONMarshaler{}
	httpReq, _ := http.NewRequest("Post", "", nil)
	httpReq.Header.Set("Content-Type", jsonContentType)
	decoder := &Decoder{Format: common.ProtocolOTLPMetricV1}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			m := tt.args.md()
			jsonBuf, err := encoder.MarshalMetrics(m)
			assert.NoError(t, err)
			pipelineGrouptEventsSlice, err := decoder.DecodeV2(jsonBuf, httpReq)
			assert.NoError(t, err)

			expectedGroupEventsLen := func() int {
				count := 0
				for i := 0; i < m.ResourceMetrics().Len(); i++ {
					resourceMetric := m.ResourceMetrics().At(i)
					count += resourceMetric.ScopeMetrics().Len()
				}
				return count
			}()
			assert.Equal(t, expectedGroupEventsLen, len(pipelineGrouptEventsSlice))

			groupindex, eventIndex := 0, 0
			for i := 0; i < m.ResourceMetrics().Len(); i++ {
				resourceMetric := m.ResourceMetrics().At(i)
				for j := 0; j < resourceMetric.ScopeMetrics().Len(); j++ {
					scopeMetric := resourceMetric.ScopeMetrics().At(j)

					for m := 0; m < scopeMetric.Metrics().Len(); m++ {
						otMetric := scopeMetric.Metrics().At(m)
						otUnit := otMetric.Unit()
						otDescription := otMetric.Description()

						switch otMetric.Type() {
						case pmetric.MetricTypeGauge:
							otGauge := otMetric.Gauge()
							otDatapoints := otGauge.DataPoints()
							for l := 0; l < otDatapoints.Len(); l++ {
								datapoint := otDatapoints.At(l)
								event := pipelineGrouptEventsSlice[groupindex].Events[eventIndex]
								// check tags
								datapoint.Attributes().Range(
									func(k string, v pcommon.Value) bool {
										assert.Equal(t, v.AsString(), event.GetTags().Get(k))
										return true
									})

								metric, ok := event.(*models.Metric)
								assert.True(t, ok)
								assert.Equal(t, models.MetricTypeGauge, metric.MetricType)
								assert.Equal(t, otUnit, metric.Unit)
								assert.Equal(t, otDescription, metric.Description)

								assert.Equal(t, otMetric.Name(), event.GetName())
								assert.Equal(t, uint64(datapoint.Timestamp()), event.GetTimestamp())
								otValue := float64(datapoint.IntValue())
								if datapoint.ValueType() == pmetric.NumberDataPointValueTypeDouble {
									otValue = datapoint.DoubleValue()
								}
								assert.Equal(t, otValue, metric.Value.GetSingleValue())

								eventIndex++
							}
						case pmetric.MetricTypeSum:
							otSum := otMetric.Sum()
							isMonotonic := strconv.FormatBool(otSum.IsMonotonic())
							aggregationTemporality := otSum.AggregationTemporality()
							otDatapoints := otSum.DataPoints()

							for l := 0; l < otDatapoints.Len(); l++ {
								datapoint := otDatapoints.At(l)
								event := pipelineGrouptEventsSlice[groupindex].Events[eventIndex]

								// check tags
								datapoint.Attributes().Range(
									func(k string, v pcommon.Value) bool {
										assert.Equal(t, v.AsString(), event.GetTags().Get(k))
										return true
									})
								assert.Equal(t, aggregationTemporality.String(), event.GetTags().Get(otlp.TagKeyMetricAggregationTemporality))
								assert.Equal(t, isMonotonic, event.GetTags().Get(otlp.TagKeyMetricIsMonotonic))

								metric, ok := event.(*models.Metric)
								assert.True(t, ok)
								if aggregationTemporality == pmetric.AggregationTemporalityCumulative {
									assert.Equal(t, models.MetricTypeCounter, metric.MetricType)
								} else {
									assert.Equal(t, models.MetricTypeRateCounter, metric.MetricType)
								}

								assert.Equal(t, otUnit, metric.Unit)
								assert.Equal(t, otDescription, metric.Description)

								// check values
								otValue := float64(datapoint.IntValue())
								if datapoint.ValueType() == pmetric.NumberDataPointValueTypeDouble {
									otValue = datapoint.DoubleValue()
								}
								assert.Equal(t, otValue, metric.Value.GetSingleValue())

								assert.Equal(t, otMetric.Name(), event.GetName())
								assert.Equal(t, uint64(datapoint.Timestamp()), event.GetTimestamp())
								eventIndex++
							}
						case pmetric.MetricTypeSummary:
							otSummary := otMetric.Summary()
							otDatapoints := otSummary.DataPoints()
							for l := 0; l < otDatapoints.Len(); l++ {
								datapoint := otDatapoints.At(l)
								event := pipelineGrouptEventsSlice[groupindex].Events[eventIndex]

								// check tags
								datapoint.Attributes().Range(
									func(k string, v pcommon.Value) bool {
										assert.Equal(t, v.AsString(), event.GetTags().Get(k))
										return true
									})

								metric, ok := event.(*models.Metric)
								assert.True(t, ok)
								// check type
								assert.Equal(t, models.MetricTypeSummary, metric.MetricType)
								assert.Equal(t, otUnit, metric.Unit)
								assert.Equal(t, otDescription, metric.Description)
								assert.Equal(t, otMetric.Name(), event.GetName())
								assert.Equal(t, uint64(datapoint.Timestamp()), event.GetTimestamp())

								// check values
								quantiles := datapoint.QuantileValues()
								for n := 0; n < quantiles.Len(); n++ {
									quantile := quantiles.At(n)
									assert.Equal(t, quantile.Value(), metric.Value.GetMultiValues().Get(strconv.FormatFloat(quantile.Quantile(), 'f', -1, 64)))
								}

								assert.Equal(t, float64(datapoint.Count()), metric.Value.GetMultiValues().Get(otlp.FieldCount))
								assert.Equal(t, datapoint.Sum(), metric.Value.GetMultiValues().Get(otlp.FieldSum))
								eventIndex++
							}
						case pmetric.MetricTypeHistogram:
							otHistogram := otMetric.Histogram()
							aggregationTemporality := otHistogram.AggregationTemporality()
							otDatapoints := otHistogram.DataPoints()

							for l := 0; l < otDatapoints.Len(); l++ {
								datapoint := otDatapoints.At(l)
								event := pipelineGrouptEventsSlice[groupindex].Events[eventIndex]

								// check tags
								datapoint.Attributes().Range(
									func(k string, v pcommon.Value) bool {
										assert.Equal(t, v.AsString(), event.GetTags().Get(k))
										return true
									})
								assert.Equal(t, aggregationTemporality.String(), event.GetTags().Get(otlp.TagKeyMetricAggregationTemporality))
								assert.Equal(t, pmetric.MetricTypeHistogram.String(), event.GetTags().Get(otlp.TagKeyMetricHistogramType))

								metric, ok := event.(*models.Metric)
								assert.True(t, ok)
								assert.Equal(t, models.MetricTypeHistogram, metric.MetricType)
								assert.Equal(t, otUnit, metric.Unit)
								assert.Equal(t, otDescription, metric.Description)

								assert.Equal(t, otMetric.Name(), event.GetName())
								assert.Equal(t, uint64(datapoint.Timestamp()), event.GetTimestamp())

								// check values
								otBucketCounts := datapoint.BucketCounts()
								otBucketBoundarys := datapoint.ExplicitBounds()
								assert.Equal(t, otBucketCounts.Len(), otBucketBoundarys.Len()+1)
								bucketBounds, bucketCounts := otlp.ComputeBuckets(metric.Value.GetMultiValues(), true)
								otBucketCountFloat := make([]float64, 0, len(otBucketCounts.AsRaw()))
								for _, v := range otBucketCounts.AsRaw() {
									otBucketCountFloat = append(otBucketCountFloat, float64(v))
								}
								assert.Equal(t, otBucketCountFloat, bucketCounts)
								assert.Equal(t, otBucketBoundarys.AsRaw(), bucketBounds[1:])

								assert.Equal(t, float64(datapoint.Count()), metric.Value.GetMultiValues().Get(otlp.FieldCount))
								assert.Equal(t, datapoint.Sum(), metric.Value.GetMultiValues().Get(otlp.FieldSum))
								eventIndex++
							}
						case pmetric.MetricTypeExponentialHistogram:
							otExponentialHistogram := otMetric.ExponentialHistogram()
							aggregationTemporality := otExponentialHistogram.AggregationTemporality()
							otDatapoints := otExponentialHistogram.DataPoints()

							for l := 0; l < otDatapoints.Len(); l++ {
								datapoint := otDatapoints.At(l)
								event := pipelineGrouptEventsSlice[groupindex].Events[eventIndex]

								datapoint.Attributes().Range(
									func(k string, v pcommon.Value) bool {
										assert.Equal(t, v.AsString(), event.GetTags().Get(k))
										return true
									})
								assert.Equal(t, aggregationTemporality.String(), event.GetTags().Get(otlp.TagKeyMetricAggregationTemporality))
								assert.Equal(t, pmetric.MetricTypeExponentialHistogram.String(), event.GetTags().Get(otlp.TagKeyMetricHistogramType))
								metric, ok := event.(*models.Metric)
								assert.True(t, ok)
								assert.Equal(t, otUnit, metric.Unit)
								assert.Equal(t, otDescription, metric.Description)
								assert.Equal(t, models.MetricTypeHistogram, metric.MetricType)
								assert.Equal(t, otMetric.Name(), event.GetName())
								assert.Equal(t, uint64(datapoint.Timestamp()), event.GetTimestamp())

								// check values
								scale := datapoint.Scale()
								base := math.Pow(2, math.Pow(2, float64(-scale)))
								assert.Equal(t, scale, int32(metric.Value.GetMultiValues().Get(otlp.FieldScale)))

								positiveOffset := datapoint.Positive().Offset()
								assert.Equal(t, positiveOffset, int32(metric.Value.GetMultiValues().Get(otlp.FieldPositiveOffset)))
								otPositiveBucketCounts := datapoint.Positive().BucketCounts()

								otPositiveBucketBounds, otPositiveBucketCountsFloat := make([]float64, 0), make([]float64, 0)
								for i, v := range otPositiveBucketCounts.AsRaw() {
									lowerBoundary := math.Pow(base, float64(int(positiveOffset)+i))
									otPositiveBucketBounds = append(otPositiveBucketBounds, lowerBoundary)
									otPositiveBucketCountsFloat = append(otPositiveBucketCountsFloat, float64(v))
								}
								postiveBucketBounds, positveBucketCounts := otlp.ComputeBuckets(metric.Value.GetMultiValues(), true)

								assert.Equal(t, otPositiveBucketBounds, postiveBucketBounds)
								assert.Equal(t, otPositiveBucketCountsFloat, positveBucketCounts)

								negativeOffset := datapoint.Negative().Offset()
								assert.Equal(t, negativeOffset, int32(metric.Value.GetMultiValues().Get(otlp.FieldNegativeOffset)))
								otNegativeBucetCounts := datapoint.Negative().BucketCounts()

								otNegativeBucketBounds, otNegativeBucketCountsFloat := make([]float64, 0), make([]float64, 0)

								for i, v := range otNegativeBucetCounts.AsRaw() {
									upperBoundary := -math.Pow(base, float64(int(negativeOffset)+i))
									otNegativeBucketBounds = append(otNegativeBucketBounds, upperBoundary)
									otNegativeBucketCountsFloat = append(otNegativeBucketCountsFloat, float64(v))
								}

								netativeBucketBounds, netativeBucketCounts := otlp.ComputeBuckets(metric.Value.GetMultiValues(), false)
								assert.Equal(t, otNegativeBucketBounds, netativeBucketBounds)
								assert.Equal(t, otNegativeBucketCountsFloat, netativeBucketCounts)

								assert.Equal(t, float64(datapoint.Count()), metric.Value.GetMultiValues().Get(otlp.FieldCount))
								assert.Equal(t, datapoint.Sum(), metric.Value.GetMultiValues().Get(otlp.FieldSum))
								assert.Equal(t, float64(datapoint.ZeroCount()), metric.Value.GetMultiValues().Get(otlp.FieldZeroCount))
								eventIndex++
							}
						}

					}

					assert.Equal(t, eventIndex, len(pipelineGrouptEventsSlice[groupindex].Events))
					eventIndex = 0
					groupindex++
				}

			}

		})
	}

}

func TestDecoder_DecodeV2_TracesEmpty(t *testing.T) {
	var tracesJSON = `{"resourceSpans":[{"resource":{"attributes":[{"key":"host.name","value":{"stringValue":"testHost"}}]},"scopeSpans":[{"scope":{"name":"name","version":"version"},"spans":[{"traceId":"","spanId":"","parentSpanId":"","name":"testSpan","status":{}}]}]}]}`
	httpReq, _ := http.NewRequest("Post", "", nil)
	httpReq.Header.Set("Content-Type", jsonContentType)
	decoder := &Decoder{Format: common.ProtocolOTLPTraceV1}

	pipelineGrouptEventsSlice, err := decoder.DecodeV2([]byte(tracesJSON), httpReq)
	assert.Nil(t, err)
	assert.Equal(t, 1, len(pipelineGrouptEventsSlice))

	for _, groupEvents := range pipelineGrouptEventsSlice {
		resource := groupEvents.Group.Metadata
		assert.True(t, resource.Contains("host.name"))
		scopeAttributes := groupEvents.Group.Tags
		assert.True(t, scopeAttributes.Contains(otlp.TagKeyScopeVersion))

		assert.Equal(t, 1, len(groupEvents.Events))
		for _, event := range groupEvents.Events {
			name := event.GetName()
			assert.Equal(t, "testSpan", name)
			eventType := event.GetType()
			assert.Equal(t, models.EventTypeSpan, eventType)
			span, ok := event.(*models.Span)
			assert.True(t, ok)
			fmt.Println(span.TraceID, span.SpanID)
			assert.Equal(t, "", span.TraceID)
			assert.Equal(t, "", span.SpanID)
		}
	}
}

func TestDecoder_DecodeV2_Traces(t *testing.T) {
	// complcated case
	encoder := &ptrace.JSONMarshaler{}
	jsonBuf, err := encoder.MarshalTraces(tracesOTLPFull)
	assert.NoError(t, err)
	httpReq, _ := http.NewRequest("Post", "", nil)
	httpReq.Header.Set("Content-Type", jsonContentType)
	decoder := &Decoder{Format: common.ProtocolOTLPTraceV1}

	pipelineGrouptEventsSlice, err := decoder.DecodeV2(jsonBuf, httpReq)
	assert.NoError(t, err)
	assert.Equal(t, 1, len(pipelineGrouptEventsSlice))
	otlpScopeSpans := tracesOTLPFull.ResourceSpans().At(0)

	for i, groupEvents := range pipelineGrouptEventsSlice {
		resource := groupEvents.Group.Metadata
		assert.Equal(t, "testHost", resource.Get("host.name"))
		assert.Equal(t, "testService", resource.Get("service.name"))

		scopeAttributes := groupEvents.Group.Tags
		assert.Equal(t, "scope version", scopeAttributes.Get(otlp.TagKeyScopeVersion))
		assert.Equal(t, "scope name", scopeAttributes.Get(otlp.TagKeyScopeName))

		otlpSpans := otlpScopeSpans.ScopeSpans().At(i).Spans()
		assert.Equal(t, 2, len(groupEvents.Events))

		for j, event := range groupEvents.Events {
			otlpSpan := otlpSpans.At(j)
			name := event.GetName()
			assert.Equal(t, otlpSpan.Name(), name)
			eventType := event.GetType()
			assert.Equal(t, models.EventTypeSpan, eventType)
			span, ok := event.(*models.Span)
			assert.True(t, ok)
			assert.Equal(t, otlpSpan.TraceID().String(), span.TraceID)
			assert.Equal(t, otlpSpan.SpanID().String(), span.SpanID)
			assert.Equal(t, int(otlpSpan.Status().Code()), int(span.Status))

			otlpSpan.Attributes().Range(
				func(k string, v pcommon.Value) bool {
					assert.True(t, span.Tags.Contains(k))
					return true
				},
			)

			for m := 0; m < otlpSpan.Links().Len(); m++ {
				otLink := otlpSpan.Links().At(m)
				assert.Equal(t, otLink.SpanID().String(), span.Links[m].SpanID)
				assert.Equal(t, otLink.TraceID().String(), span.Links[m].TraceID)
				assert.Equal(t, otLink.TraceState().AsRaw(), span.Links[m].TraceState)

				otLink.Attributes().Range(func(k string, v pcommon.Value) bool {
					assert.True(t, span.Links[m].Tags.Contains(k))
					return true
				})
			}

			for m := 0; m < otlpSpan.Events().Len(); m++ {
				otEvent := otlpSpan.Events().At(m)
				assert.Equal(t, otEvent.Name(), span.Events[m].Name)
				assert.Equal(t, int64(otEvent.Timestamp()), span.Events[m].Timestamp)
				otEvent.Attributes().Range(func(k string, v pcommon.Value) bool {
					assert.True(t, span.Links[m].Tags.Contains(k))
					return true
				})
			}
		}
	}
}

// reference: https://github.com/open-telemetry/opentelemetry-collector/blob/main/pdata/plog/json_test.go#L28
var logsOTLP = func() plog.Logs {
	ld := plog.NewLogs()
	rl := ld.ResourceLogs().AppendEmpty()
	rl.Resource().Attributes().PutStr("host.name", "testHost")
	rl.Resource().SetDroppedAttributesCount(1)
	rl.SetSchemaUrl("testSchemaURL")
	il := rl.ScopeLogs().AppendEmpty()
	il.Scope().SetName("name")
	il.Scope().SetVersion("")
	il.Scope().SetDroppedAttributesCount(1)
	il.SetSchemaUrl("ScopeLogsSchemaURL")
	lg := il.LogRecords().AppendEmpty()
	lg.SetSeverityNumber(plog.SeverityNumberError)
	lg.SetSeverityText("Error")
	lg.SetDroppedAttributesCount(1)
	lg.SetFlags(plog.DefaultLogRecordFlags)
	traceID := pcommon.TraceID([16]byte{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10})
	spanID := pcommon.SpanID([8]byte{0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18})
	lg.SetTraceID(traceID)
	lg.SetSpanID(spanID)
	lg.Body().SetStr("hello world")
	t := pcommon.NewTimestampFromTime(time.Now())
	lg.SetTimestamp(t)
	lg.SetObservedTimestamp(t)
	lg.Attributes().PutStr("sdkVersion", "1.0.1")
	return ld
}()

// reference: https://github.com/open-telemetry/opentelemetry-collector/blob/main/pdata/pmetric/json_test.go#L60
var metricsSumOTLPFull = func() pmetric.Metrics {
	metric := pmetric.NewMetrics()
	rs := metric.ResourceMetrics().AppendEmpty()
	rs.SetSchemaUrl("schemaURL")
	// Add resource.
	rs.Resource().Attributes().PutStr("host.name", "testHost")
	rs.Resource().Attributes().PutStr("service.name", "testService")
	rs.Resource().SetDroppedAttributesCount(1)
	// Add InstrumentationLibraryMetrics.
	m := rs.ScopeMetrics().AppendEmpty()
	m.Scope().SetName("instrumentation name")
	m.Scope().SetVersion("instrumentation version")
	m.Scope().Attributes().PutStr("instrumentation.attribute", "test")
	m.SetSchemaUrl("schemaURL")
	// Add Metric
	sumMetric := m.Metrics().AppendEmpty()
	sumMetric.SetName("test sum")
	sumMetric.SetDescription("test sum")
	sumMetric.SetUnit("unit")
	sum := sumMetric.SetEmptySum()
	sum.SetAggregationTemporality(pmetric.AggregationTemporalityCumulative)
	sum.SetIsMonotonic(true)
	datapoint := sum.DataPoints().AppendEmpty()
	datapoint.SetStartTimestamp(pcommon.NewTimestampFromTime(time.Now()))
	datapoint.SetIntValue(100)
	datapoint.Attributes().PutStr("string", "value")
	datapoint.Attributes().PutBool("bool", true)
	datapoint.Attributes().PutInt("int", 1)
	datapoint.Attributes().PutDouble("double", 1.1)
	datapoint.Attributes().PutEmptyBytes("bytes").FromRaw([]byte("foo"))
	exemplar := datapoint.Exemplars().AppendEmpty()
	exemplar.SetDoubleValue(99.3)
	exemplar.SetTimestamp(pcommon.NewTimestampFromTime(time.Now()))
	traceID := pcommon.TraceID([16]byte{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10})
	spanID := pcommon.SpanID([8]byte{0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18})
	exemplar.SetSpanID(spanID)
	exemplar.SetTraceID(traceID)
	exemplar.FilteredAttributes().PutStr("service.name", "testService")
	datapoint.SetTimestamp(pcommon.NewTimestampFromTime(time.Now()))

	datapoint2 := sum.DataPoints().AppendEmpty()
	datapoint2.SetStartTimestamp(pcommon.NewTimestampFromTime(time.Now()))
	datapoint2.SetIntValue(50)
	datapoint2.Attributes().PutStr("string", "value2")
	datapoint2.Attributes().PutBool("bool", false)
	datapoint2.Attributes().PutInt("int", 2)
	datapoint2.Attributes().PutDouble("double", 2.2)
	datapoint2.Attributes().PutEmptyBytes("bytes").FromRaw([]byte("bar"))
	return metric
}

var metricsGaugeOTLPFull = func() pmetric.Metrics {
	metric := pmetric.NewMetrics()
	rs := metric.ResourceMetrics().AppendEmpty()
	rs.SetSchemaUrl("schemaURL")
	// Add resource.
	rs.Resource().Attributes().PutStr("host.name", "testHost")
	rs.Resource().Attributes().PutStr("service.name", "testService")
	rs.Resource().SetDroppedAttributesCount(1)
	// Add InstrumentationLibraryMetrics.
	m := rs.ScopeMetrics().AppendEmpty()
	m.Scope().SetName("instrumentation name")
	m.Scope().SetVersion("instrumentation version")
	m.SetSchemaUrl("schemaURL")
	// Add Metric
	gaugeMetric := m.Metrics().AppendEmpty()
	gaugeMetric.SetName("test gauge")
	gaugeMetric.SetDescription("test gauge")
	gaugeMetric.SetUnit("unit")
	gauge := gaugeMetric.SetEmptyGauge()
	datapoint := gauge.DataPoints().AppendEmpty()
	datapoint.SetStartTimestamp(pcommon.NewTimestampFromTime(time.Now()))
	datapoint.SetDoubleValue(10.2)
	datapoint.Attributes().PutStr("string", "value")
	datapoint.Attributes().PutBool("bool", true)
	datapoint.Attributes().PutInt("int", 1)
	datapoint.Attributes().PutDouble("double", 1.1)
	datapoint.Attributes().PutEmptyBytes("bytes").FromRaw([]byte("foo"))
	exemplar := datapoint.Exemplars().AppendEmpty()
	exemplar.SetDoubleValue(99.3)
	exemplar.SetTimestamp(pcommon.NewTimestampFromTime(time.Now()))
	traceID := pcommon.TraceID([16]byte{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10})
	spanID := pcommon.SpanID([8]byte{0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18})
	exemplar.SetSpanID(spanID)
	exemplar.SetTraceID(traceID)
	exemplar.FilteredAttributes().PutStr("service.name", "testService")
	datapoint.SetTimestamp(pcommon.NewTimestampFromTime(time.Now()))
	return metric
}

var metricsHistogramOTLPFull = func() pmetric.Metrics {
	metric := pmetric.NewMetrics()
	rs := metric.ResourceMetrics().AppendEmpty()
	rs.SetSchemaUrl("schemaURL")
	// Add resource.
	rs.Resource().Attributes().PutStr("host.name", "testHost")
	rs.Resource().Attributes().PutStr("service.name", "testService")
	rs.Resource().SetDroppedAttributesCount(1)
	// Add InstrumentationLibraryMetrics.
	m := rs.ScopeMetrics().AppendEmpty()
	m.Scope().SetName("instrumentation name")
	m.Scope().SetVersion("instrumentation version")
	m.SetSchemaUrl("schemaURL")
	// Add Metric
	histogramMetric := m.Metrics().AppendEmpty()
	histogramMetric.SetName("test Histogram")
	histogramMetric.SetDescription("test Histogram")
	histogramMetric.SetUnit("unit")
	histogram := histogramMetric.SetEmptyHistogram()
	histogram.SetAggregationTemporality(pmetric.AggregationTemporalityCumulative)
	datapoint := histogram.DataPoints().AppendEmpty()
	datapoint.SetStartTimestamp(pcommon.NewTimestampFromTime(time.Now()))
	datapoint.Attributes().PutStr("string", "value")
	datapoint.Attributes().PutBool("bool", true)
	datapoint.Attributes().PutInt("int", 1)
	datapoint.Attributes().PutDouble("double", 1.1)
	datapoint.Attributes().PutEmptyBytes("bytes").FromRaw([]byte("foo"))
	datapoint.SetCount(4)
	datapoint.SetSum(345)
	datapoint.BucketCounts().FromRaw([]uint64{1, 1, 2})
	datapoint.ExplicitBounds().FromRaw([]float64{10, 100})
	exemplar := datapoint.Exemplars().AppendEmpty()
	exemplar.SetDoubleValue(99.3)
	exemplar.SetTimestamp(pcommon.NewTimestampFromTime(time.Now()))
	datapoint.SetMin(float64(time.Now().Unix()))
	traceID := pcommon.TraceID([16]byte{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10})
	spanID := pcommon.SpanID([8]byte{0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18})
	exemplar.SetSpanID(spanID)
	exemplar.SetTraceID(traceID)
	exemplar.FilteredAttributes().PutStr("service.name", "testService")
	datapoint.SetMax(float64(time.Now().Unix()))
	datapoint.SetTimestamp(pcommon.NewTimestampFromTime(time.Now()))
	return metric
}

var metricsExponentialHistogramOTLPFull = func() pmetric.Metrics {
	metric := pmetric.NewMetrics()
	rs := metric.ResourceMetrics().AppendEmpty()
	rs.SetSchemaUrl("schemaURL")
	// Add resource.
	rs.Resource().Attributes().PutStr("host.name", "testHost")
	rs.Resource().Attributes().PutStr("service.name", "testService")
	rs.Resource().SetDroppedAttributesCount(1)
	// Add InstrumentationLibraryMetrics.
	m := rs.ScopeMetrics().AppendEmpty()
	m.Scope().SetName("instrumentation name")
	m.Scope().SetVersion("instrumentation version")
	m.SetSchemaUrl("schemaURL")
	// Add Metric
	histogramMetric := m.Metrics().AppendEmpty()
	histogramMetric.SetName("test ExponentialHistogram")
	histogramMetric.SetDescription("test ExponentialHistogram")
	histogramMetric.SetUnit("unit")
	histogram := histogramMetric.SetEmptyExponentialHistogram()
	histogram.SetAggregationTemporality(pmetric.AggregationTemporalityCumulative)
	datapoint := histogram.DataPoints().AppendEmpty()
	datapoint.SetScale(1)
	datapoint.SetStartTimestamp(pcommon.NewTimestampFromTime(time.Now()))
	datapoint.Attributes().PutStr("string", "value")
	datapoint.Attributes().PutBool("bool", true)
	datapoint.Attributes().PutInt("int", 1)
	datapoint.Attributes().PutDouble("double", 1.1)
	datapoint.Attributes().PutEmptyBytes("bytes").FromRaw([]byte("foo"))
	datapoint.SetCount(4)
	datapoint.SetSum(345)
	datapoint.Positive().BucketCounts().FromRaw([]uint64{1, 1, 2})
	datapoint.Positive().SetOffset(2)
	datapoint.Negative().BucketCounts().FromRaw([]uint64{1, 1, 2})
	datapoint.Negative().SetOffset(3)

	exemplar := datapoint.Exemplars().AppendEmpty()
	exemplar.SetDoubleValue(99.3)
	exemplar.SetTimestamp(pcommon.NewTimestampFromTime(time.Now()))
	datapoint.SetMin(float64(time.Now().Unix()))
	traceID := pcommon.TraceID([16]byte{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10})
	spanID := pcommon.SpanID([8]byte{0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18})
	exemplar.SetSpanID(spanID)
	exemplar.SetTraceID(traceID)
	exemplar.FilteredAttributes().PutStr("service.name", "testService")
	datapoint.SetMax(float64(time.Now().Unix()))
	datapoint.Negative().BucketCounts().FromRaw([]uint64{1, 1, 2})
	datapoint.Negative().SetOffset(2)
	datapoint.SetZeroCount(5)
	datapoint.SetTimestamp(pcommon.NewTimestampFromTime(time.Now()))
	return metric
}

var metricsSummaryOTLPFull = func() pmetric.Metrics {
	metric := pmetric.NewMetrics()
	rs := metric.ResourceMetrics().AppendEmpty()
	rs.SetSchemaUrl("schemaURL")
	// Add resource.
	rs.Resource().Attributes().PutStr("host.name", "testHost")
	rs.Resource().Attributes().PutStr("service.name", "testService")
	rs.Resource().SetDroppedAttributesCount(1)
	// Add InstrumentationLibraryMetrics.
	m := rs.ScopeMetrics().AppendEmpty()
	m.Scope().SetName("instrumentation name")
	m.Scope().SetVersion("instrumentation version")
	m.SetSchemaUrl("schemaURL")
	// Add Metric
	summaryMetric := m.Metrics().AppendEmpty()
	summaryMetric.SetName("test summary")
	summaryMetric.SetDescription("test summary")
	summaryMetric.SetUnit("unit")
	summary := summaryMetric.SetEmptySummary()
	datapoint := summary.DataPoints().AppendEmpty()
	datapoint.SetStartTimestamp(pcommon.NewTimestampFromTime(time.Now()))
	datapoint.SetCount(100)
	datapoint.SetSum(1000)
	quantile := datapoint.QuantileValues().AppendEmpty()
	quantile.SetQuantile(0.5)
	quantile.SetValue(1.2)
	datapoint.Attributes().PutStr("string", "value")
	datapoint.Attributes().PutBool("bool", true)
	datapoint.Attributes().PutInt("int", 1)
	datapoint.Attributes().PutDouble("double", 1.1)
	datapoint.Attributes().PutEmptyBytes("bytes").FromRaw([]byte("foo"))
	datapoint.SetTimestamp(pcommon.NewTimestampFromTime(time.Now()))
	return metric
}

// reference: https://github.com/open-telemetry/opentelemetry-collector/blob/main/pdata/ptrace/json_test.go#L59
var tracesOTLPFull = func() ptrace.Traces {
	traceID := pcommon.TraceID([16]byte{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10})
	spanID := pcommon.SpanID([8]byte{0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18})
	td := ptrace.NewTraces()
	// Add ResourceSpans.
	rs := td.ResourceSpans().AppendEmpty()
	rs.SetSchemaUrl("schemaURL")
	// Add resource.
	rs.Resource().Attributes().PutStr("host.name", "testHost")
	rs.Resource().Attributes().PutStr("service.name", "testService")
	rs.Resource().SetDroppedAttributesCount(1)
	// Add ScopeSpans.
	il := rs.ScopeSpans().AppendEmpty()
	il.Scope().SetName("scope name")
	il.Scope().SetVersion("scope version")
	il.SetSchemaUrl("schemaURL")
	// Add spans.
	sp := il.Spans().AppendEmpty()
	sp.SetName("testSpan")
	sp.SetKind(ptrace.SpanKindClient)
	sp.SetDroppedAttributesCount(1)
	sp.SetStartTimestamp(pcommon.NewTimestampFromTime(time.Now()))
	sp.SetTraceID(traceID)
	sp.SetSpanID(spanID)
	sp.SetDroppedEventsCount(1)
	sp.SetDroppedLinksCount(1)
	sp.SetEndTimestamp(pcommon.NewTimestampFromTime(time.Now()))
	sp.SetParentSpanID(spanID)
	sp.TraceState().FromRaw("state")
	sp.Status().SetCode(ptrace.StatusCodeOk)
	sp.Status().SetMessage("message")
	// Add attributes.
	sp.Attributes().PutStr("string", "value")
	sp.Attributes().PutBool("bool", true)
	sp.Attributes().PutInt("int", 1)
	sp.Attributes().PutDouble("double", 1.1)
	sp.Attributes().PutEmptyBytes("bytes").FromRaw([]byte("foo"))
	arr := sp.Attributes().PutEmptySlice("array")
	arr.AppendEmpty().SetInt(1)
	arr.AppendEmpty().SetStr("str")
	kvList := sp.Attributes().PutEmptyMap("kvList")
	kvList.PutInt("int", 1)
	kvList.PutStr("string", "string")
	// Add events.
	event := sp.Events().AppendEmpty()
	event.SetName("eventName")
	event.SetTimestamp(pcommon.NewTimestampFromTime(time.Now()))
	event.SetDroppedAttributesCount(1)
	event.Attributes().PutStr("string", "value")
	event.Attributes().PutBool("bool", true)
	event.Attributes().PutInt("int", 1)
	event.Attributes().PutDouble("double", 1.1)
	event.Attributes().PutEmptyBytes("bytes").FromRaw([]byte("foo"))
	// Add links.
	link := sp.Links().AppendEmpty()
	link.TraceState().FromRaw("state")
	link.SetTraceID(traceID)
	link.SetSpanID(spanID)
	link.SetDroppedAttributesCount(1)
	link.Attributes().PutStr("string", "value")
	link.Attributes().PutBool("bool", true)
	link.Attributes().PutInt("int", 1)
	link.Attributes().PutDouble("double", 1.1)
	link.Attributes().PutEmptyBytes("bytes").FromRaw([]byte("foo"))
	// Add another span.
	sp2 := il.Spans().AppendEmpty()
	sp2.SetName("testSpan2")
	return td
}()

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
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.opentelemetry.io/collector/pdata/ptrace"

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

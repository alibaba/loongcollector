// Copyright 2022 iLogtail Authors
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
	"errors"
	"fmt"
	"math"
	"net/http"
	"strconv"
	"time"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.opentelemetry.io/collector/pdata/plog"
	"go.opentelemetry.io/collector/pdata/plog/plogotlp"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.opentelemetry.io/collector/pdata/pmetric/pmetricotlp"
	"go.opentelemetry.io/collector/pdata/ptrace"
	"go.opentelemetry.io/collector/pdata/ptrace/ptraceotlp"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/common"
	"github.com/alibaba/ilogtail/pkg/protocol/otlp"
	"github.com/alibaba/ilogtail/pkg/util"
)

const (
	pbContentType   = "application/x-protobuf"
	jsonContentType = "application/json"
)

const (
	logEventName = "log_event"
)

// Decoder impl
type Decoder struct {
	Format string
}

// Decode impl
func (d *Decoder) Decode(data []byte, req *http.Request, tags map[string]string) (logs []*protocol.Log, err error) {
	switch d.Format {
	case common.ProtocolOTLPLogV1:
		otlpLogReq := plogotlp.NewExportRequest()
		otlpLogReq, err = DecodeOtlpRequest(otlpLogReq, data, req)
		if err != nil {
			return logs, err
		}
		logs, err = d.ConvertOtlpLogV1(otlpLogReq)
	case common.ProtocolOTLPMetricV1:
		otlpMetricReq := pmetricotlp.NewExportRequest()
		otlpMetricReq, err = DecodeOtlpRequest(otlpMetricReq, data, req)
		if err != nil {
			return logs, err
		}
		logs, err = d.ConvertOtlpMetricV1(otlpMetricReq)
	case common.ProtocolOTLPTraceV1:
		otlpTraceReq := ptraceotlp.NewExportRequest()
		otlpTraceReq, err = DecodeOtlpRequest(otlpTraceReq, data, req)
		if err != nil {
			return logs, err
		}
		logs, err = d.ConvertOtlpTraceV1(otlpTraceReq)
	default:
		err = errors.New("Invalid RequestURI: " + req.RequestURI)
	}
	return logs, err
}

// ParseRequest impl
func (d *Decoder) ParseRequest(res http.ResponseWriter, req *http.Request, maxBodySize int64) (data []byte, statusCode int, err error) {
	return common.CollectBody(res, req, maxBodySize)
}

// DecodeV2 impl
func (d *Decoder) DecodeV2(data []byte, req *http.Request) (groups []*models.PipelineGroupEvents, err error) {
	switch d.Format {
	case common.ProtocolOTLPLogV1:
		otlpLogReq := plogotlp.NewExportRequest()
		otlpLogReq, err = DecodeOtlpRequest(otlpLogReq, data, req)
		if err != nil {
			return groups, err
		}
		groups, err = ConvertOtlpLogRequestToGroupEvents(otlpLogReq)
	case common.ProtocolOTLPMetricV1:
		otlpMetricReq := pmetricotlp.NewExportRequest()
		otlpMetricReq, err = DecodeOtlpRequest(otlpMetricReq, data, req)
		if err != nil {
			return groups, err
		}
		groups, err = ConvertOtlpMetricRequestToGroupEvents(otlpMetricReq)
	case common.ProtocolOTLPTraceV1:
		otlpTraceReq := ptraceotlp.NewExportRequest()
		otlpTraceReq, err = DecodeOtlpRequest(otlpTraceReq, data, req)
		if err != nil {
			return groups, err
		}
		groups, err = ConvertOtlpTraceRequestToGroupEvents(otlpTraceReq)
	default:
		err = errors.New("Invalid RequestURI: " + req.RequestURI)
	}
	return groups, err
}

func (d *Decoder) ConvertOtlpLogV1(otlpLogReq plogotlp.ExportRequest) (logs []*protocol.Log, err error) {
	resLogs := otlpLogReq.Logs().ResourceLogs()
	for i := 0; i < resLogs.Len(); i++ {
		resourceLog := resLogs.At(i)
		sLogs := resourceLog.ScopeLogs()
		for j := 0; j < sLogs.Len(); j++ {
			scopeLog := sLogs.At(j)
			lRecords := scopeLog.LogRecords()
			for k := 0; k < lRecords.Len(); k++ {
				logRecord := lRecords.At(k)

				protoContents := []*protocol.Log_Content{
					{
						Key:   "time_unix_nano",
						Value: strconv.FormatInt(logRecord.Timestamp().AsTime().UnixNano(), 10),
					},
					{
						Key:   "severity_number",
						Value: strconv.FormatInt(int64(logRecord.SeverityNumber()), 10),
					},
					{
						Key:   "severity_text",
						Value: logRecord.SeverityText(),
					},
					{
						Key:   "content",
						Value: logRecord.Body().AsString(),
					},
				}

				if logRecord.Attributes().Len() != 0 {
					if d, err := json.Marshal(logRecord.Attributes().AsRaw()); err == nil {
						protoContents = append(protoContents, &protocol.Log_Content{
							Key:   "attributes",
							Value: string(d),
						})
					}
				}

				if resourceLog.Resource().Attributes().Len() != 0 {
					if d, err := json.Marshal(resourceLog.Resource().Attributes().AsRaw()); err == nil {
						protoContents = append(protoContents, &protocol.Log_Content{
							Key:   "resources",
							Value: string(d),
						})
					}
				}

				protoLog := &protocol.Log{
					Time:     uint32(logRecord.Timestamp().AsTime().Unix()),
					Contents: protoContents,
				}
				logs = append(logs, protoLog)
			}
		}
	}

	return logs, nil
}

func (d *Decoder) ConvertOtlpMetricV1(otlpMetricReq pmetricotlp.ExportRequest) (logs []*protocol.Log, err error) {
	return logs, fmt.Errorf("does_not_support_otlpmetrics")
}

func (d *Decoder) ConvertOtlpTraceV1(otlpTraceReq ptraceotlp.ExportRequest) (logs []*protocol.Log, err error) {
	return logs, fmt.Errorf("does_not_support_otlptraces")
}

// DecodeOtlpRequest decodes the data and fills into the otlp logs/metrics/traces export request.
func DecodeOtlpRequest[P interface {
	UnmarshalProto(data []byte) error
	UnmarshalJSON(data []byte) error
}](des P, data []byte, req *http.Request) (P, error) {
	var err error
	switch req.Header.Get("Content-Type") {
	case pbContentType:
		if err = des.UnmarshalProto(data); err != nil {
			return des, err
		}
	case jsonContentType:
		if err = des.UnmarshalJSON(data); err != nil {
			return des, err
		}
	default:
		err = errors.New("Invalid ContentType: " + req.Header.Get("Content-Type"))
	}
	return des, err
}

func ConvertOtlpLogRequestToGroupEvents(otlpLogReq plogotlp.ExportRequest) ([]*models.PipelineGroupEvents, error) {
	return ConvertOtlpLogsToGroupEvents(otlpLogReq.Logs())
}

func ConvertOtlpMetricRequestToGroupEvents(otlpMetricReq pmetricotlp.ExportRequest) ([]*models.PipelineGroupEvents, error) {
	return ConvertOtlpMetricsToGroupEvents(otlpMetricReq.Metrics())
}

func ConvertOtlpTraceRequestToGroupEvents(otlpTraceReq ptraceotlp.ExportRequest) ([]*models.PipelineGroupEvents, error) {
	return ConvertOtlpTracesToGroupEvents(otlpTraceReq.Traces())
}

func ConvertOtlpLogsToGroupEvents(logs plog.Logs) (groupEventsSlice []*models.PipelineGroupEvents, err error) {
	resLogs := logs.ResourceLogs()
	resLogsLen := resLogs.Len()

	if resLogsLen == 0 {
		return
	}

	for i := 0; i < resLogsLen; i++ {
		resourceLog := resLogs.At(i)
		resourceAttrs := resourceLog.Resource().Attributes()
		scopeLogs := resourceLog.ScopeLogs()
		scopeLogsLen := scopeLogs.Len()

		for j := 0; j < scopeLogsLen; j++ {
			scopeLog := scopeLogs.At(j)
			scope := scopeLog.Scope()
			scopeTags := genScopeTags(scope)
			otLogs := scopeLog.LogRecords()
			otLogsLen := otLogs.Len()

			groupEvents := &models.PipelineGroupEvents{
				Group:  models.NewGroup(attrs2Meta(resourceAttrs), scopeTags),
				Events: make([]models.PipelineEvent, 0, otLogs.Len()),
			}

			for k := 0; k < otLogsLen; k++ {
				logRecord := otLogs.At(k)

				var body []byte
				switch logRecord.Body().Type() {
				case pcommon.ValueTypeBytes:
					body = logRecord.Body().Bytes().AsRaw()
				case pcommon.ValueTypeStr:
					body = util.ZeroCopyStringToBytes(logRecord.Body().AsString())
				default:
					body = util.ZeroCopyStringToBytes(fmt.Sprintf("%#v", logRecord.Body().AsRaw()))
				}

				level := logRecord.SeverityText()
				if level == "" {
					level = logRecord.SeverityNumber().String()
				}

				event := models.NewLog(
					logEventName,
					body,
					level,
					logRecord.SpanID().String(),
					logRecord.TraceID().String(),
					attrs2Tags(logRecord.Attributes()),
					uint64(logRecord.Timestamp().AsTime().UnixNano()),
				)
				event.ObservedTimestamp = uint64(logRecord.ObservedTimestamp().AsTime().UnixNano())
				event.Tags.Add(otlp.TagKeyLogFlag, strconv.Itoa(int(logRecord.Flags())))
				groupEvents.Events = append(groupEvents.Events, event)
			}

			groupEventsSlice = append(groupEventsSlice, groupEvents)
		}
	}

	return groupEventsSlice, err
}

func ConvertOtlpMetricsToGroupEvents(metrics pmetric.Metrics) (groupEventsSlice []*models.PipelineGroupEvents, err error) {
	resMetrics := metrics.ResourceMetrics()
	if resMetrics.Len() == 0 {
		return
	}

	for i := 0; i < resMetrics.Len(); i++ {
		resourceMetric := resMetrics.At(i)
		resourceAttrs := resourceMetric.Resource().Attributes()
		scopeMetrics := resourceMetric.ScopeMetrics()

		for j := 0; j < scopeMetrics.Len(); j++ {
			scopeMetric := scopeMetrics.At(j)
			scope := scopeMetric.Scope()
			scopeTags := genScopeTags(scope)
			otMetrics := scopeMetric.Metrics()

			groupEvents := &models.PipelineGroupEvents{
				Group:  models.NewGroup(attrs2Meta(resourceAttrs), scopeTags),
				Events: make([]models.PipelineEvent, 0, otMetrics.Len()),
			}

			for k := 0; k < otMetrics.Len(); k++ {
				otMetric := otMetrics.At(k)
				metricName := otMetric.Name()
				metricUnit := otMetric.Unit()
				metricDescription := otMetric.Description()

				switch otMetric.Type() {
				case pmetric.MetricTypeGauge:
					otGauge := otMetric.Gauge()
					otDatapoints := otGauge.DataPoints()
					for l := 0; l < otDatapoints.Len(); l++ {
						datapoint := otDatapoints.At(l)
						metric := newMetricFromGaugeDatapoint(datapoint, metricName, metricUnit, metricDescription)
						groupEvents.Events = append(groupEvents.Events, metric)
					}
				case pmetric.MetricTypeSum:
					otSum := otMetric.Sum()
					isMonotonic := strconv.FormatBool(otSum.IsMonotonic())
					aggregationTemporality := otSum.AggregationTemporality()
					otDatapoints := otSum.DataPoints()

					for l := 0; l < otDatapoints.Len(); l++ {
						datapoint := otDatapoints.At(l)
						metric := newMetricFromSumDatapoint(datapoint, aggregationTemporality, isMonotonic, metricName, metricUnit, metricDescription)
						groupEvents.Events = append(groupEvents.Events, metric)
					}
				case pmetric.MetricTypeSummary:
					otSummary := otMetric.Summary()
					otDatapoints := otSummary.DataPoints()
					for l := 0; l < otDatapoints.Len(); l++ {
						datapoint := otDatapoints.At(l)
						metric := newMetricFromSummaryDatapoint(datapoint, metricName, metricUnit, metricDescription)
						groupEvents.Events = append(groupEvents.Events, metric)
					}
				case pmetric.MetricTypeHistogram:
					otHistogram := otMetric.Histogram()
					aggregationTemporality := otHistogram.AggregationTemporality()
					otDatapoints := otHistogram.DataPoints()

					for l := 0; l < otDatapoints.Len(); l++ {
						datapoint := otDatapoints.At(l)
						metric := newMetricFromHistogramDatapoint(datapoint, aggregationTemporality, metricName, metricUnit, metricDescription)
						groupEvents.Events = append(groupEvents.Events, metric)
					}
				case pmetric.MetricTypeExponentialHistogram:
					otExponentialHistogram := otMetric.ExponentialHistogram()
					aggregationTemporality := otExponentialHistogram.AggregationTemporality()
					otDatapoints := otExponentialHistogram.DataPoints()

					for l := 0; l < otDatapoints.Len(); l++ {
						datapoint := otDatapoints.At(l)
						metric := newMetricFromExponentialHistogramDatapoint(datapoint, aggregationTemporality, metricName, metricUnit, metricDescription)
						groupEvents.Events = append(groupEvents.Events, metric)
					}
				default:
					// TODO:
					// find a better way to handle metric with type MetricTypeEmpty.
					metric := models.NewSingleValueMetric(metricName, models.MetricTypeUntyped, models.NewTags(), time.Now().Unix(), 0)
					metric.Unit = metricUnit
					metric.Description = otMetric.Description()
					groupEvents.Events = append(groupEvents.Events, metric)
				}

			}
			groupEventsSlice = append(groupEventsSlice, groupEvents)
		}
	}
	return groupEventsSlice, err
}

func ConvertOtlpTracesToGroupEvents(traces ptrace.Traces) (groupEventsSlice []*models.PipelineGroupEvents, err error) {
	resSpans := traces.ResourceSpans()

	for i := 0; i < resSpans.Len(); i++ {
		resourceSpan := resSpans.At(i)
		// A Resource is an immutable representation of the entity producing telemetry as Attributes.
		// These attributes should be stored in meta.
		resourceAttrs := resourceSpan.Resource().Attributes()
		scopeSpans := resourceSpan.ScopeSpans()

		for j := 0; j < scopeSpans.Len(); j++ {
			scopeSpan := scopeSpans.At(j)
			scope := scopeSpan.Scope()
			scopeTags := genScopeTags(scope)
			otSpans := scopeSpan.Spans()

			groupEvents := &models.PipelineGroupEvents{
				Group:  models.NewGroup(attrs2Meta(resourceAttrs), scopeTags),
				Events: make([]models.PipelineEvent, 0, otSpans.Len()),
			}

			for k := 0; k < otSpans.Len(); k++ {
				otSpan := otSpans.At(k)

				spanName := otSpan.Name()
				traceID := otSpan.TraceID().String()
				spanID := otSpan.SpanID().String()
				spanKind := models.SpanKind(otSpan.Kind())
				startTs := otSpan.StartTimestamp()
				endTs := otSpan.EndTimestamp()
				spanAttrs := otSpan.Attributes()
				events := convertSpanEvents(otSpan.Events())
				links := convertSpanLinks(otSpan.Links())

				span := models.NewSpan(spanName, traceID, spanID, spanKind,
					uint64(startTs), uint64(endTs), attrs2Tags(spanAttrs), events, links)

				span.ParentSpanID = otSpan.ParentSpanID().String()
				span.Status = models.StatusCode(otSpan.Status().Code())

				if message := otSpan.Status().Message(); len(message) > 0 {
					span.Tags.Add(otlp.TagKeySpanStatusMessage, message)
				}

				span.Tags.Add(otlp.TagKeySpanDroppedEventsCount, strconv.Itoa(int(otSpan.DroppedEventsCount())))
				span.Tags.Add(otlp.TagKeySpanDroppedAttrsCount, strconv.Itoa(int(otSpan.DroppedAttributesCount())))
				span.Tags.Add(otlp.TagKeySpanDroppedLinksCount, strconv.Itoa(int(otSpan.DroppedLinksCount())))

				groupEvents.Events = append(groupEvents.Events, span)
			}
			groupEventsSlice = append(groupEventsSlice, groupEvents)
		}

	}
	return groupEventsSlice, err
}

func genScopeTags(scope pcommon.InstrumentationScope) models.Tags {
	scopeTags := attrs2Tags(scope.Attributes())
	scopeTags.Add(otlp.TagKeyScopeName, scope.Name())
	scopeTags.Add(otlp.TagKeyScopeVersion, scope.Version())
	scopeTags.Add(otlp.TagKeyScopeDroppedAttributesCount, strconv.Itoa(int(scope.DroppedAttributesCount())))
	return scopeTags
}

func convertSpanLinks(srcLinks ptrace.SpanLinkSlice) []*models.SpanLink {
	spanLinks := make([]*models.SpanLink, 0, srcLinks.Len())

	for i := 0; i < srcLinks.Len(); i++ {
		srcLink := srcLinks.At(i)

		spanLink := models.SpanLink{
			TraceID:    srcLink.TraceID().String(),
			SpanID:     srcLink.SpanID().String(),
			TraceState: srcLink.TraceState().AsRaw(),
			Tags:       attrs2Tags(srcLink.Attributes()),
		}
		spanLinks = append(spanLinks, &spanLink)
	}
	return spanLinks
}

func convertSpanEvents(srcEvents ptrace.SpanEventSlice) []*models.SpanEvent {
	events := make([]*models.SpanEvent, 0, srcEvents.Len())
	for i := 0; i < srcEvents.Len(); i++ {
		srcEvent := srcEvents.At(i)

		spanEvent := &models.SpanEvent{
			Name:      srcEvent.Name(),
			Timestamp: int64(srcEvent.Timestamp()),
			Tags:      attrs2Tags(srcEvent.Attributes()),
		}
		events = append(events, spanEvent)
	}
	return events
}

func attrs2Tags(attributes pcommon.Map) models.Tags {
	tags := models.NewTags()

	attributes.Range(
		func(k string, v pcommon.Value) bool {
			tags.Add(k, v.AsString())
			return true
		},
	)
	return tags
}

func attrs2Meta(attributes pcommon.Map) models.Metadata {
	meta := models.NewMetadata()

	attributes.Range(
		func(k string, v pcommon.Value) bool {
			meta.Add(k, v.AsString())
			return true
		},
	)
	return meta
}

func getValue(intValue int64, doubleValue float64) float64 {
	if intValue != 0 {
		return float64(intValue)
	}

	return doubleValue
}

func newMetricFromGaugeDatapoint(datapoint pmetric.NumberDataPoint, metricName, metricUnit, metricDescription string) *models.Metric {
	timestamp := int64(datapoint.Timestamp())
	startTimestamp := datapoint.StartTimestamp()
	tags := attrs2Tags(datapoint.Attributes())

	value := getValue(datapoint.IntValue(), datapoint.DoubleValue())
	metric := models.NewSingleValueMetric(metricName, models.MetricTypeGauge, tags, timestamp, value)
	metric.Unit = metricUnit
	metric.Description = metricDescription
	metric.SetObservedTimestamp(uint64(startTimestamp))
	return metric
}

func newMetricFromSumDatapoint(datapoint pmetric.NumberDataPoint, aggregationTemporality pmetric.AggregationTemporality, isMonotonic, metricName, metricUnit, metricDescription string) *models.Metric {
	timestamp := int64(datapoint.Timestamp())
	startTimestamp := datapoint.StartTimestamp()
	tags := attrs2Tags(datapoint.Attributes())
	tags.Add(otlp.TagKeyMetricIsMonotonic, isMonotonic)
	tags.Add(otlp.TagKeyMetricAggregationTemporality, aggregationTemporality.String())

	value := getValue(datapoint.IntValue(), datapoint.DoubleValue())
	var metric *models.Metric
	if aggregationTemporality == pmetric.AggregationTemporalityCumulative {
		metric = models.NewSingleValueMetric(metricName, models.MetricTypeCounter, tags, timestamp, value)
	} else {
		metric = models.NewSingleValueMetric(metricName, models.MetricTypeRateCounter, tags, timestamp, value)
	}
	metric.Unit = metricUnit
	metric.Description = metricDescription
	metric.SetObservedTimestamp(uint64(startTimestamp))
	return metric
}

func newMetricFromSummaryDatapoint(datapoint pmetric.SummaryDataPoint, metricName, metricUnit, metricDescription string) *models.Metric {
	timestamp := int64(datapoint.Timestamp())
	startTimestamp := datapoint.StartTimestamp()
	tags := attrs2Tags(datapoint.Attributes())

	multivalue := models.NewMetricMultiValue()
	summaryValues := datapoint.QuantileValues()
	for m := 0; m < summaryValues.Len(); m++ {
		summaryValue := summaryValues.At(m)
		quantile := summaryValue.Quantile()
		value := summaryValue.Value()
		multivalue.Add(strconv.FormatFloat(quantile, 'f', -1, 64), value)
	}

	multivalue.Add(otlp.FieldCount, float64(datapoint.Count()))
	multivalue.Add(otlp.FieldSum, datapoint.Sum())

	metric := models.NewMultiValuesMetric(metricName, models.MetricTypeSummary, tags, timestamp, multivalue.GetMultiValues())
	metric.Unit = metricUnit
	metric.Description = metricDescription
	metric.SetObservedTimestamp(uint64(startTimestamp))
	return metric
}

func newMetricFromHistogramDatapoint(datapoint pmetric.HistogramDataPoint, aggregationTemporality pmetric.AggregationTemporality, metricName, metricUnit, metricDescription string) *models.Metric {
	timestamp := int64(datapoint.Timestamp())
	startTimestamp := datapoint.StartTimestamp()

	tags := attrs2Tags(datapoint.Attributes())
	tags.Add(otlp.TagKeyMetricAggregationTemporality, aggregationTemporality.String())
	tags.Add(otlp.TagKeyMetricHistogramType, pmetric.MetricTypeHistogram.String())

	// TODO:
	// handle datapoint's Exemplars, Flags
	multivalue := models.NewMetricMultiValue()
	multivalue.Add(otlp.FieldCount, float64(datapoint.Count()))

	if datapoint.HasSum() {
		multivalue.Add(otlp.FieldSum, datapoint.Sum())
	}
	if datapoint.HasMin() {
		multivalue.Add(otlp.FieldMin, datapoint.Min())
	}

	if datapoint.HasMax() {
		multivalue.Add(otlp.FieldMax, datapoint.Max())
	}

	bucketCounts, explicitBounds := datapoint.BucketCounts(), datapoint.ExplicitBounds()

	// bucketsCounts can be 0, otherwise, #bucketCounts == #bounds + 1.
	if bucketCounts.Len() == 0 || bucketCounts.Len() != explicitBounds.Len()+1 {
		metric := models.NewMultiValuesMetric(metricName, models.MetricTypeHistogram, tags, timestamp, multivalue.GetMultiValues())
		metric.Unit = metricUnit
		metric.SetObservedTimestamp(uint64(startTimestamp))
		return metric
	}

	// for bucket bounds: [0, 5, 10], there are 4 bucket counts: (-inf, 0], (0, 5], (5, 10], (10, +inf]
	for m := 0; m < bucketCounts.Len(); m++ {
		lowerBound := math.Inf(-1)
		upperBound := math.Inf(+1)
		if m > 0 {
			lowerBound = explicitBounds.At(m - 1)
		}
		if m < explicitBounds.Len() {
			upperBound = explicitBounds.At(m)
		}
		fieldName := otlp.ComposeBucketFieldName(lowerBound, upperBound, true)

		count := bucketCounts.At(m)
		multivalue.Add(fieldName, float64(count))
	}

	metric := models.NewMultiValuesMetric(metricName, models.MetricTypeHistogram, tags, timestamp, multivalue.GetMultiValues())
	metric.Unit = metricUnit
	metric.Description = metricDescription
	metric.SetObservedTimestamp(uint64(startTimestamp))
	return metric
}

// Note that ExponentialHistogram is still in experimental status.
// https://opentelemetry.io/docs/reference/specification/metrics/data-model/#exponentialhistogram
func newMetricFromExponentialHistogramDatapoint(datapoint pmetric.ExponentialHistogramDataPoint, aggregationTemporality pmetric.AggregationTemporality, metricName, metricUnit, metricDescription string) *models.Metric {
	timestamp := int64(datapoint.Timestamp())
	startTimestamp := datapoint.StartTimestamp()

	tags := attrs2Tags(datapoint.Attributes())
	tags.Add(otlp.TagKeyMetricAggregationTemporality, aggregationTemporality.String())
	tags.Add(otlp.TagKeyMetricHistogramType, pmetric.MetricTypeExponentialHistogram.String())

	// TODO:
	// handle datapoint's Exemplars, Flags
	multivalue := models.NewMetricMultiValue()
	multivalue.Add(otlp.FieldCount, float64(datapoint.Count()))

	if datapoint.HasSum() {
		multivalue.Add(otlp.FieldSum, datapoint.Sum())
	}
	if datapoint.HasMin() {
		multivalue.Add(otlp.FieldMin, datapoint.Min())
	}

	if datapoint.HasMax() {
		multivalue.Add(otlp.FieldMax, datapoint.Max())
	}

	scale := datapoint.Scale()
	base := math.Pow(2, math.Pow(2, float64(-scale)))
	multivalue.Values.Add(otlp.FieldScale, float64(scale)) // store scale in multivalues

	// For example, when scale is 3, base is 2 ** (2 ** -3) = 2 ** (1/8).
	// The negative bucket bounds look like:
	// ...., [-2 ** (2/8),  -2 ** (1/8)), [-2 ** (1/8), -2 ** (0/8)),
	// The positive bucket bounds look like:
	// (2 ** (0/8), 2 ** (1/8)], (2 ** (1/8), 2 ** (2/8)], ....
	// Meanwhile, there is also a zero bucket.
	postiveFields := genExponentialHistogramValues(true, base, datapoint.Positive())
	negativeFields := genExponentialHistogramValues(false, base, datapoint.Negative())

	multivalue.Values.AddAll(postiveFields)
	multivalue.Values.AddAll(negativeFields)

	multivalue.Add(otlp.FieldZeroCount, float64(datapoint.ZeroCount()))

	metric := models.NewMultiValuesMetric(metricName, models.MetricTypeHistogram, tags, timestamp, multivalue.GetMultiValues())
	metric.Unit = metricUnit
	metric.Description = metricDescription
	metric.SetObservedTimestamp(uint64(startTimestamp))
	return metric
}

func genExponentialHistogramValues(isPositive bool, base float64, buckets pmetric.ExponentialHistogramDataPointBuckets) map[string]float64 {
	offset := buckets.Offset()
	rawbucketCounts := buckets.BucketCounts()
	res := make(map[string]float64, rawbucketCounts.Len())
	for i := 0; i < rawbucketCounts.Len(); i++ {
		bucketCount := rawbucketCounts.At(i)
		lowerBoundary := math.Pow(base, float64(int(offset)+i))
		upperBoundary := lowerBoundary * base
		fieldKey := otlp.ComposeBucketFieldName(lowerBoundary, upperBoundary, isPositive)
		res[fieldKey] = float64(bucketCount)
	}
	if isPositive {
		res[otlp.FieldPositiveOffset] = float64(offset)
	} else {
		res[otlp.FieldNegativeOffset] = float64(offset)
	}
	return res
}

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

package protocol

import (
	"encoding/hex"
	"errors"
	"fmt"
	"strconv"
	"time"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.opentelemetry.io/collector/pdata/plog"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.opentelemetry.io/collector/pdata/ptrace"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/protocol/otlp"
)

var (
	bodyKey  = "content"
	levelKey = "level"
)

var (
	errBytesLengthNotMatch = errors.New("bytes_length_not_match")
)

func (c *Converter) ConvertToOtlpResourseLogs(logGroup *protocol.LogGroup, targetFields []string) (plog.ResourceLogs, []map[string]string, error) {
	rsLogs := plog.NewResourceLogs()
	desiredValues := make([]map[string]string, len(logGroup.Logs))

	if logGroup.GetSource() != "" {
		rsLogs.Resource().Attributes().PutStr("source", logGroup.GetSource())
	}

	if logGroup.GetTopic() != "" {
		rsLogs.Resource().Attributes().PutStr("topic", logGroup.GetTopic())
	}

	if logGroup.GetMachineUUID() != "" {
		rsLogs.Resource().Attributes().PutStr("machine_uuid", logGroup.GetMachineUUID())
	}

	for _, t := range logGroup.LogTags {
		rsLogs.Resource().Attributes().PutStr(t.Key, t.Value)
	}

	scopeLog := rsLogs.ScopeLogs().AppendEmpty()

	for i, log := range logGroup.Logs {
		logRecord := scopeLog.LogRecords().AppendEmpty()

		contents, tags := convertLogToMap(log, logGroup.LogTags, logGroup.Source, logGroup.Topic, c.TagKeyRenameMap)
		desiredValue, err := findTargetValues(targetFields, contents, tags, c.TagKeyRenameMap)
		if err != nil {
			return rsLogs, nil, err
		}
		desiredValues[i] = desiredValue

		for k, v := range contents {
			if k != bodyKey && k != levelKey {
				logRecord.Attributes().PutStr(k, v)
			}
		}
		for k, v := range tags {
			logRecord.Attributes().PutStr(k, v)
		}

		logRecord.SetObservedTimestamp(pcommon.Timestamp(time.Now().UnixNano()))
		if config.LogtailGlobalConfig.EnableTimestampNanosecond {
			logRecord.SetTimestamp(pcommon.Timestamp(uint64(log.Time)*uint64(time.Second)) + pcommon.Timestamp(uint64(*log.TimeNs)*uint64(time.Nanosecond)))
		} else {
			logRecord.SetTimestamp(pcommon.Timestamp(uint64(log.Time) * uint64(time.Second)))
		}

		if body, has := contents[bodyKey]; has {
			logRecord.Body().SetStr(body)
		}
		if level, has := contents[levelKey]; has {
			logRecord.SetSeverityText(level)
		} else if level, has = tags[level]; has {
			logRecord.SetSeverityText(level)
		}

	}

	return rsLogs, desiredValues, nil
}

func ConvertPipelineEventToOtlpEvent[
	T1 plog.ResourceLogs,
	T2 pmetric.ResourceMetrics,
	T3 ptrace.ResourceSpans,
](c *Converter, ps *models.PipelineGroupEvents) (t1 T1, t2 T2, t3 T3, err error) {
	switch c.Protocol {
	case ProtocolOtlpV1:
		rsLogs := plog.NewResourceLogs()
		rsMetrics := pmetric.NewResourceMetrics()
		rsTraces := ptrace.NewResourceSpans()
		err = ConvertPipelineGroupEvenstsToOtlpEvents(ps, rsLogs, rsMetrics, rsTraces)
		return T1(rsLogs), T2(rsMetrics), T3(rsTraces), err
	default:
		err = fmt.Errorf("unsupported protocol %v", c.Protocol)
	}
	return
}

// PipelineGroupEvents -> OTLP Logs/Metrics/Traces
func (c *Converter) ConvertPipelineGroupEventsToOTLPEventsV1(ps *models.PipelineGroupEvents) (plog.ResourceLogs, pmetric.ResourceMetrics, ptrace.ResourceSpans, error) {
	var err error
	rsLogs := plog.NewResourceLogs()
	rsMetrics := pmetric.NewResourceMetrics()
	rsTraces := ptrace.NewResourceSpans()
	err = ConvertPipelineGroupEvenstsToOtlpEvents(ps, rsLogs, rsMetrics, rsTraces)
	return rsLogs, rsMetrics, rsTraces, err
}

func ConvertPipelineGroupEvenstsToOtlpEvents(ps *models.PipelineGroupEvents, rsLogs plog.ResourceLogs, rsMetrics pmetric.ResourceMetrics, rsTraces ptrace.ResourceSpans) error {
	var err error
	if ps == nil || len(ps.Events) == 0 {
		return err
	}
	meta := ps.Group.Metadata
	groupTags := ps.Group.Tags

	var scopeLog plog.ScopeLogs
	var scopeMetric pmetric.ScopeMetrics
	var scopeTrace ptrace.ScopeSpans

	hasLogs, hasMetrics, hasTraces := false, false, false
	for _, v := range ps.Events {
		switch v.GetType() {
		case models.EventTypeLogging:
			if !hasLogs {
				setAttributes(rsLogs.Resource().Attributes(), meta)
				scopeLog = plog.NewScopeLogs()
				setScope(scopeLog, groupTags)
				hasLogs = true
			}
			err = ConvertPipelineEventToOtlpLog(v, scopeLog)
		case models.EventTypeMetric:
			if !hasMetrics {
				setAttributes(rsMetrics.Resource().Attributes(), meta)
				scopeMetric = pmetric.NewScopeMetrics()
				setScope(scopeMetric, groupTags)
				hasMetrics = true
			}
			err = ConvertPipelineEventToOtlpMetric(v, scopeMetric)
		case models.EventTypeSpan:
			if !hasTraces {
				setAttributes(rsTraces.Resource().Attributes(), meta)
				scopeTrace = ptrace.NewScopeSpans()
				setScope(scopeTrace, groupTags)
				hasTraces = true
			}
			err = ConvertPipelineEventToOtlpSpan(v, scopeTrace)
		}
	}

	if hasLogs {
		newScopeLogs := rsLogs.ScopeLogs().AppendEmpty()
		scopeLog.MoveTo(newScopeLogs)
	}

	if hasMetrics {
		newScopeMetric := rsMetrics.ScopeMetrics().AppendEmpty()
		scopeMetric.MoveTo(newScopeMetric)
	}

	if hasTraces {
		newScopeTrace := rsTraces.ScopeSpans().AppendEmpty()
		scopeTrace.MoveTo(newScopeTrace)
	}

	return err
}

func ConvertPipelineEventToOtlpLog(event models.PipelineEvent, scopeLog plog.ScopeLogs) (err error) {
	if event.GetType() != models.EventTypeLogging {
		return fmt.Errorf("pipeline_event:%s is not a log", event.GetName())
	}

	logEvent, ok := event.(*models.Log)

	if !ok {
		return fmt.Errorf("pipeline_event:%s is not a log", event.GetName())
	}

	log := scopeLog.LogRecords().AppendEmpty()
	setAttributes(log.Attributes(), logEvent.Tags)

	// set body as bytes
	log.Body().SetEmptyBytes().Append(logEvent.GetBody()...)
	log.SetTimestamp(pcommon.Timestamp(logEvent.Timestamp))
	log.SetObservedTimestamp(pcommon.Timestamp(logEvent.ObservedTimestamp))
	log.SetSeverityText(logEvent.Level)
	log.SetSeverityNumber(otlp.SeverityTextToSeverityNumber(logEvent.Level))

	if logEvent.Tags.Contains(otlp.TagKeyLogFlag) {
		if flag, errConvert := strconv.Atoi(logEvent.Tags.Get(otlp.TagKeyLogFlag)); errConvert == nil {
			log.SetFlags(plog.LogRecordFlags(flag))
		}
	}

	if traceID, errConvert := convertTraceID(logEvent.TraceID); errConvert == nil {
		log.SetTraceID(traceID)
	}

	if spanID, errConvert := convertSpanID(logEvent.SpanID); errConvert == nil {
		log.SetSpanID(spanID)
	}
	return err
}

func ConvertPipelineEventToOtlpMetric(event models.PipelineEvent, scopeMetric pmetric.ScopeMetrics) (err error) {
	if event.GetType() != models.EventTypeMetric {
		return fmt.Errorf("pipeline_event:%s is not a metric", event.GetName())
	}

	metricEvent, ok := event.(*models.Metric)
	if !ok {
		return fmt.Errorf("pipeline_event:%s is not a metric", event.GetName())
	}

	m := scopeMetric.Metrics().AppendEmpty()
	m.SetName(event.GetName())
	m.SetDescription(metricEvent.Description)
	m.SetUnit(metricEvent.Unit)

	switch metricEvent.MetricType {
	case models.MetricTypeUntyped:
		// skip untyped metrics
	case models.MetricTypeGauge:
		gauge := m.SetEmptyGauge()
		_, err = appgendNumberDatapoint(gauge, metricEvent)
	case models.MetricTypeCounter:
		sum := m.SetEmptySum()
		sum, err = appgendNumberDatapoint(sum, metricEvent)
		sum.SetAggregationTemporality(pmetric.AggregationTemporalityDelta)
	case models.MetricTypeRateCounter:
		sum := m.SetEmptySum()
		sum, err = appgendNumberDatapoint(sum, metricEvent)
		at := metricEvent.Tags.Get(otlp.TagKeyMetricAggregationTemporality)
		if at == pmetric.AggregationTemporalityDelta.String() {
			sum.SetAggregationTemporality(pmetric.AggregationTemporalityDelta)
		} else if at == pmetric.AggregationTemporalityCumulative.String() {
			sum.SetAggregationTemporality(pmetric.AggregationTemporalityCumulative)
		}
		if metricEvent.Tags.Get(otlp.TagKeyMetricIsMonotonic) == "true" {
			sum.SetIsMonotonic(true)
		}
	case models.MetricTypeMeter:
		// otlp does not support metric.
	case models.MetricTypeSummary:
		summary := m.SetEmptySummary()
		appgendSummaryDatapoint(summary, metricEvent)
	case models.MetricTypeHistogram:
		if metricEvent.Tags.Get(otlp.TagKeyMetricHistogramType) == pmetric.MetricTypeExponentialHistogram.String() {
			exponentialHistogram := m.SetEmptyExponentialHistogram()
			exponentialHistogram = appendExponentialHistogramDatapoint(exponentialHistogram, metricEvent)
			at := metricEvent.Tags.Get(otlp.TagKeyMetricAggregationTemporality)
			if at == pmetric.AggregationTemporalityDelta.String() {
				exponentialHistogram.SetAggregationTemporality(pmetric.AggregationTemporalityDelta)
			} else if at == pmetric.AggregationTemporalityCumulative.String() {
				exponentialHistogram.SetAggregationTemporality(pmetric.AggregationTemporalityCumulative)
			}
		} else {
			histogram := m.SetEmptyHistogram()
			appendHistogramDatapoint(histogram, metricEvent)
		}
	}

	return err
}

func ConvertPipelineEventToOtlpSpan(event models.PipelineEvent, scopeTrace ptrace.ScopeSpans) error {
	if event.GetType() != models.EventTypeSpan {
		return fmt.Errorf("pipeline_event:%s is not a span: %v", event.GetName(), event.GetType())
	}

	spanEvent, ok := event.(*models.Span)
	if !ok {
		return fmt.Errorf("pipeline_event:%s is not a span: %v", event.GetName(), event.GetType())
	}

	span := scopeTrace.Spans().AppendEmpty()
	span.SetName(event.GetName())
	span.SetKind(ptrace.SpanKind(spanEvent.GetKind()))

	if traceID, err := convertTraceID(spanEvent.TraceID); err == nil {
		span.SetTraceID(traceID)
	}

	if spanID, err := convertSpanID(spanEvent.SpanID); err == nil {
		span.SetSpanID(spanID)
	}

	if parentSpanID, err := convertSpanID(spanEvent.ParentSpanID); err == nil {
		span.SetParentSpanID(parentSpanID)
	}

	span.SetStartTimestamp(pcommon.Timestamp(spanEvent.StartTime))
	span.SetEndTimestamp(pcommon.Timestamp(spanEvent.EndTime))

	setAttributes(span.Attributes(), spanEvent.Tags)

	for _, v := range spanEvent.Events {
		otSpanEvent := span.Events().AppendEmpty()
		otSpanEvent.SetName(v.Name)
		otSpanEvent.SetTimestamp(pcommon.Timestamp(v.Timestamp))
		setAttributes(otSpanEvent.Attributes(), v.Tags)
	}

	for _, v := range spanEvent.Links {
		otSpanLink := span.Links().AppendEmpty()
		if traceID, err := convertTraceID(v.TraceID); err == nil {
			otSpanLink.SetTraceID(traceID)
		}

		if spanID, err := convertSpanID(v.SpanID); err == nil {
			otSpanLink.SetSpanID(spanID)
		}
		otSpanLink.TraceState().FromRaw(v.TraceState)
		setAttributes(otSpanLink.Attributes(), v.Tags)
	}

	span.Status().SetCode(ptrace.StatusCode(spanEvent.Status))
	if spanEvent.Tags.Contains(otlp.TagKeySpanStatusMessage) {
		span.Status().SetMessage(spanEvent.Tags.Get(otlp.TagKeySpanStatusMessage))
	}

	if droppedAttributesCount, err := strconv.Atoi(spanEvent.Tags.Get(otlp.TagKeySpanDroppedAttrsCount)); err == nil {
		span.SetDroppedAttributesCount(uint32(droppedAttributesCount))
	}

	if droppedEventsCount, err := strconv.Atoi(spanEvent.Tags.Get(otlp.TagKeySpanDroppedEventsCount)); err == nil {
		span.SetDroppedAttributesCount(uint32(droppedEventsCount))
	}

	if droppedLinksCount, err := strconv.Atoi(spanEvent.Tags.Get(otlp.TagKeySpanDroppedLinksCount)); err == nil {
		span.SetDroppedLinksCount(uint32(droppedLinksCount))
	}

	return nil
}

func setScope[T interface {
	Scope() pcommon.InstrumentationScope
}](t T, groupTags models.Tags) {
	scope := t.Scope()
	if groupTags.Contains(otlp.TagKeyScopeName) {
		scope.SetName(groupTags.Get(otlp.TagKeyScopeName))
	}

	if groupTags.Contains(otlp.TagKeyScopeVersion) {
		scope.SetVersion(groupTags.Get(otlp.TagKeyScopeVersion))
	}
	scopeDroppedAttributesCount, err := strconv.Atoi(groupTags.Get(otlp.TagKeyScopeDroppedAttributesCount))
	if err == nil {
		scope.SetDroppedAttributesCount(uint32(scopeDroppedAttributesCount))
	}
	setAttributes(scope.Attributes(), groupTags)
}

func appgendNumberDatapoint[T interface {
	DataPoints() pmetric.NumberDataPointSlice
}](t T, metricEvent *models.Metric) (T, error) {
	datapoint := t.DataPoints().AppendEmpty()
	datapoint = setDatapoint(datapoint, metricEvent)
	datapoint.SetDoubleValue(metricEvent.GetValue().GetSingleValue())
	return t, nil
}

func appgendSummaryDatapoint(summary pmetric.Summary, metricEvent *models.Metric) {
	datapoint := summary.DataPoints().AppendEmpty()
	datapoint = setDatapoint(datapoint, metricEvent)

	multiValues := metricEvent.GetValue().GetMultiValues()
	datapoint.SetSum(multiValues.Get(otlp.FieldSum))
	datapoint.SetCount(uint64(multiValues.Get(otlp.FieldCount)))
	for field, value := range multiValues.Iterator() {
		if !otlp.IsInternalField(field) {
			quantileValue := datapoint.QuantileValues().AppendEmpty()
			quantile, err := strconv.ParseFloat(field, 64)
			if err != nil {
				continue
			}
			quantileValue.SetQuantile(quantile)
			quantileValue.SetValue(value)
		}
	}
}

func appendHistogramDatapoint(histogram pmetric.Histogram, metricEvent *models.Metric) {
	datapoint := histogram.DataPoints().AppendEmpty()
	datapoint = setDatapoint(datapoint, metricEvent)

	multiValues := metricEvent.GetValue().GetMultiValues()
	datapoint.SetCount(uint64(multiValues.Get(otlp.FieldCount)))
	if multiValues.Contains(otlp.FieldMin) {
		datapoint.SetMin(multiValues.Get(otlp.FieldMin))
	}

	if multiValues.Contains(otlp.FieldMax) {
		datapoint.SetMax(multiValues.Get(otlp.FieldMax))
	}

	if multiValues.Contains(otlp.FieldSum) {
		datapoint.SetSum(multiValues.Get(otlp.FieldSum))
	}

	bucketBounds, bucketCounts := otlp.ComputeBuckets(multiValues, true)

	if len(bucketCounts) >= 1 {
		datapoint.ExplicitBounds().FromRaw(bucketBounds[1:])
		for _, v := range bucketCounts {
			datapoint.BucketCounts().Append(uint64(v))
		}
	}
}

func appendExponentialHistogramDatapoint(histogram pmetric.ExponentialHistogram, metricEvent *models.Metric) pmetric.ExponentialHistogram {
	datapoint := histogram.DataPoints().AppendEmpty()
	datapoint = setDatapoint(datapoint, metricEvent)

	multiValues := metricEvent.GetValue().GetMultiValues()
	datapoint.SetCount(uint64(multiValues.Get(otlp.FieldCount)))
	if multiValues.Contains(otlp.FieldMin) {
		datapoint.SetMin(multiValues.Get(otlp.FieldMin))
	}

	if multiValues.Contains(otlp.FieldMax) {
		datapoint.SetMax(multiValues.Get(otlp.FieldMax))
	}

	if multiValues.Contains(otlp.FieldSum) {
		datapoint.SetSum(multiValues.Get(otlp.FieldSum))
	}

	scale := int32(multiValues.Get(otlp.FieldScale))
	datapoint.SetScale(scale)

	postiveOffset := int32(multiValues.Get(otlp.FieldPositiveOffset))
	datapoint.Positive().SetOffset(postiveOffset)
	_, positveBucketCounts := otlp.ComputeBuckets(multiValues, true)
	for _, v := range positveBucketCounts {
		datapoint.Positive().BucketCounts().Append(uint64(v))
	}

	negativeOffset := int32(multiValues.Get(otlp.FieldNegativeOffset))
	datapoint.Negative().SetOffset(negativeOffset)
	_, negativeBucketCounts := otlp.ComputeBuckets(multiValues, false)
	for _, v := range negativeBucketCounts {
		datapoint.Negative().BucketCounts().Append(uint64(v))
	}

	return histogram
}

func setDatapoint[T interface {
	SetTimestamp(v pcommon.Timestamp)
	SetStartTimestamp(v pcommon.Timestamp)
	Attributes() pcommon.Map
}](t T, metricEvent *models.Metric) T {
	t.SetTimestamp(pcommon.Timestamp(metricEvent.Timestamp))
	t.SetStartTimestamp(pcommon.Timestamp(metricEvent.ObservedTimestamp))
	setAttributes(t.Attributes(), metricEvent.Tags)
	return t
}

func setAttributes[
	T interface {
		Iterator() map[string]string
	},
](attributes pcommon.Map, tags T) {
	for k, v := range tags.Iterator() {
		if !otlp.IsInternalTag(k) {
			attributes.PutStr(k, v)
		}
	}
}

func convertTraceID(hexString string) (pcommon.TraceID, error) {
	var id pcommon.TraceID = pcommon.NewTraceIDEmpty()

	if hexString == "" {
		return id, nil
	}

	bytes, err := hex.DecodeString(hexString)
	if err != nil {
		return id, err
	}

	if len(bytes) != 16 {
		return id, fmt.Errorf("%w: should be %d, but is %d after traceID: %s is decoded", errBytesLengthNotMatch, 16, len(bytes), hexString)
	}

	return pcommon.TraceID(*(*[16]byte)(bytes)), nil
}

func convertSpanID(hexString string) (pcommon.SpanID, error) {
	id := pcommon.NewSpanIDEmpty()
	if hexString == "" {
		return id, nil
	}

	bytes, err := hex.DecodeString(hexString)
	if err != nil {
		return id, err
	}

	if len(bytes) != 8 {
		return id, fmt.Errorf("%w: should be %d, but is %d after traceID: %s is decoded", errBytesLengthNotMatch, 8, len(bytes), hexString)
	}
	return pcommon.SpanID(*(*[8]byte)(bytes)), nil
}

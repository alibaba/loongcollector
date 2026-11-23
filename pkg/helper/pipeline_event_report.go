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

package helper

import (
	"encoding/json"
	"fmt"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

// ForEachPipelineEventReport encodes contiguous runs of the same event kind,
// retaining input order and the distinction between event and group tags.
func ForEachPipelineEventReport(input *models.PipelineGroupEvents, visit func(*protocol.PipelineEventGroupReport) error) error {
	if input == nil {
		return nil
	}
	for start := 0; start < len(input.Events); {
		first := input.Events[start]
		if first == nil {
			return fmt.Errorf("nil pipeline event at index %d", start)
		}
		end := start + 1
		for end < len(input.Events) && input.Events[end] != nil && input.Events[end].GetType() == first.GetType() {
			end++
		}
		report, err := encodePipelineEventReport(input.Group, input.Events[start:end])
		if err != nil {
			return err
		}
		if err := visit(report); err != nil {
			return err
		}
		start = end
	}
	return nil
}

func encodePipelineEventReport(info *models.GroupInfo, events []models.PipelineEvent) (*protocol.PipelineEventGroupReport, error) {
	group := &protocol.PipelineEventGroup{
		Metadata: reportBytes(info.GetMetadata()),
		Tags:     reportBytes(info.GetTags()),
	}
	report := &protocol.PipelineEventGroupReport{EventGroup: group, DestinationLogstore: info.GetDestinationLogstore()}
	switch events[0].GetType() {
	case models.EventTypeLogging:
		logs := &protocol.PipelineEventGroup_LogEvents{Events: make([]*protocol.LogEvent, 0, len(events))}
		group.PipelineEvents = &protocol.PipelineEventGroup_Logs{Logs: logs}
		for _, event := range events {
			log, ok := event.(*models.Log)
			if !ok || log == nil {
				return nil, fmt.Errorf("invalid log event %T", event)
			}
			wire, attributes := encodeReportLog(log)
			logs.Events = append(logs.Events, wire)
			report.LogAttributes = append(report.LogAttributes, attributes)
		}
	case models.EventTypeMetric:
		metrics := &protocol.PipelineEventGroup_MetricEvents{Events: make([]*protocol.MetricEvent, 0, len(events))}
		group.PipelineEvents = &protocol.PipelineEventGroup_Metrics{Metrics: metrics}
		for _, event := range events {
			metric, ok := event.(*models.Metric)
			if !ok || metric == nil {
				return nil, fmt.Errorf("invalid metric event %T", event)
			}
			wire, attributes, err := encodeReportMetric(metric)
			if err != nil {
				return nil, err
			}
			metrics.Events = append(metrics.Events, wire)
			report.MetricAttributes = append(report.MetricAttributes, attributes)
		}
	case models.EventTypeSpan:
		spans := &protocol.PipelineEventGroup_SpanEvents{Events: make([]*protocol.SpanEvent, 0, len(events))}
		group.PipelineEvents = &protocol.PipelineEventGroup_Spans{Spans: spans}
		for _, event := range events {
			span, ok := event.(*models.Span)
			if !ok || span == nil {
				return nil, fmt.Errorf("invalid span event %T", event)
			}
			wire, err := TransferSpanEventToPB(span)
			if err != nil {
				return nil, err
			}
			spans.Events = append(spans.Events, wire)
		}
	case models.EventTypeByteArray:
		for _, event := range events {
			raw, ok := event.(models.ByteArray)
			if !ok {
				return nil, fmt.Errorf("invalid raw event %T", event)
			}
			report.RawEvents = append(report.RawEvents, []byte(raw))
		}
	default:
		return nil, fmt.Errorf("unsupported pipeline event type %v", events[0].GetType())
	}
	for i, event := range events {
		if observed := event.GetObservedTimestamp(); observed != 0 {
			if report.ObservedTimestamps == nil {
				report.ObservedTimestamps = make([]uint64, len(events))
			}
			report.ObservedTimestamps[i] = observed
		}
	}
	return report, nil
}

func reportBytes(values models.KeyValues[string]) map[string][]byte {
	if values == nil || values.Len() == 0 {
		return nil
	}
	result := make(map[string][]byte, values.Len())
	for key, value := range values.Iterator() {
		result[key] = util.ZeroCopyStringToBytes(value)
	}
	return result
}

func encodeReportLog(log *models.Log) (*protocol.LogEvent, *protocol.ReportLogAttributes) {
	fields := log.GetIndices()
	wire := &protocol.LogEvent{
		Timestamp:  log.Timestamp,
		Level:      util.ZeroCopyStringToBytes(log.Level),
		FileOffset: log.Offset,
		RawSize:    log.RawSize,
		Contents:   make([]*protocol.LogEvent_Content, 0, fields.Len()),
	}
	attributes := &protocol.ReportLogAttributes{
		Tags: reportBytes(log.GetTags()), Name: log.Name, TraceId: log.TraceID, SpanId: log.SpanID,
	}
	for key, value := range models.LogContentEntries(fields) {
		var encoded []byte
		if raw, ok := value.([]byte); ok {
			attributes.ByteFieldIndices = append(attributes.ByteFieldIndices, uint32(len(wire.Contents)))
			encoded = raw
		} else {
			encoded = util.ZeroCopyStringToBytes(pipeline.GetStringValue(value))
		}
		wire.Contents = append(wire.Contents, &protocol.LogEvent_Content{Key: util.ZeroCopyStringToBytes(key), Value: encoded})
	}
	return wire, attributes
}

func encodeReportMetric(metric *models.Metric) (*protocol.MetricEvent, *protocol.ReportMetricAttributes, error) {
	var wire *protocol.MetricEvent
	if value := metric.GetValue(); value.IsSingleValue() || value.IsMultiValues() {
		// The shared PEG converter expects initialized tags.
		withTags := *metric
		if withTags.Tags == nil {
			withTags.Tags = models.NilStringValues
		}
		var err error
		wire, err = TransferMetricEventToPB(&withTags)
		if err != nil {
			return nil, nil, err
		}
	} else {
		wire = &protocol.MetricEvent{Timestamp: metric.Timestamp, Name: []byte(metric.Name), Tags: reportBytes(metric.Tags)}
	}
	attributes := &protocol.ReportMetricAttributes{
		MetricType: models.MetricTypeTexts[metric.MetricType], Unit: metric.Unit, Description: metric.Description,
	}
	if fields := metric.GetTypedValue(); fields.Len() > 0 {
		attributes.TypedValuesJson = make(map[string][]byte, fields.Len())
		for key, value := range fields.Iterator() {
			if value == nil {
				return nil, nil, fmt.Errorf("nil typed metric field %q", key)
			}
			encoded, err := json.Marshal(value.Value)
			if err != nil {
				return nil, nil, fmt.Errorf("encode metric field %q: %w", key, err)
			}
			switch encoded[0] {
			case '"', 't', 'f', '[', '{':
				attributes.TypedValuesJson[key] = encoded
			default:
				return nil, nil, fmt.Errorf("unsupported typed metric field %q of type %T", key, value.Value)
			}
		}
	}
	return wire, attributes, nil
}

// DecodePipelineEventReport restores the original event kind, group metadata
// and tags, destination, and the per-event report attributes.
func DecodePipelineEventReport(report *protocol.PipelineEventGroupReport) (*models.PipelineGroupEvents, error) {
	if report == nil || report.EventGroup == nil {
		return nil, fmt.Errorf("missing event group")
	}
	group := report.EventGroup
	info := &models.GroupInfo{
		Metadata: reportStrings(group.Metadata), Tags: reportStrings(group.Tags), DestinationLogstore: report.DestinationLogstore,
	}
	if len(report.RawEvents) > 0 && group.PipelineEvents != nil {
		return nil, fmt.Errorf("raw events and a typed event group in the same report")
	}
	var events []models.PipelineEvent
	var err error
	switch typed := group.PipelineEvents.(type) {
	case *protocol.PipelineEventGroup_Logs:
		if typed.Logs == nil {
			return nil, fmt.Errorf("missing log events")
		}
		events, err = decodeReportLogs(typed.Logs.Events, report.LogAttributes)
	case *protocol.PipelineEventGroup_Metrics:
		if typed.Metrics == nil {
			return nil, fmt.Errorf("missing metric events")
		}
		events, err = decodeReportMetrics(typed.Metrics.Events, report.MetricAttributes)
	case *protocol.PipelineEventGroup_Spans:
		if typed.Spans == nil {
			return nil, fmt.Errorf("missing span events")
		}
		for _, wire := range typed.Spans.Events {
			span, convertErr := TransferPBToSpanEvent(wire)
			if convertErr != nil {
				return nil, convertErr
			}
			// Observed timestamps are explicitly carried in the report.
			span.ObservedTimestamp = 0
			events = append(events, span)
		}
	case nil:
		for _, raw := range report.RawEvents {
			events = append(events, models.ByteArray(raw))
		}
	}
	if err != nil {
		return nil, err
	}
	if len(events) == 0 {
		return nil, fmt.Errorf("empty event report")
	}
	if observed := report.ObservedTimestamps; len(observed) != 0 {
		if len(observed) != len(events) {
			return nil, fmt.Errorf("observed timestamps do not match event count")
		}
		for i, event := range events {
			event.SetObservedTimestamp(observed[i])
		}
	}
	return &models.PipelineGroupEvents{Group: info, Events: events}, nil
}

func reportStrings(values map[string][]byte) models.KeyValues[string] {
	result := models.NewKeyValues[string]()
	for key, value := range values {
		result.Add(key, string(value))
	}
	return result
}

func decodeReportLogs(logs []*protocol.LogEvent, attributes []*protocol.ReportLogAttributes) ([]models.PipelineEvent, error) {
	if len(attributes) != 0 && len(attributes) != len(logs) {
		return nil, fmt.Errorf("log attributes do not match event count")
	}
	events := make([]models.PipelineEvent, 0, len(logs))
	for i, wire := range logs {
		log, err := TransferPBToLogEvent(wire)
		if err != nil {
			return nil, err
		}
		if len(attributes) > 0 && attributes[i] != nil {
			meta := attributes[i]
			log.Tags = reportStrings(meta.Tags)
			log.Name, log.TraceID, log.SpanID = meta.Name, meta.TraceId, meta.SpanId
			// The PEG defaults "content" to bytes. The report also carries the
			// actual byte positions, including when "content" is a string.
			if body, ok := log.GetIndices().Get(models.BodyKey).([]byte); ok {
				log.GetIndices().Add(models.BodyKey, string(body))
			}
			for _, index := range meta.ByteFieldIndices {
				if uint64(index) >= uint64(len(wire.Contents)) {
					return nil, fmt.Errorf("invalid log byte field index %d", index)
				}
				field := wire.Contents[index]
				log.GetIndices().Add(string(field.Key), field.Value)
			}
		}
		events = append(events, log)
	}
	return events, nil
}

func decodeReportMetrics(metrics []*protocol.MetricEvent, attributes []*protocol.ReportMetricAttributes) ([]models.PipelineEvent, error) {
	if len(attributes) != 0 && len(attributes) != len(metrics) {
		return nil, fmt.Errorf("metric attributes do not match event count")
	}
	events := make([]models.PipelineEvent, 0, len(metrics))
	for i, wire := range metrics {
		if wire == nil {
			return nil, fmt.Errorf("nil metric event")
		}
		var metric *models.Metric
		if wire.Value == nil {
			metric = &models.Metric{
				Timestamp: wire.Timestamp, Name: string(wire.Name), Tags: reportStrings(wire.Tags),
			}
		} else {
			var err error
			metric, err = TransferPBToMetricEvent(wire)
			if err != nil {
				return nil, err
			}
		}
		if len(attributes) > 0 && attributes[i] != nil {
			meta := attributes[i]
			if kind, ok := models.MetricTypeValues[meta.MetricType]; ok {
				metric.MetricType = kind
			}
			metric.Unit, metric.Description = meta.Unit, meta.Description
			if len(meta.TypedValuesJson) > 0 {
				metric.TypedValue = models.NewMetricTypedValues()
				for key, raw := range meta.TypedValuesJson {
					value, err := decodeReportMetricValue(raw)
					if err != nil {
						return nil, fmt.Errorf("decode metric field %q: %w", key, err)
					}
					metric.TypedValue.Add(key, value)
				}
			}
		}
		events = append(events, metric)
	}
	return events, nil
}

func decodeReportMetricValue(raw []byte) (*models.TypedValue, error) {
	var value any
	if err := json.Unmarshal(raw, &value); err != nil {
		return nil, err
	}
	field := &models.TypedValue{Value: value}
	switch value.(type) {
	case string:
		field.Type = models.ValueTypeString
	case bool:
		field.Type = models.ValueTypeBoolean
	case []any:
		field.Type = models.ValueTypeArray
	case map[string]any:
		field.Type = models.ValueTypeMap
	default:
		return nil, fmt.Errorf("unsupported typed metric value %T", value)
	}
	return field, nil
}

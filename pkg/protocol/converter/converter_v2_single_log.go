// Copyright 2026 iLogtail Authors
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
	"fmt"
	"sort"
	"strconv"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

// byteArrayContentKey carries the raw ByteArray payload, which has no
// structured fields of its own, so it is never silently dropped.
const byteArrayContentKey = "content"

// protocolRecord is one flushable record extracted from a pipeline event. A Log
// maps to a single record; a Metric may expand into several records (one per
// value/typed-value) so every value is flushed as a first-class field.
type protocolRecord struct {
	contents map[string]string
	tags     map[string]string
	timeSec  uint32
}

func (c *Converter) ConvertToSingleProtocolStreamV2(groupEvents *models.PipelineGroupEvents, targetFields []string) ([][]byte, []map[string]string, error) {
	if groupEvents == nil || len(groupEvents.Events) == 0 {
		return nil, nil, nil
	}

	marshaledLogs := make([][]byte, 0, len(groupEvents.Events))
	desiredValues := make([]map[string]string, 0, len(groupEvents.Events))
	for _, event := range groupEvents.Events {
		records, err := c.pipelineEventToRecords(event, groupEvents.Group)
		if err != nil {
			if c.IgnoreUnExpectedData {
				continue
			}
			return nil, nil, err
		}
		for _, record := range records {
			desiredValue, err := findTargetValues(targetFields, record.contents, record.tags, c.TagKeyRenameMap)
			if err != nil {
				return nil, nil, err
			}
			b, err := marshalSingleProtocolEntry(c.buildSingleProtocolEntry(record))
			if err != nil {
				return nil, nil, err
			}
			marshaledLogs = append(marshaledLogs, b)
			desiredValues = append(desiredValues, desiredValue)
		}
	}
	return marshaledLogs, desiredValues, nil
}

func (c *Converter) ConvertToSingleProtocolStreamFlattenV2(groupEvents *models.PipelineGroupEvents, targetFields []string) ([][]byte, []map[string]string, error) {
	if groupEvents == nil || len(groupEvents.Events) == 0 {
		return nil, nil, nil
	}

	marshaledLogs := make([][]byte, 0, len(groupEvents.Events))
	desiredValues := make([]map[string]string, 0, len(groupEvents.Events))
	for _, event := range groupEvents.Events {
		records, err := c.pipelineEventToRecords(event, groupEvents.Group)
		if err != nil {
			if c.IgnoreUnExpectedData {
				continue
			}
			return nil, nil, err
		}
		for _, record := range records {
			desiredValue, err := findTargetValues(targetFields, record.contents, record.tags, c.TagKeyRenameMap)
			if err != nil {
				return nil, nil, err
			}
			b, err := marshalSingleProtocolEntry(c.buildFlattenProtocolEntry(record))
			if err != nil {
				return nil, nil, err
			}
			marshaledLogs = append(marshaledLogs, b)
			desiredValues = append(desiredValues, desiredValue)
		}
	}
	return marshaledLogs, desiredValues, nil
}

func (c *Converter) ConvertToJsonlineProtocolStreamV2(groupEvents *models.PipelineGroupEvents) ([]byte, []map[string]string, error) {
	convertedLogs, _, err := c.ConvertToSingleProtocolStreamFlattenV2(groupEvents, nil)
	if err != nil {
		return nil, nil, err
	}
	if len(convertedLogs) == 0 {
		return nil, nil, nil
	}

	joinedStream := *GetPooledByteBuf()
	for i, logBytes := range convertedLogs {
		if i > 0 {
			joinedStream = append(joinedStream, '\n')
		}
		joinedStream = append(joinedStream, logBytes...)
	}
	return joinedStream, nil, nil
}

// pipelineEventToRecords converts one pipeline event into flushable records.
// Log events keep their structured fields; Metric/Span/ByteArray events are
// converted into structured logs (mirroring v1's representation) instead of
// being wrapped in an opaque pass-through blob.
func (c *Converter) pipelineEventToRecords(event models.PipelineEvent, group *models.GroupInfo) ([]protocolRecord, error) {
	if pipeline.LogOnlyEventKinds.Supports(event.GetType()) {
		log, ok := event.(*models.Log)
		if !ok {
			return nil, fmt.Errorf("expected log event, got %T", event)
		}
		contents, tags := convertPipelineLogToMap(log, group, c.TagKeyRenameMap)
		return []protocolRecord{{
			contents: contents,
			tags:     tags,
			timeSec:  uint32(log.GetTimestamp() / 1e9),
		}}, nil
	}

	logs, err := convertNonLogEventToLogs(event)
	if err != nil {
		return nil, err
	}
	// Non-log events carry their own dimensions inside the converted contents
	// (metric labels in __labels__, span/byteArray fields inline). Their
	// event-level tags plus the group tags are also surfaced as record tags so
	// that DynamicLabels selection (e.g. a loki stream label built from a metric
	// dimension) resolves the same way it does for a v1 log tag. Without this,
	// a metric dimension only lives in __labels__ and DynamicLabels resolves to
	// an empty value, mislabeling the flushed loki stream so it is never queried.
	tags := collectGroupTags(group, c.TagKeyRenameMap)
	for k, v := range event.GetTags().Iterator() {
		addTagIfRequired(tags, c.TagKeyRenameMap, k, v)
	}
	records := make([]protocolRecord, 0, len(logs))
	for _, log := range logs {
		records = append(records, protocolRecord{
			contents: logContentsToMap(log),
			tags:     tags,
			timeSec:  log.GetTime(),
		})
	}
	return records, nil
}

func (c *Converter) buildSingleProtocolEntry(record protocolRecord) map[string]interface{} {
	entry := make(map[string]interface{}, numProtocolKeys)
	if newKey, ok := c.ProtocolKeyRenameMap[protocolKeyTime]; ok {
		entry[newKey] = record.timeSec
	} else {
		entry[protocolKeyTime] = record.timeSec
	}
	if newKey, ok := c.ProtocolKeyRenameMap[protocolKeyContent]; ok {
		entry[newKey] = record.contents
	} else {
		entry[protocolKeyContent] = record.contents
	}
	if newKey, ok := c.ProtocolKeyRenameMap[protocolKeyTag]; ok {
		entry[newKey] = record.tags
	} else {
		entry[protocolKeyTag] = record.tags
	}
	return entry
}

func (c *Converter) buildFlattenProtocolEntry(record protocolRecord) map[string]interface{} {
	entry := make(map[string]interface{}, 1+len(record.contents)+len(record.tags))
	for k, v := range record.contents {
		entry[k] = v
	}
	if !c.OnlyContents {
		for k, v := range record.tags {
			entry[k] = v
		}
	}
	if newKey, ok := c.ProtocolKeyRenameMap[protocolKeyTime]; ok {
		entry[newKey] = record.timeSec
	} else {
		entry[protocolKeyTime] = record.timeSec
	}
	return entry
}

func convertPipelineLogToMap(log *models.Log, group *models.GroupInfo, tagKeyRenameMap map[string]string) (map[string]string, map[string]string) {
	contents, tags := make(map[string]string), make(map[string]string)
	for k, v := range log.GetIndices().Iterator() {
		contents[k] = interfaceToString(v)
	}
	for k, v := range log.GetTags().Iterator() {
		addTagIfRequired(tags, tagKeyRenameMap, k, v)
	}
	if group != nil {
		for k, v := range group.GetTags().Iterator() {
			addTagIfRequired(tags, tagKeyRenameMap, k, v)
		}
	}
	return contents, tags
}

func collectGroupTags(group *models.GroupInfo, tagKeyRenameMap map[string]string) map[string]string {
	tags := make(map[string]string)
	if group == nil {
		return tags
	}
	for k, v := range group.GetTags().Iterator() {
		addTagIfRequired(tags, tagKeyRenameMap, k, v)
	}
	return tags
}

func logContentsToMap(log *protocol.Log) map[string]string {
	contents := make(map[string]string, len(log.Contents))
	for _, content := range log.Contents {
		contents[content.GetKey()] = content.GetValue()
	}
	return contents
}

func interfaceToString(v interface{}) string {
	switch tv := v.(type) {
	case string:
		return tv
	case []byte:
		return string(tv)
	default:
		return fmt.Sprint(tv)
	}
}

func marshalSingleProtocolEntry(entry map[string]interface{}) ([]byte, error) {
	return marshalWithoutHTMLEscaped(entry)
}

// PipelineGroupEventsToLogGroup converts v2 pipeline events into a legacy LogGroup for v1-only flush paths.
func PipelineGroupEventsToLogGroup(groupEvents *models.PipelineGroupEvents) (*protocol.LogGroup, error) {
	if groupEvents == nil || len(groupEvents.Events) == 0 {
		return nil, nil
	}

	logGroup := &protocol.LogGroup{
		Logs: make([]*protocol.Log, 0, len(groupEvents.Events)),
	}
	if groupEvents.Group != nil {
		logGroup.LogTags = make([]*protocol.LogTag, 0, groupEvents.Group.GetTags().Len())
		for k, v := range groupEvents.Group.GetTags().Iterator() {
			logGroup.LogTags = append(logGroup.LogTags, &protocol.LogTag{Key: k, Value: v})
		}
	}

	for _, event := range groupEvents.Events {
		if pipeline.LogOnlyEventKinds.Supports(event.GetType()) {
			log, ok := event.(*models.Log)
			if !ok {
				return nil, fmt.Errorf("expected log event, got %T", event)
			}
			pbLog, err := helper.TransferLogEventToPB(log)
			if err != nil {
				return nil, err
			}
			legacyLog, err := helper.TransferPBLogEventToLegacyLog(pbLog)
			if err != nil {
				return nil, err
			}
			logGroup.Logs = append(logGroup.Logs, legacyLog)
			continue
		}
		// Metric/Span/ByteArray are converted structurally so they are flushed
		// as first-class fields and never silently dropped.
		logs, err := convertNonLogEventToLogs(event)
		if err != nil {
			return nil, err
		}
		logGroup.Logs = append(logGroup.Logs, logs...)
	}
	return logGroup, nil
}

// convertNonLogEventToLogs converts a non-Log pipeline event (Metric/Span/
// ByteArray) into one or more structured protocol logs. Metric events reuse
// v1's canonical metric-log format (__name__/__labels__/__value__/__time_nano__)
// via helper.NewMetricLog so that v2 supports Metric the same way v1 does.
func convertNonLogEventToLogs(event models.PipelineEvent) ([]*protocol.Log, error) {
	switch event.GetType() {
	case models.EventTypeMetric:
		metric, ok := event.(*models.Metric)
		if !ok {
			return nil, fmt.Errorf("expected metric event, got %T", event)
		}
		return convertPipelineMetricToLogs(metric), nil
	case models.EventTypeSpan:
		span, ok := event.(*models.Span)
		if !ok {
			return nil, fmt.Errorf("expected span event, got %T", event)
		}
		return []*protocol.Log{convertPipelineSpanToLog(span)}, nil
	case models.EventTypeByteArray:
		return []*protocol.Log{convertPipelineByteArrayToLog(event)}, nil
	default:
		return nil, fmt.Errorf("unsupported event type: %v", event.GetType())
	}
}

func convertPipelineMetricToLogs(metric *models.Metric) []*protocol.Log {
	labels := &helper.MetricLabels{}
	for k, v := range metric.GetTags().Iterator() {
		labels.Append(k, v)
	}

	name := metric.GetName()
	t := int64(metric.GetTimestamp())
	logs := make([]*protocol.Log, 0, 1)

	if value := metric.GetValue(); value != nil {
		if value.IsSingleValue() {
			logs = append(logs, helper.NewMetricLog(name, t, value.GetSingleValue(), labels))
		} else if value.IsMultiValues() {
			multiValues := value.GetMultiValues().Iterator()
			for _, field := range sortedFloatKeys(multiValues) {
				logs = append(logs, helper.NewMetricLog(name+"_"+field, t, multiValues[field], labels))
			}
		}
	}

	if typedValues := metric.GetTypedValue(); typedValues.Len() > 0 {
		iter := typedValues.Iterator()
		for _, field := range sortedTypedKeys(iter) {
			logs = append(logs, helper.NewMetricLogStringVal(name+"_"+field, t, typedValueToString(iter[field]), labels))
		}
	}

	// A metric with neither value nor typed value still emits its name so it is
	// never silently dropped.
	if len(logs) == 0 {
		logs = append(logs, helper.NewMetricLogStringVal(name, t, "", labels))
	}
	return logs
}

func convertPipelineSpanToLog(span *models.Span) *protocol.Log {
	log := &protocol.Log{
		Time: uint32(span.GetTimestamp() / 1e9),
		Contents: []*protocol.Log_Content{
			{Key: "traceID", Value: span.GetTraceID()},
			{Key: "spanID", Value: span.GetSpanID()},
			{Key: "parentSpanID", Value: span.GetParentSpanID()},
			{Key: "name", Value: span.GetName()},
			{Key: "kind", Value: spanKindText(span.GetKind())},
			{Key: "startTime", Value: strconv.FormatUint(span.GetStartTime(), 10)},
			{Key: "endTime", Value: strconv.FormatUint(span.GetEndTime(), 10)},
			{Key: "statusCode", Value: strconv.FormatInt(int64(span.GetStatus()), 10)},
		},
	}
	for _, k := range sortedStringKeys(span.GetTags().Iterator()) {
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: k, Value: span.GetTags().Iterator()[k]})
	}
	return log
}

func convertPipelineByteArrayToLog(event models.PipelineEvent) *protocol.Log {
	log := &protocol.Log{
		Time: uint32(event.GetTimestamp() / 1e9),
		Contents: []*protocol.Log_Content{
			{Key: byteArrayContentKey, Value: string(event.(models.ByteArray))},
		},
	}
	for _, k := range sortedStringKeys(event.GetTags().Iterator()) {
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: k, Value: event.GetTags().Iterator()[k]})
	}
	return log
}

func typedValueToString(tv *models.TypedValue) string {
	if tv == nil {
		return ""
	}
	return fmt.Sprint(tv.Value)
}

func sortedStringKeys(m map[string]string) []string {
	keys := make([]string, 0, len(m))
	for k := range m {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	return keys
}

func sortedFloatKeys(m map[string]float64) []string {
	keys := make([]string, 0, len(m))
	for k := range m {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	return keys
}

func sortedTypedKeys(m map[string]*models.TypedValue) []string {
	keys := make([]string, 0, len(m))
	for k := range m {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	return keys
}

func spanKindText(kind models.SpanKind) string {
	switch kind {
	case models.SpanKindInternal:
		return models.SpanKindTextInternal
	case models.SpanKindServer:
		return models.SpanKindTextServer
	case models.SpanKindClient:
		return models.SpanKindTextClient
	case models.SpanKindProducer:
		return models.SpanKindTextProducer
	case models.SpanKindConsumer:
		return models.SpanKindTextConsumer
	default:
		return models.SpanKindTextInternal
	}
}

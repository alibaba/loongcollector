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

func (c *Converter) ConvertToSingleProtocolStream(groupEvents *models.PipelineGroupEvents, targetFields []string) ([][]byte, []map[string]string, error) {
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

func (c *Converter) ConvertToSingleProtocolStreamFlatten(groupEvents *models.PipelineGroupEvents, targetFields []string) ([][]byte, []map[string]string, error) {
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

func (c *Converter) ConvertToJSONLineStream(groupEvents *models.PipelineGroupEvents) ([]byte, []map[string]string, error) {
	convertedLogs, _, err := c.ConvertToSingleProtocolStreamFlatten(groupEvents, nil)
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
// Log events retain their fields. Metric, Span, and ByteArray events are
// mapped to the destination's record fields.
func (c *Converter) pipelineEventToRecords(event models.PipelineEvent, group *models.GroupInfo) ([]protocolRecord, error) {
	if log, ok := event.(*models.Log); ok && log != nil {
		contents, tags := convertPipelineLogToMap(log, group, c.TagKeyRenameMap)
		return []protocolRecord{{
			contents: contents,
			tags:     tags,
			timeSec:  uint32(log.GetTimestamp() / 1e9),
		}}, nil
	}

	rows, err := encodeNonLogRecords(event)
	if err != nil {
		return nil, err
	}
	// Non-log events carry their own dimensions inside the converted contents
	// (metric labels in __labels__, span/byteArray fields inline). Their
	// event-level tags plus the group tags are also surfaced as record tags so
	// that DynamicLabels selection can use dimensions such as metric labels
	// when assigning Loki stream labels.
	tags := collectGroupTags(group, c.TagKeyRenameMap)
	for k, v := range event.GetTags().Iterator() {
		addTagIfRequired(tags, c.TagKeyRenameMap, k, v)
	}
	records := make([]protocolRecord, 0, len(rows))
	for _, row := range rows {
		records = append(records, protocolRecord{
			contents: row.Fields,
			tags:     tags,
			timeSec:  uint32(row.Timestamp / 1e9),
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

// PipelineGroupEventsToLogGroups serializes native events using the existing
// LogGroup wire schema. Per-log tags stay in LogTags; distinct tag sets are
// emitted in separate groups rather than flattened into log contents.
func PipelineGroupEventsToLogGroups(events *models.PipelineGroupEvents) ([]*protocol.LogGroup, error) {
	var groups []*protocol.LogGroup
	err := helper.ForEachLogTagGroup(events, func(group *models.PipelineGroupEvents) error {
		converted, err := pipelineGroupEventsToLogGroup(group)
		if err != nil {
			return err
		}
		groups = append(groups, converted)
		return nil
	})
	return groups, err
}

func pipelineGroupEventsToLogGroup(groupEvents *models.PipelineGroupEvents) (*protocol.LogGroup, error) {
	logGroup := &protocol.LogGroup{
		Category: groupEvents.Group.GetDestinationLogstore(),
		Logs:     make([]*protocol.Log, 0, len(groupEvents.Events)),
	}
	for k, v := range groupEvents.Group.GetTags().Iterator() {
		if k == pipeline.TopicTagKey {
			logGroup.Topic = v
			continue
		}
		logGroup.LogTags = append(logGroup.LogTags, &protocol.LogTag{Key: k, Value: v})
	}
	for _, event := range groupEvents.Events {
		if log, ok := event.(*models.Log); ok && log != nil {
			wireLog, err := newWireLog(log)
			if err != nil {
				return nil, err
			}
			logGroup.Logs = append(logGroup.Logs, wireLog)
			continue
		}
		rows, err := encodeNonLogRecords(event)
		if err != nil {
			return nil, err
		}
		for _, row := range rows {
			log := &protocol.Log{Contents: make([]*protocol.Log_Content, 0, len(row.Fields))}
			setWireTimestamp(log, row.Timestamp)
			for key, value := range row.Fields {
				log.Contents = append(log.Contents, &protocol.Log_Content{Key: key, Value: value})
			}
			logGroup.Logs = append(logGroup.Logs, log)
		}
	}
	return logGroup, nil
}

func newWireLog(event *models.Log) (*protocol.Log, error) {
	fields := event.GetIndices()
	log := &protocol.Log{Contents: make([]*protocol.Log_Content, 0, fields.Len())}
	setWireTimestamp(log, event.GetTimestamp())
	for key, value := range fields.Iterator() {
		text, err := stringLogField(key, value)
		if err != nil {
			return nil, err
		}
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: key, Value: text})
	}
	return log, nil
}

func setWireTimestamp(log *protocol.Log, timestamp uint64) {
	log.Time = uint32(timestamp / 1e9)
	if nanos := uint32(timestamp % 1e9); nanos != 0 {
		log.TimeNs = &nanos
	}
}

func stringLogField(key string, value any) (string, error) {
	switch typed := value.(type) {
	case string:
		return typed, nil
	case []byte:
		return string(typed), nil
	default:
		return "", fmt.Errorf("unsupported log content type for key %s: %T", key, value)
	}
}

// EventRecord is a string-valued output row with a Unix timestamp in nanoseconds.
type EventRecord struct {
	Fields    map[string]string
	Timestamp uint64
}

// EncodeEventRecords prepares rows for sinks that accept string-valued fields.
func EncodeEventRecords(group *models.PipelineGroupEvents) ([]EventRecord, error) {
	if group == nil || len(group.Events) == 0 {
		return nil, nil
	}
	rows := make([]EventRecord, 0, len(group.Events))
	for _, event := range group.Events {
		records, err := encodeEventRecords(event)
		if err != nil {
			return nil, err
		}
		rows = append(rows, records...)
	}
	return rows, nil
}

func encodeEventRecords(event models.PipelineEvent) ([]EventRecord, error) {
	if log, ok := event.(*models.Log); ok && log != nil {
		fields := make(map[string]string, log.GetIndices().Len())
		for key, value := range log.GetIndices().Iterator() {
			text, err := stringLogField(key, value)
			if err != nil {
				return nil, err
			}
			fields[key] = text
		}
		return []EventRecord{{Fields: fields, Timestamp: log.GetTimestamp()}}, nil
	}
	return encodeNonLogRecords(event)
}

func encodeNonLogRecords(event models.PipelineEvent) ([]EventRecord, error) {
	switch typed := event.(type) {
	case *models.Metric:
		return encodeMetricRecords(typed), nil
	case *models.Span:
		fields := map[string]string{
			"traceID":      typed.GetTraceID(),
			"spanID":       typed.GetSpanID(),
			"parentSpanID": typed.GetParentSpanID(),
			"name":         typed.GetName(),
			"kind":         spanKindText(typed.GetKind()),
			"startTime":    strconv.FormatUint(typed.GetStartTime(), 10),
			"endTime":      strconv.FormatUint(typed.GetEndTime(), 10),
			"statusCode":   strconv.FormatInt(int64(typed.GetStatus()), 10),
		}
		for k, v := range typed.GetTags().Iterator() {
			fields[k] = v
		}
		return []EventRecord{{Fields: fields, Timestamp: typed.GetTimestamp()}}, nil
	case models.ByteArray:
		return []EventRecord{{Fields: map[string]string{byteArrayContentKey: string(typed)}, Timestamp: typed.GetTimestamp()}}, nil
	default:
		return nil, fmt.Errorf("unsupported event type %T", event)
	}
}

func encodeMetricRecords(metric *models.Metric) []EventRecord {
	labels := &helper.MetricLabels{}
	for k, v := range metric.GetTags().Iterator() {
		labels.Append(k, v)
	}
	timeText := strconv.FormatUint(metric.GetTimestamp(), 10)
	labelText := labels.String()
	newRecord := func(name, value string) EventRecord {
		return EventRecord{
			Timestamp: metric.GetTimestamp(),
			Fields: map[string]string{
				metricNameKey:     helper.FormatMetricLogName(name),
				metricLabelsKey:   labelText,
				metricTimeNanoKey: timeText,
				metricValueKey:    value,
			},
		}
	}

	name := metric.GetName()
	records := make([]EventRecord, 0, 1)
	if value := metric.GetValue(); value != nil {
		if value.IsSingleValue() {
			records = append(records, newRecord(name, helper.FormatMetricLogValue(value.GetSingleValue())))
		} else if value.IsMultiValues() {
			multiValues := value.GetMultiValues().Iterator()
			for _, field := range sortedFloatKeys(multiValues) {
				records = append(records, newRecord(name+"_"+field, helper.FormatMetricLogValue(multiValues[field])))
			}
		}
	}
	if typedValues := metric.GetTypedValue(); typedValues.Len() > 0 {
		iter := typedValues.Iterator()
		for _, field := range sortedTypedKeys(iter) {
			records = append(records, newRecord(name+"_"+field, typedValueToString(iter[field])))
		}
	}
	if len(records) == 0 {
		records = append(records, newRecord(name, ""))
	}
	return records
}

func typedValueToString(tv *models.TypedValue) string {
	if tv == nil {
		return ""
	}
	return fmt.Sprint(tv.Value)
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

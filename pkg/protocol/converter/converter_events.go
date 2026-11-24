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
	"bytes"
	"encoding/json"
	"fmt"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
)

const (
	protocolKeyTime    = "time"
	protocolKeyContent = "contents"
	protocolKeyTag     = "tags"
	numProtocolKeys    = 3
)

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

	rows, err := helper.EncodeEventRecordsForEvent(event)
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
	var buf bytes.Buffer
	encoder := json.NewEncoder(&buf)
	encoder.SetEscapeHTML(false)
	if err := encoder.Encode(entry); err != nil {
		return nil, err
	}
	return bytes.TrimSuffix(buf.Bytes(), []byte{'\n'}), nil
}

// EventRecord is a string-valued output row with a Unix timestamp in nanoseconds.
type EventRecord = helper.EventRecord

// EncodeEventRecords prepares rows for sinks that accept string-valued fields.
func EncodeEventRecords(group *models.PipelineGroupEvents) ([]EventRecord, error) {
	return helper.EncodeEventRecords(group)
}

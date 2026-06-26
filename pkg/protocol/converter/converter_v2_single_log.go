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
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const passthroughLogKey = "__pipeline_passthrough__"

func (c *Converter) ConvertToSingleProtocolStreamV2(groupEvents *models.PipelineGroupEvents, targetFields []string) ([][]byte, []map[string]string, error) {
	if groupEvents == nil || len(groupEvents.Events) == 0 {
		return nil, nil, nil
	}

	marshaledLogs := make([][]byte, 0, len(groupEvents.Events))
	desiredValues := make([]map[string]string, 0, len(groupEvents.Events))
	for _, event := range groupEvents.Events {
		switch event.GetType() {
		case models.EventTypeLogging:
			log, ok := event.(*models.Log)
			if !ok {
				return nil, nil, fmt.Errorf("expected log event, got %T", event)
			}
			entry, desiredValue, err := c.convertPipelineLogToSingleProtocol(log, groupEvents.Group, targetFields)
			if err != nil {
				return nil, nil, err
			}
			b, err := marshalSingleProtocolEntry(entry)
			if err != nil {
				return nil, nil, err
			}
			marshaledLogs = append(marshaledLogs, b)
			desiredValues = append(desiredValues, desiredValue)
		case models.EventTypeMetric, models.EventTypeSpan, models.EventTypeByteArray:
			entry, desiredValue, err := c.convertPassthroughToSingleProtocol(event, groupEvents.Group, targetFields)
			if err != nil {
				if c.IgnoreUnExpectedData {
					continue
				}
				return nil, nil, err
			}
			b, err := marshalSingleProtocolEntry(entry)
			if err != nil {
				return nil, nil, err
			}
			marshaledLogs = append(marshaledLogs, b)
			desiredValues = append(desiredValues, desiredValue)
		default:
			if c.IgnoreUnExpectedData {
				continue
			}
			return nil, nil, fmt.Errorf("unsupported event type: %v", event.GetType())
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
		switch event.GetType() {
		case models.EventTypeLogging:
			log, ok := event.(*models.Log)
			if !ok {
				return nil, nil, fmt.Errorf("expected log event, got %T", event)
			}
			entry, desiredValue, err := c.convertPipelineLogToSingleProtocolFlatten(log, groupEvents.Group, targetFields)
			if err != nil {
				return nil, nil, err
			}
			b, err := marshalSingleProtocolEntry(entry)
			if err != nil {
				return nil, nil, err
			}
			marshaledLogs = append(marshaledLogs, b)
			desiredValues = append(desiredValues, desiredValue)
		case models.EventTypeMetric, models.EventTypeSpan, models.EventTypeByteArray:
			entry, desiredValue, err := c.convertPassthroughToSingleProtocolFlatten(event, groupEvents.Group, targetFields)
			if err != nil {
				if c.IgnoreUnExpectedData {
					continue
				}
				return nil, nil, err
			}
			b, err := marshalSingleProtocolEntry(entry)
			if err != nil {
				return nil, nil, err
			}
			marshaledLogs = append(marshaledLogs, b)
			desiredValues = append(desiredValues, desiredValue)
		default:
			if c.IgnoreUnExpectedData {
				continue
			}
			return nil, nil, fmt.Errorf("unsupported event type: %v", event.GetType())
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

func (c *Converter) convertPipelineLogToSingleProtocol(log *models.Log, group *models.GroupInfo, targetFields []string) (map[string]interface{}, map[string]string, error) {
	contents, tags := convertPipelineLogToMap(log, group, c.TagKeyRenameMap)
	desiredValue, err := findTargetValues(targetFields, contents, tags, c.TagKeyRenameMap)
	if err != nil {
		return nil, nil, err
	}

	entry := make(map[string]interface{}, numProtocolKeys)
	timeSec := uint32(log.GetTimestamp() / 1e9)
	if newKey, ok := c.ProtocolKeyRenameMap[protocolKeyTime]; ok {
		entry[newKey] = timeSec
	} else {
		entry[protocolKeyTime] = timeSec
	}
	if newKey, ok := c.ProtocolKeyRenameMap[protocolKeyContent]; ok {
		entry[newKey] = contents
	} else {
		entry[protocolKeyContent] = contents
	}
	if newKey, ok := c.ProtocolKeyRenameMap[protocolKeyTag]; ok {
		entry[newKey] = tags
	} else {
		entry[protocolKeyTag] = tags
	}
	return entry, desiredValue, nil
}

func (c *Converter) convertPipelineLogToSingleProtocolFlatten(log *models.Log, group *models.GroupInfo, targetFields []string) (map[string]interface{}, map[string]string, error) {
	contents, tags := convertPipelineLogToMap(log, group, c.TagKeyRenameMap)
	desiredValue, err := findTargetValues(targetFields, contents, tags, c.TagKeyRenameMap)
	if err != nil {
		return nil, nil, err
	}

	logLength := 1 + len(contents)
	if !c.OnlyContents {
		logLength += len(tags)
	}
	entry := make(map[string]interface{}, logLength)
	for k, v := range contents {
		entry[k] = v
	}
	if !c.OnlyContents {
		for k, v := range tags {
			entry[k] = v
		}
	}
	timeSec := uint32(log.GetTimestamp() / 1e9)
	if newKey, ok := c.ProtocolKeyRenameMap[protocolKeyTime]; ok {
		entry[newKey] = timeSec
	} else {
		entry[protocolKeyTime] = timeSec
	}
	return entry, desiredValue, nil
}

func (c *Converter) convertPassthroughToSingleProtocol(event models.PipelineEvent, group *models.GroupInfo, targetFields []string) (map[string]interface{}, map[string]string, error) {
	payload, err := serializePassthroughEvent(event)
	if err != nil {
		return nil, nil, err
	}
	contents := map[string]string{passthroughLogKey: string(payload)}
	tags := collectGroupTags(group, c.TagKeyRenameMap)
	for k, v := range event.GetTags().Iterator() {
		addTagIfRequired(tags, c.TagKeyRenameMap, k, v)
	}
	desiredValue, err := findTargetValues(targetFields, contents, tags, c.TagKeyRenameMap)
	if err != nil {
		return nil, nil, err
	}

	entry := make(map[string]interface{}, numProtocolKeys)
	timeSec := uint32(event.GetTimestamp() / 1e9)
	if newKey, ok := c.ProtocolKeyRenameMap[protocolKeyTime]; ok {
		entry[newKey] = timeSec
	} else {
		entry[protocolKeyTime] = timeSec
	}
	if newKey, ok := c.ProtocolKeyRenameMap[protocolKeyContent]; ok {
		entry[newKey] = contents
	} else {
		entry[protocolKeyContent] = contents
	}
	if newKey, ok := c.ProtocolKeyRenameMap[protocolKeyTag]; ok {
		entry[newKey] = tags
	} else {
		entry[protocolKeyTag] = tags
	}
	return entry, desiredValue, nil
}

func (c *Converter) convertPassthroughToSingleProtocolFlatten(event models.PipelineEvent, group *models.GroupInfo, targetFields []string) (map[string]interface{}, map[string]string, error) {
	payload, err := serializePassthroughEvent(event)
	if err != nil {
		return nil, nil, err
	}
	contents := map[string]string{passthroughLogKey: string(payload)}
	tags := collectGroupTags(group, c.TagKeyRenameMap)
	for k, v := range event.GetTags().Iterator() {
		addTagIfRequired(tags, c.TagKeyRenameMap, k, v)
	}
	desiredValue, err := findTargetValues(targetFields, contents, tags, c.TagKeyRenameMap)
	if err != nil {
		return nil, nil, err
	}

	entry := make(map[string]interface{}, 1+len(contents)+len(tags))
	entry[passthroughLogKey] = string(payload)
	if !c.OnlyContents {
		for k, v := range tags {
			entry[k] = v
		}
	}
	timeSec := uint32(event.GetTimestamp() / 1e9)
	if newKey, ok := c.ProtocolKeyRenameMap[protocolKeyTime]; ok {
		entry[newKey] = timeSec
	} else {
		entry[protocolKeyTime] = timeSec
	}
	return entry, desiredValue, nil
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
		switch event.GetType() {
		case models.EventTypeLogging:
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
		case models.EventTypeMetric, models.EventTypeSpan, models.EventTypeByteArray:
			payload, err := serializePassthroughEvent(event)
			if err != nil {
				return nil, err
			}
			ts := event.GetTimestamp()
			logGroup.Logs = append(logGroup.Logs, &protocol.Log{
				Time: uint32(ts / 1e9),
				Contents: []*protocol.Log_Content{{
					Key:   passthroughLogKey,
					Value: string(payload),
				}},
			})
		default:
			return nil, fmt.Errorf("unsupported event type: %v", event.GetType())
		}
	}
	return logGroup, nil
}

func serializePassthroughEvent(event models.PipelineEvent) ([]byte, error) {
	payload, err := passthroughPayload(event)
	if err != nil {
		return nil, err
	}
	var buf bytes.Buffer
	encoder := json.NewEncoder(&buf)
	encoder.SetEscapeHTML(false)
	if err := encoder.Encode(payload); err != nil {
		return nil, err
	}
	return bytes.TrimSpace(buf.Bytes()), nil
}

func passthroughPayload(event models.PipelineEvent) (map[string]interface{}, error) {
	payload := map[string]interface{}{
		"name":              event.GetName(),
		"timestamp":         event.GetTimestamp(),
		"observedTimestamp": event.GetObservedTimestamp(),
	}
	tags := make(map[string]string, event.GetTags().Len())
	for k, v := range event.GetTags().Iterator() {
		tags[k] = v
	}
	if len(tags) > 0 {
		payload["tags"] = tags
	}

	switch event.GetType() {
	case models.EventTypeMetric:
		payload["eventType"] = "metric"
		metric, ok := event.(*models.Metric)
		if !ok {
			return nil, fmt.Errorf("expected metric event, got %T", event)
		}
		payload["metricType"] = models.MetricTypeTexts[metric.GetMetricType()]
		if metric.GetValue().IsSingleValue() {
			payload["value"] = metric.GetValue().GetSingleValue()
		} else if metric.GetValue().IsMultiValues() {
			values := make(map[string]float64, metric.GetValue().GetMultiValues().Len())
			for k, v := range metric.GetValue().GetMultiValues().Iterator() {
				values[k] = v
			}
			payload["values"] = values
		}
		if metric.GetTypedValue().Len() > 0 {
			typedValues := make(map[string]interface{}, metric.GetTypedValue().Len())
			for k, v := range metric.GetTypedValue().Iterator() {
				typedValues[k] = v.Value
			}
			payload["typedValues"] = typedValues
		}
	case models.EventTypeSpan:
		payload["eventType"] = "span"
		span, ok := event.(*models.Span)
		if !ok {
			return nil, fmt.Errorf("expected span event, got %T", event)
		}
		payload["traceID"] = span.GetTraceID()
		payload["spanID"] = span.GetSpanID()
		payload["parentSpanID"] = span.GetParentSpanID()
		payload["kind"] = spanKindText(span.GetKind())
		payload["statusCode"] = span.GetStatus()
	case models.EventTypeByteArray:
		payload["eventType"] = "byteArray"
		payload["body"] = string(event.(models.ByteArray))
	default:
		return nil, fmt.Errorf("unsupported passthrough event type: %v", event.GetType())
	}
	return payload, nil
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

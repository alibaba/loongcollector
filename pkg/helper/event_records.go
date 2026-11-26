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

package helper

import (
	"fmt"
	"sort"
	"strconv"

	"github.com/alibaba/ilogtail/pkg/models"
)

const (
	metricNameKey     = "__name__"
	metricLabelsKey   = "__labels__"
	metricTimeNanoKey = "__time_nano__"
	metricValueKey    = "__value__"
)

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
		records, err := EncodeEventRecordsForEvent(event)
		if err != nil {
			return nil, err
		}
		rows = append(rows, records...)
	}
	return rows, nil
}

// EncodeEventRecordsForEvent prepares one or more string-valued rows for an event.
func EncodeEventRecordsForEvent(event models.PipelineEvent) ([]EventRecord, error) {
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
		return []EventRecord{{Fields: map[string]string{models.ContentKey: string(typed)}, Timestamp: typed.GetTimestamp()}}, nil
	default:
		return nil, fmt.Errorf("unsupported event type %T", event)
	}
}

func encodeMetricRecords(metric *models.Metric) []EventRecord {
	labels := &MetricLabels{}
	for k, v := range metric.GetTags().Iterator() {
		labels.Append(k, v)
	}
	timeText := strconv.FormatUint(metric.GetTimestamp(), 10)
	labelText := labels.String()
	newRecord := func(name, value string) EventRecord {
		return EventRecord{
			Timestamp: metric.GetTimestamp(),
			Fields: map[string]string{
				metricNameKey:     FormatMetricLogName(name),
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
			records = append(records, newRecord(name, FormatMetricLogValue(value.GetSingleValue())))
		} else if value.IsMultiValues() {
			multiValues := value.GetMultiValues().Iterator()
			for _, field := range sortedFloatKeys(multiValues) {
				records = append(records, newRecord(name+"_"+field, FormatMetricLogValue(multiValues[field])))
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

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

package models

import "github.com/alibaba/ilogtail/pkg/constraints"

// strKeyValuesMapImpl implements Tags.
// It is actually a wrapper of keyValuesMapImpl[string] with extra Size() and Clone() method.
type strKeyValuesMapImpl struct {
	keyValuesMapImpl[string]
}

func (s *strKeyValuesMapImpl) Size() int {
	if s == nil || s.IsNil() {
		return 0
	}
	size := 0
	for k, v := range s.keyValues {
		size += len(k) + len(v)
	}
	return size
}

func (s *strKeyValuesMapImpl) Clone() KeyValues[string] {
	if s == nil {
		return nil
	}
	newTags := NewTags()
	newTags.AddAll(s.keyValues)
	return newTags
}

func NewTagsWithMap(tags map[string]string) Tags {
	return &strKeyValuesMapImpl{
		keyValuesMapImpl: keyValuesMapImpl[string]{
			keyValues: tags,
		},
	}
}

func NewTagsWithKeyValues(keyValues ...string) Tags {
	if len(keyValues)%2 != 0 {
		keyValues = keyValues[:len(keyValues)-1]
	}
	tags := make(map[string]string)
	for i := 0; i < len(keyValues); i += 2 {
		tags[keyValues[i]] = keyValues[i+1]
	}
	return &strKeyValuesMapImpl{
		keyValuesMapImpl: keyValuesMapImpl[string]{
			keyValues: tags,
		},
	}
}

func NewTags() Tags {
	return &strKeyValuesMapImpl{
		keyValuesMapImpl: keyValuesMapImpl[string]{
			keyValues: make(map[string]string),
		},
	}
}

func NewMetadataWithMap(md map[string]string) Metadata {
	return &strKeyValuesMapImpl{
		keyValuesMapImpl: keyValuesMapImpl[string]{
			keyValues: md,
		},
	}
}

func NewMetadataWithKeyValues(keyValues ...string) Metadata {
	if len(keyValues)%2 != 0 {
		keyValues = keyValues[:len(keyValues)-1]
	}
	md := make(map[string]string)
	for i := 0; i < len(keyValues); i += 2 {
		md[keyValues[i]] = keyValues[i+1]
	}
	return &strKeyValuesMapImpl{
		keyValuesMapImpl: keyValuesMapImpl[string]{
			keyValues: md,
		},
	}
}

func NewMetadata() Metadata {
	return NewMetadataWithMap(map[string]string{})
}

func NewGroup(meta Metadata, tags Tags) *GroupInfo {
	return &GroupInfo{
		Metadata: meta,
		Tags:     tags,
	}
}
func NewMetric(name string, metricType MetricType, tags Tags, timestamp int64, value MetricValue, typedValues MetricTypedValues) *Metric {
	return &Metric{
		Name:       name,
		MetricType: metricType,
		Timestamp:  uint64(timestamp),
		Tags:       tags,
		Value:      value,
		TypedValue: typedValues,
	}
}

func NewSingleValueMetric[T constraints.IntUintFloat](name string, metricType MetricType, tags Tags, timestamp int64, value T) *Metric {
	return &Metric{
		Name:       name,
		MetricType: metricType,
		Timestamp:  uint64(timestamp),
		Tags:       tags,
		Value:      &MetricSingleValue{Value: float64(value)},
		TypedValue: NilTypedValues,
	}
}

func NewMultiValuesMetric(name string, metricType MetricType, tags Tags, timestamp int64, values MetricFloatValues) *Metric {
	return &Metric{
		Name:       name,
		MetricType: metricType,
		Timestamp:  uint64(timestamp),
		Tags:       tags,
		Value:      &MetricMultiValue{Values: values},
		TypedValue: NilTypedValues,
	}
}

func NewMetricMultiValue() *MetricMultiValue {
	return &MetricMultiValue{
		Values: &keyValuesMapImpl[float64]{
			keyValues: make(map[string]float64),
		},
	}
}

func NewMetricMultiValueWithMap(keyValues map[string]float64) *MetricMultiValue {
	return &MetricMultiValue{
		Values: &keyValuesMapImpl[float64]{
			keyValues: keyValues,
		},
	}
}

func NewMetricTypedValues() MetricTypedValues {
	return &keyValuesMapImpl[*TypedValue]{
		keyValues: make(map[string]*TypedValue),
	}
}

func NewMetricTypedValueWithMap(keyValues map[string]*TypedValue) MetricTypedValues {
	return &keyValuesMapImpl[*TypedValue]{
		keyValues: keyValues,
	}
}

func NewSpan(name, traceID, spanID string, kind SpanKind, startTime, endTime uint64, tags Tags, events []*SpanEvent, links []*SpanLink) *Span {
	return &Span{
		Name:      name,
		TraceID:   traceID,
		SpanID:    spanID,
		Kind:      kind,
		StartTime: startTime,
		EndTime:   endTime,
		Tags:      tags,
		Events:    events,
		Links:     links,
	}
}

func NewByteArray(bytes []byte) ByteArray {
	return ByteArray(bytes)
}

func NewLog(name string, body []byte, level, spanID, traceID string, tags Tags, timestamp uint64) *Log {
	log := &Log{
		Name:      name,
		Level:     level,
		Tags:      tags,
		Timestamp: timestamp,
		SpanID:    spanID,
		TraceID:   traceID,
		Contents:  NewLogContents(),
	}
	log.SetBody(body)
	return log
}

func NewSimpleLog(body []byte, tags Tags, timestamp uint64) *Log {
	log := &Log{
		Tags:      tags,
		Timestamp: timestamp,
		Contents:  NewLogContents(),
	}
	log.SetBody(body)
	return log
}

func NewSimpleLevelLog(level string, body []byte, tags Tags, timestamp uint64) *Log {
	log := &Log{
		Level:     level,
		Tags:      tags,
		Timestamp: timestamp,
		Contents:  NewLogContents(),
	}
	log.SetBody(body)
	return log
}

func NewLogContents() LogContents {
	return NewKeyValues[interface{}]()
}

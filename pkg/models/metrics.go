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

import (
	"fmt"
	"strings"
)

type MetricType int

const (
	// Common metrics types
	_ MetricType = iota
	MetricTypeUntyped
	MetricTypeCounter
	MetricTypeGauge
	MetricTypeHistogram
	MetricTypeSummary

	// Extended metrics types
	MetricTypeMeter       // In bytetsd, meter is an extension of the counter type, which contains a counter value and a rate value within a period
	MetricTypeRateCounter // In bytetsd, ratecounter is an extension of the counter type, which contains a rate value within a period
)

var (
	MetricTypeTexts = map[MetricType]string{
		MetricTypeCounter:     "Counter",
		MetricTypeGauge:       "Gauge",
		MetricTypeHistogram:   "Histogram",
		MetricTypeSummary:     "Summary",
		MetricTypeUntyped:     "Untyped",
		MetricTypeMeter:       "Meter",
		MetricTypeRateCounter: "RateCounter",
	}

	MetricTypeValues = map[string]MetricType{
		"Counter":     MetricTypeCounter,
		"Gauge":       MetricTypeGauge,
		"Histogram":   MetricTypeHistogram,
		"Summary":     MetricTypeSummary,
		"Untyped":     MetricTypeUntyped,
		"Meter":       MetricTypeMeter,
		"RateCounter": MetricTypeRateCounter,
	}

	emptyMetricValue = &EmptyMetricValue{}
)

type MetricValue interface {
	IsSingleValue() bool

	IsMultiValues() bool

	GetSingleValue() float64

	GetMultiValues() MetricFloatValues
}

type MetricSingleValue struct {
	Value float64
}

func (v *MetricSingleValue) IsSingleValue() bool {
	return true
}

func (v *MetricSingleValue) IsMultiValues() bool {
	return false
}

func (v *MetricSingleValue) GetSingleValue() float64 {
	if v != nil {
		return v.Value
	}
	return 0
}

func (v *MetricSingleValue) GetMultiValues() MetricFloatValues {
	return NilFloatValues
}

type MetricMultiValue struct {
	Values MetricFloatValues
}

func (v *MetricMultiValue) Add(key string, value float64) {
	v.Values.Add(key, value)
}

func (v *MetricMultiValue) IsSingleValue() bool {
	return false
}

func (v *MetricMultiValue) IsMultiValues() bool {
	return true
}

func (v *MetricMultiValue) GetSingleValue() float64 {
	return 0
}

func (v *MetricMultiValue) GetMultiValues() MetricFloatValues {
	if v != nil && v.Values != nil {
		return v.Values
	}
	return NilFloatValues
}

type EmptyMetricValue struct {
}

func (v *EmptyMetricValue) IsSingleValue() bool {
	return false
}

func (v *EmptyMetricValue) IsMultiValues() bool {
	return false
}

func (v *EmptyMetricValue) GetSingleValue() float64 {
	return 0
}

func (v *EmptyMetricValue) GetMultiValues() MetricFloatValues {
	return NilFloatValues
}

type MetricFloatValues interface {
	KeyValues[float64]
}

// MetricTypedValues In TSDB such as influxdb,
// its fields not only have numeric types, also string, bool, and array types.
// MetricTypedValues is used to define types other than numeric values.
type MetricTypedValues interface {
	KeyValues[*TypedValue]
}

// Defines a Metric which has one or more timeseries.  The following is a
// brief summary of the Metric data model.  For more details, see:
// https://github.com/alibaba/ilogtail/discussions/518
// - Metric is composed of a metadata and data.
// - Metadata part contains a name, description, unit, tags
// - Data is one of the possible types (Counter, Gauge, Histogram, Summary).
type Metric struct {
	Name              string
	Unit              string
	Description       string
	Timestamp         uint64
	ObservedTimestamp uint64

	Tags       Tags
	MetricType MetricType
	Value      MetricValue
	TypedValue MetricTypedValues
}

func (m *Metric) GetName() string {
	if m != nil {
		return m.Name
	}
	return ""
}

func (m *Metric) SetName(name string) {
	if m != nil {
		m.Name = name
	}
}

func (m *Metric) GetTags() Tags {
	if m != nil {
		return m.Tags
	}
	return NilStringValues
}

func (m *Metric) GetType() EventType {
	return EventTypeMetric
}

func (m *Metric) GetTimestamp() uint64 {
	if m != nil {
		return m.Timestamp
	}
	return 0
}

func (m *Metric) GetObservedTimestamp() uint64 {
	if m != nil {
		return m.ObservedTimestamp
	}
	return 0
}

func (m *Metric) SetObservedTimestamp(timestamp uint64) {
	if m != nil {
		m.ObservedTimestamp = timestamp
	}
}

func (m *Metric) GetMetricType() MetricType {
	if m != nil {
		return m.MetricType
	}
	return MetricTypeUntyped
}

func (m *Metric) GetUnit() string {
	if m != nil {
		return m.Unit
	}
	return ""
}

func (m *Metric) GetDescription() string {
	if m != nil {
		return m.Description
	}
	return ""
}

func (m *Metric) GetValue() MetricValue {
	if m != nil && m.Value != nil {
		return m.Value
	}
	return emptyMetricValue
}

func (m *Metric) GetTypedValue() MetricTypedValues {
	if m != nil && m.TypedValue != nil {
		return m.TypedValue
	}
	return NilTypedValues
}

func (m *Metric) GetSize() int64 {
	return int64(len(m.String()))
}

func (m *Metric) Clone() PipelineEvent {
	if m != nil {
		return &Metric{
			Name:              m.Name,
			Description:       m.Description,
			Timestamp:         m.Timestamp,
			ObservedTimestamp: m.ObservedTimestamp,
			Tags:              m.Tags,
			MetricType:        m.MetricType,
			Value:             m.Value,
			TypedValue:        m.TypedValue,
		}
	}
	return nil
}

func (m *Metric) String() string {
	var builder strings.Builder
	builder.WriteString(m.GetName())

	tags := m.GetTags()

	if tags.Len() > 0 {
		builder.WriteByte('{')
		sortedTags := tags.SortTo(nil)
		for i, tags := range sortedTags {
			builder.WriteString(fmt.Sprintf("%v=%v", tags.Key, tags.Value))
			if i < len(sortedTags)-1 {
				builder.WriteString(", ")
			}
		}
		builder.WriteString("}")
	}

	builder.WriteString("[Type=")
	builder.WriteString(MetricTypeTexts[m.MetricType])
	builder.WriteString(", Ts=")
	builder.WriteString(fmt.Sprintf("%v", m.Timestamp))
	builder.WriteString("] ")

	if m.Value.IsSingleValue() {
		builder.WriteString(fmt.Sprintf("value=%v", m.Value.GetSingleValue()))
	} else {
		sortedValues := m.Value.GetMultiValues().SortTo(nil)
		for i, values := range sortedValues {
			builder.WriteString(fmt.Sprintf("%v=%v", values.Key, values))
			if i < len(sortedValues)-1 {
				builder.WriteString(", ")
			}
		}
	}

	return builder.String()
}

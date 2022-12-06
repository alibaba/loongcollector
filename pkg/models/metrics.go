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

type MetricType uint32

const (
	// Common metrics types
	_ MetricType = iota
	MetricTypeCounter
	MetricTypeGauge
	MetricTypeHistogram
	MetricTypeSummary
	MetricTypeUntyped

	// Extended metrics types
	MetricTypeMeter       // In bytetsd, meter is an extension of the counter type, which contains a counter value and a rate value within a period
	MetricTypeRateCounter // In bytetsd, ratecounter is an extension of the counter type, which contains a rate value within a period
)

var (
	MetricTypeStrings = map[MetricType]string{
		MetricTypeCounter:     "Counter",
		MetricTypeGauge:       "Gauge",
		MetricTypeHistogram:   "Histogram",
		MetricTypeSummary:     "Summary",
		MetricTypeUntyped:     "Untyped",
		MetricTypeMeter:       "Meter",
		MetricTypeRateCounter: "RateCounter",
	}
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
	return nil
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
	if v != nil {
		return v.Values
	}
	return nil
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

type MetricEvent struct {
	Name              string
	MetricType        MetricType
	Timestamp         uint64
	ObservedTimestamp uint64
	Tags              Tags
	Value             MetricValue
	TypedValue        MetricTypedValues
}

func (m *MetricEvent) GetName() string {
	if m != nil {
		return m.Name
	}
	return ""
}

func (m *MetricEvent) SetName(name string) {
	if m != nil {
		m.Name = name
	}
}

func (m *MetricEvent) GetTags() Tags {
	if m != nil {
		return m.Tags
	}
	return nil
}

func (m *MetricEvent) GetType() EventType {
	return EventTypeMetric
}

func (m *MetricEvent) GetTimestamp() uint64 {
	if m != nil {
		return m.Timestamp
	}
	return 0
}

func (m *MetricEvent) GetObservedTimestamp() uint64 {
	if m != nil {
		return m.ObservedTimestamp
	}
	return 0
}

func (m *MetricEvent) SetObservedTimestamp(timestamp uint64) {
	if m != nil {
		m.ObservedTimestamp = timestamp
	}
}

// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package selfmonitor

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

func TestStrMetricV2_Name(t *testing.T) {
	type fields struct {
		name   string
		value  string
		labels []*protocol.Log_Content
	}
	tests := []struct {
		name   string
		fields fields
		want   string
	}{
		{
			name: "test_name",
			fields: fields{
				name:  "field",
				value: "v",
			},
			want: "field",
		},
		{
			name: "test_name",
			fields: fields{
				name:  "field",
				value: "v",
				labels: []*protocol.Log_Content{
					{
						Key:   "key",
						Value: "value",
					},
				},
			},
			want: "field",
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			constLabels := map[string]string{}
			for _, v := range tt.fields.labels {
				constLabels[v.Key] = v.Value
			}
			metric := NewStringMetricVector(tt.fields.name, constLabels, nil).WithLabels()
			metric.Set(tt.fields.value)

			if got := metric.(*strMetricImp).Name(); got != tt.want {
				t.Errorf("StrMetric.Name() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestStrMetricV2_Set(t *testing.T) {
	type fields struct {
		name  string
		value string
	}
	type args struct {
		v string
	}
	tests := []struct {
		name   string
		fields fields
		args   args
	}{
		{
			name: "1",
			fields: fields{
				name:  "n",
				value: "v",
			},
			args: args{
				"x",
			},
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			constLabels := map[string]string{}
			s := NewStringMetricVector(tt.fields.name, constLabels, nil).WithLabels()
			s.Set(tt.args.v)
			if s.Collect().Value != tt.args.v {
				t.Errorf("fail %s != %s\n", s.Collect(), tt.args.v)
			}
		})
	}
}

func TestDelta(t *testing.T) {
	option := defaultMetricOption()
	option.name = "test"
	option.metricType = CounterType
	ms := newMetricVector(option)
	delta := newDeltaCounter(ms, nil)

	for i := 0; i < 1000; i++ {
		delta.Add(int64(1))
	}

	assert.Equal(t, float64(1000), delta.Collect().Value)
	assert.Equal(t, float64(0), delta.Collect().Value)

	for i := 0; i < 100000; i++ {
		delta.Add(int64(1))
	}

	assert.Equal(t, float64(100000), delta.Collect().Value)
	assert.Equal(t, float64(0), delta.Collect().Value)
}

type mockMetricSet struct {
	metricOption
}

func newMockMetricSet(name string) *mockMetricSet {
	option := defaultMetricOption()
	option.name = name
	return &mockMetricSet{
		metricOption: option,
	}
}

func (m *mockMetricSet) ConstLabels() []LabelPair {
	return m.constLabels
}

func (m *mockMetricSet) IsLabelDynamic() bool {
	return m.isDynamicLabel
}

func (m *mockMetricSet) LabelKeys() []string {
	return m.labelKeys
}

func (m *mockMetricSet) Name() string {
	return m.name
}

func (m *mockMetricSet) Type() SelfMetricType {
	return m.metricType
}

var _ MetricSet = (*mockMetricSet)(nil)

func TestExpireable(t *testing.T) {
	ms := newMockMetricSet("test")
	d := newDeltaCounter(ms, nil)
	e, ok := d.(Expirable)
	assert.True(t, ok)
	assert.False(t, e.GetLastActiveTime().IsZero())
	last := e.GetLastActiveTime()
	export := d.Export()
	assert.Equal(t, "test", getMetriName(export))
	last2 := e.GetLastActiveTime()
	assert.Equal(t, last, last2)

	d.Add(1)
	export2 := d.Export()
	assert.Equal(t, "1.0000", getMetricValue(export2))
	last3 := e.GetLastActiveTime()
	assert.False(t, last3.IsZero())
	assert.NotEqual(t, last, last3)
}

func TestMetricDynamicLabel(t *testing.T) {
	constLabels := map[string]string{
		"constKey1": "constValue1",
	}
	counters := NewMetricVector[CounterMetric]("test", CounterType, constLabels, nil, WithDynamicLabel())
	counters.WithLabels(LabelPair{
		Key:   "key1",
		Value: "value1",
	}).Add(1)

	counters.WithLabels(LabelPair{
		Key:   "key2",
		Value: "value2",
	}).Add(3)

	ms := counters.Collect()
	assert.Equal(t, 2, len(ms))
	for _, m := range ms {
		event := m.ExportEvent()
		assert.Equal(t, "test", event.GetName())
		assert.Equal(t, "constValue1", event.GetTags().Get("constKey1"))
		assert.Equal(t, 2, event.GetTags().Len())
		if event.GetTags().Contains("key1") {
			assert.Equal(t, "value1", event.GetTags().Get("key1"))
			assert.Equal(t, 1.0, event.GetValue().GetSingleValue())
		}
		if event.GetTags().Contains("key2") {
			assert.Equal(t, "value2", event.GetTags().Get("key2"))
			assert.Equal(t, 3.0, event.GetValue().GetSingleValue())
		}
	}
}

func TestHistogram(t *testing.T) {
	option := defaultMetricOption()
	option.name = "test"
	option.metricType = HistogramType
	ms := newMetricVector(option)
	histogram := newHistogram(ms, nil, nil)
	valuesToObserve := []float64{1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536}
	for _, v := range valuesToObserve {
		histogram.Observe(v)
	}
	event := histogram.ExportEvent()
	assert.Equal(t, "test", event.GetName())
	assert.Equal(t, len(defaultHistogramBucketsMs)+1+2, event.GetValue().GetMultiValues().Len())

	// (-Inf,2]=2, (10,50]=2, (100,200]=1, (1000,1400]=1, (10000,15000]=0, (1400,2000]=0, (15000,+Inf]=3, (2,4]=1, (200,400]=1, (2000,5000]=2, (4,6]=0, (400,800]=1, (50,100]=1, (5000,10000]=1, (6,8]=1, (8,10]=0, (800,1000]=0, count=17, sum=131071
	expected := map[string]float64{
		"(-Inf,2]":      2,
		"(10,50]":       2,
		"(100,200]":     1,
		"(1000,1400]":   1,
		"(10000,15000]": 0,
		"(1400,2000]":   0,
		"(15000,+Inf]":  3,
		"(2,4]":         1,
		"(200,400]":     1,
		"(2000,5000]":   2,
		"(4,6]":         0,
		"(400,800]":     1,
		"(50,100]":      1,
		"(5000,10000]":  1,
		"(6,8]":         1,
		"(8,10]":        0,
		"(800,1000]":    0,
		"count":         17,
		"sum":           131071,
	}
	for _, v := range event.GetValue().GetMultiValues().SortTo(nil) {
		assert.Equal(t, expected[v.Key], v.Value)
	}

	event2 := histogram.ExportEvent()
	expected2 := map[string]float64{
		"(-Inf,2]":      0,
		"(10,50]":       0,
		"(100,200]":     0,
		"(1000,1400]":   0,
		"(10000,15000]": 0,
		"(1400,2000]":   0,
		"(15000,+Inf]":  0,
		"(2,4]":         0,
		"(200,400]":     0,
		"(2000,5000]":   0,
		"(4,6]":         0,
		"(400,800]":     0,
		"(50,100]":      0,
		"(5000,10000]":  0,
		"(6,8]":         0,
		"(8,10]":        0,
		"(800,1000]":    0,
		"count":         0,
		"sum":           0,
	}
	for _, v := range event2.GetValue().GetMultiValues().SortTo(nil) {
		assert.Equal(t, expected2[v.Key], v.Value)
	}
}

func TestCumulativeHistogram(t *testing.T) {
	option := defaultMetricOption()
	option.name = "test"
	option.metricType = HistogramType
	option.isCumulative = true
	option.isDynamicLabel = true
	ms := newMetricVector(option)
	histogram := newCumulativeHistogram(ms, []string{"hello", "world"}, nil)
	valuesToObserve := []float64{1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536}
	for _, v := range valuesToObserve {
		histogram.Observe(v)
	}
	event := histogram.ExportEvent()
	assert.Equal(t, "test", event.GetName())
	assert.Equal(t, len(defaultHistogramBucketsMs)+1+2, event.GetValue().GetMultiValues().Len())

	// (-Inf,2]=2, (10,50]=2, (100,200]=1, (1000,1400]=1, (10000,15000]=0, (1400,2000]=0, (15000,+Inf]=3, (2,4]=1, (200,400]=1, (2000,5000]=2, (4,6]=0, (400,800]=1, (50,100]=1, (5000,10000]=1, (6,8]=1, (8,10]=0, (800,1000]=0, count=17, sum=131071
	expected := map[string]float64{
		"(-Inf,2]":      2,
		"(10,50]":       2,
		"(100,200]":     1,
		"(1000,1400]":   1,
		"(10000,15000]": 0,
		"(1400,2000]":   0,
		"(15000,+Inf]":  3,
		"(2,4]":         1,
		"(200,400]":     1,
		"(2000,5000]":   2,
		"(4,6]":         0,
		"(400,800]":     1,
		"(50,100]":      1,
		"(5000,10000]":  1,
		"(6,8]":         1,
		"(8,10]":        0,
		"(800,1000]":    0,
		"count":         17,
		"sum":           131071,
	}
	for _, v := range event.GetValue().GetMultiValues().SortTo(nil) {
		assert.Equal(t, expected[v.Key], v.Value)
	}

	event2 := histogram.ExportEvent()
	expected2 := map[string]float64{
		"(-Inf,2]":      2,
		"(10,50]":       2,
		"(100,200]":     1,
		"(1000,1400]":   1,
		"(10000,15000]": 0,
		"(1400,2000]":   0,
		"(15000,+Inf]":  3,
		"(2,4]":         1,
		"(200,400]":     1,
		"(2000,5000]":   2,
		"(4,6]":         0,
		"(400,800]":     1,
		"(50,100]":      1,
		"(5000,10000]":  1,
		"(6,8]":         1,
		"(8,10]":        0,
		"(800,1000]":    0,
		"count":         17,
		"sum":           131071,
	}
	for _, v := range event2.GetValue().GetMultiValues().SortTo(nil) {
		assert.Equal(t, expected2[v.Key], v.Value)
	}
}

// Copyright 2021 iLogtail Authors
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
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"

	"reflect"
	"testing"
	"time"
)

func TestStrMetric_Name(t *testing.T) {
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
			want: "field#key=value",
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := &StrMetric{
				name:   tt.fields.name,
				value:  tt.fields.value,
				labels: tt.fields.labels,
			}
			if got := s.Name(); got != tt.want {
				t.Errorf("StrMetric.Name() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestStrMetric_Set(t *testing.T) {
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
			s := &StrMetric{
				name:  tt.fields.name,
				value: tt.fields.value,
			}
			s.Set(tt.args.v)
			if s.Get() != tt.args.v {
				t.Errorf("fail %s != %s\n", s.Get(), tt.args.v)
			}
		})
	}
}

func TestStrMetric_Get(t *testing.T) {
	type fields struct {
		name  string
		value string
	}
	tests := []struct {
		name   string
		fields fields
		want   string
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := &StrMetric{
				name:  tt.fields.name,
				value: tt.fields.value,
			}
			if got := s.Get(); got != tt.want {
				t.Errorf("StrMetric.Get() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestNormalMetric_Add(t *testing.T) {
	type fields struct {
		name  string
		value int64
	}
	type args struct {
		v int64
	}
	tests := []struct {
		name   string
		fields fields
		args   args
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := &NormalMetric{
				name:  tt.fields.name,
				value: tt.fields.value,
			}
			s.Add(tt.args.v)
		})
	}
}

func TestNormalMetric_Clear(t *testing.T) {
	type fields struct {
		name  string
		value int64
	}
	type args struct {
		v int64
	}
	tests := []struct {
		name   string
		fields fields
		args   args
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := &NormalMetric{
				name:  tt.fields.name,
				value: tt.fields.value,
			}
			s.Clear(tt.args.v)
		})
	}
}

func TestNormalMetric_Get(t *testing.T) {
	type fields struct {
		name  string
		value int64
	}
	tests := []struct {
		name   string
		fields fields
		want   int64
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := &NormalMetric{
				name:  tt.fields.name,
				value: tt.fields.value,
			}
			if got := s.Get(); got != tt.want {
				t.Errorf("NormalMetric.Get() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestNormalMetric_Name(t *testing.T) {
	type fields struct {
		name  string
		value int64
	}
	tests := []struct {
		name   string
		fields fields
		want   string
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := &NormalMetric{
				name:  tt.fields.name,
				value: tt.fields.value,
			}
			if got := s.Name(); got != tt.want {
				t.Errorf("NormalMetric.Name() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestAvgMetric_Add(t *testing.T) {
	type fields struct {
		name    string
		value   int64
		count   int64
		prevAvg float64
	}
	type args struct {
		v int64
	}
	tests := []struct {
		name   string
		fields fields
		args   args
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := &AvgMetric{
				name:    tt.fields.name,
				value:   tt.fields.value,
				count:   tt.fields.count,
				prevAvg: tt.fields.prevAvg,
			}
			s.Add(tt.args.v)
		})
	}
}

func TestAvgMetric_Clear(t *testing.T) {
	type fields struct {
		name    string
		value   int64
		count   int64
		prevAvg float64
	}
	type args struct {
		v int64
	}
	tests := []struct {
		name   string
		fields fields
		args   args
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := &AvgMetric{
				name:    tt.fields.name,
				value:   tt.fields.value,
				count:   tt.fields.count,
				prevAvg: tt.fields.prevAvg,
			}
			s.Clear(tt.args.v)
		})
	}
}

func TestAvgMetric_Get(t *testing.T) {
	type fields struct {
		name    string
		value   int64
		count   int64
		prevAvg float64
	}
	tests := []struct {
		name   string
		fields fields
		want   int64
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := &AvgMetric{
				name:    tt.fields.name,
				value:   tt.fields.value,
				count:   tt.fields.count,
				prevAvg: tt.fields.prevAvg,
			}
			if got := s.Get(); got != tt.want {
				t.Errorf("AvgMetric.Get() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestAvgMetric_GetAvg(t *testing.T) {
	type fields struct {
		name    string
		value   int64
		count   int64
		prevAvg float64
	}
	tests := []struct {
		name   string
		fields fields
		want   float64
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := &AvgMetric{
				name:    tt.fields.name,
				value:   tt.fields.value,
				count:   tt.fields.count,
				prevAvg: tt.fields.prevAvg,
			}
			if got := s.GetAvg(); got != tt.want {
				t.Errorf("AvgMetric.GetAvg() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestAvgMetric_Name(t *testing.T) {
	type fields struct {
		name    string
		value   int64
		count   int64
		prevAvg float64
	}
	tests := []struct {
		name   string
		fields fields
		want   string
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := &AvgMetric{
				name:    tt.fields.name,
				value:   tt.fields.value,
				count:   tt.fields.count,
				prevAvg: tt.fields.prevAvg,
			}
			if got := s.Name(); got != tt.want {
				t.Errorf("AvgMetric.Name() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestLatMetric_Name(t *testing.T) {
	type fields struct {
		name       string
		t          time.Time
		count      int
		latencySum time.Duration
	}
	tests := []struct {
		name   string
		fields fields
		want   string
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := &LatMetric{
				name:       tt.fields.name,
				t:          tt.fields.t,
				count:      tt.fields.count,
				latencySum: tt.fields.latencySum,
			}
			if got := s.Name(); got != tt.want {
				t.Errorf("LatMetric.Name() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestLatMetric_Begin(t *testing.T) {
	type fields struct {
		name       string
		t          time.Time
		count      int
		latencySum time.Duration
	}
	tests := []struct {
		name   string
		fields fields
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := &LatMetric{
				name:       tt.fields.name,
				t:          tt.fields.t,
				count:      tt.fields.count,
				latencySum: tt.fields.latencySum,
			}
			s.Begin()
		})
	}
}

func TestLatMetric_End(t *testing.T) {
	type fields struct {
		name       string
		t          time.Time
		count      int
		latencySum time.Duration
	}
	tests := []struct {
		name   string
		fields fields
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := &LatMetric{
				name:       tt.fields.name,
				t:          tt.fields.t,
				count:      tt.fields.count,
				latencySum: tt.fields.latencySum,
			}
			s.End()
		})
	}
}

func TestLatMetric_Clear(t *testing.T) {
	type fields struct {
		name       string
		t          time.Time
		count      int
		latencySum time.Duration
	}
	tests := []struct {
		name   string
		fields fields
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := &LatMetric{
				name:       tt.fields.name,
				t:          tt.fields.t,
				count:      tt.fields.count,
				latencySum: tt.fields.latencySum,
			}
			s.Clear()
		})
	}
}

func TestLatMetric_Get(t *testing.T) {
	type fields struct {
		name       string
		t          time.Time
		count      int
		latencySum time.Duration
	}
	tests := []struct {
		name   string
		fields fields
		want   int64
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := &LatMetric{
				name:       tt.fields.name,
				t:          tt.fields.t,
				count:      tt.fields.count,
				latencySum: tt.fields.latencySum,
			}
			if got := s.Get(); got != tt.want {
				t.Errorf("LatMetric.Get() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestNewCounterMetric(t *testing.T) {
	type args struct {
		n string
	}
	tests := []struct {
		name string
		args args
		want pipeline.CounterMetric
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := NewCumulativeCounterMetric(tt.args.n); !reflect.DeepEqual(got, tt.want) {
				t.Errorf("NewCounterMetric() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestNewAverageMetric(t *testing.T) {
	type args struct {
		n string
	}
	tests := []struct {
		name string
		args args
		want pipeline.CounterMetric
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := NewAverageMetric(tt.args.n); !reflect.DeepEqual(got, tt.want) {
				t.Errorf("NewAverageMetric() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestNewStringMetric(t *testing.T) {
	type args struct {
		n string
	}
	tests := []struct {
		name string
		args args
		want pipeline.StringMetric
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := NewStringMetric(tt.args.n); !reflect.DeepEqual(got, tt.want) {
				t.Errorf("NewStringMetric() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestNewLatencyMetric(t *testing.T) {
	type args struct {
		n string
	}
	tests := []struct {
		name string
		args args
		want pipeline.LatencyMetric
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := NewLatencyMetric(tt.args.n); !reflect.DeepEqual(got, tt.want) {
				t.Errorf("NewLatencyMetric() = %v, want %v", got, tt.want)
			}
		})
	}
}

// Copyright 2023 iLogtail Authors
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
	"reflect"
	"testing"
	"time"
	"unsafe"

	"github.com/stretchr/testify/assert"
)

func Test_Tags_SortTo(t *testing.T) {
	tests := []struct {
		caseName      string
		tags          map[string]string
		buf           []KeyValue[string]
		want          []KeyValue[string]
		wantOriginBuf bool
	}{
		{
			caseName: "empty tags empty buf",
			tags:     nil,
			buf:      nil,
			want:     nil,
		},
		{
			caseName: "empty tags none empty buf",
			tags:     nil,
			buf:      []KeyValue[string]{{Key: "a", Value: "b"}},
			want:     []KeyValue[string]{},
		},
		{
			caseName: "none empty tags empty buf",
			tags:     map[string]string{"b": "b", "a": "a"},
			buf:      nil,
			want:     []KeyValue[string]{{Key: "a", Value: "a"}, {Key: "b", Value: "b"}},
		},
		{
			caseName:      "none empty tags none empty buf enough len",
			tags:          map[string]string{"b": "b", "a": "a"},
			buf:           []KeyValue[string]{{Key: "c", Value: "c"}, {Key: "c", Value: "c"}, {Key: "c", Value: "c"}},
			want:          []KeyValue[string]{{Key: "a", Value: "a"}, {Key: "b", Value: "b"}},
			wantOriginBuf: true,
		},
		{
			caseName: "none empty tags none empty buf not enough len",
			tags:     map[string]string{"b": "b", "a": "a"},
			buf:      []KeyValue[string]{{Key: "c", Value: "c"}},
			want:     []KeyValue[string]{{Key: "a", Value: "a"}, {Key: "b", Value: "b"}},
		},
	}

	for _, cases := range tests {
		tags := NewTagsWithMap(cases.tags)
		result := tags.SortTo(cases.buf)
		assert.ElementsMatchf(t, result, cases.want, cases.caseName)
		if cases.wantOriginBuf {
			assert.Equalf(t, (*reflect.SliceHeader)(unsafe.Pointer(&cases.buf)).Data, (*reflect.SliceHeader)(unsafe.Pointer(&result)).Data, cases.caseName)
		}
	}
}

func BenchmarkMetricGetSize(b *testing.B) {
	tags := NewTags()
	for i := 0; i < 15; i++ {
		tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
	}
	metric := NewSingleValueMetric("cpu_usage", MetricTypeGauge, tags, time.Now().UnixNano(), 1.0)

	b.Run("GetSizeByString", func(b *testing.B) {
		b.ReportAllocs()
		b.ResetTimer()
		for i := 0; i < b.N; i++ {
			_ = int64(len(metric.String()))
		}
	})

	b.Run("MetricGetSize", func(b *testing.B) {
		b.ReportAllocs()
		b.ResetTimer()
		for i := 0; i < b.N; i++ {
			_ = metric.GetSize()
		}
	})
}

func BenchmarkNilKeyValueGetIterator(b *testing.B) {
	kvs := NilStringValues

	b.Run("NilKeyValueGetIterator", func(b *testing.B) {
		b.ReportAllocs()
		b.ResetTimer()
		for i := 0; i < b.N; i++ {
			_ = kvs.Iterator()
		}
	})
}

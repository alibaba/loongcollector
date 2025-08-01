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
	sortedTags := NewSortedKeyValues()
	for i := 0; i < 15; i++ {
		tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
		sortedTags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
	}
	metric := NewSingleValueMetric("cpu_usage", MetricTypeGauge, tags, time.Now().UnixNano(), 1.0)
	sortedMetric := NewSingleValueMetric("cpu_usage", MetricTypeGauge, sortedTags, time.Now().UnixNano(), 1.0)

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

	b.Run("MetricGetSize_WithSortedTags", func(b *testing.B) {
		b.ReportAllocs()
		b.ResetTimer()
		for i := 0; i < b.N; i++ {
			_ = sortedMetric.GetSize()
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

func TestSortedKeyValues(t *testing.T) {
	t.Run("Add", func(t *testing.T) {
		testcases := []struct {
			caseName string
			elements []KeyValue[string]
			want     []KeyValue[string]
		}{
			{
				caseName: "AddOne",
				elements: []KeyValue[string]{{Key: "a", Value: "a"}},
				want:     []KeyValue[string]{{Key: "a", Value: "a"}},
			},
			{
				caseName: "AddSameKey",
				elements: []KeyValue[string]{{Key: "a", Value: "a"}, {Key: "a", Value: "b"}},
				want:     []KeyValue[string]{{Key: "a", Value: "b"}},
			},
			{
				caseName: "AddInOrder",
				elements: []KeyValue[string]{{Key: "a", Value: "a"}, {Key: "b", Value: "b"}, {Key: "c", Value: "c"}},
				want:     []KeyValue[string]{{Key: "a", Value: "a"}, {Key: "b", Value: "b"}, {Key: "c", Value: "c"}},
			},
			{
				caseName: "AddInOrderSameKey",
				elements: []KeyValue[string]{{Key: "a", Value: "a"}, {Key: "a", Value: "b"}, {Key: "a", Value: "c"}, {Key: "a", Value: "d"}},
				want:     []KeyValue[string]{{Key: "a", Value: "d"}},
			},
			{
				caseName: "addInReverseOrder",
				elements: []KeyValue[string]{{Key: "c", Value: "c"}, {Key: "b", Value: "b"}, {Key: "a", Value: "a"}},
				want:     []KeyValue[string]{{Key: "a", Value: "a"}, {Key: "b", Value: "b"}, {Key: "c", Value: "c"}},
			},
		}
		for _, tc := range testcases {
			t.Run(tc.caseName, func(t *testing.T) {
				s := NewSortedKeyValues()
				for _, kv := range tc.elements {
					s.Add(kv.Key, kv.Value)
				}
				assert.ElementsMatch(t, s.ToArray(), tc.want)
			})
		}
	})

	t.Run("AddAll", func(t *testing.T) {
		testcases := []struct {
			caseName string
			elements map[string]string
			want     []KeyValue[string]
		}{
			{
				caseName: "AddAll",
				elements: map[string]string{"a": "a", "b": "b", "c": "c"},
				want:     []KeyValue[string]{{Key: "a", Value: "a"}, {Key: "b", Value: "b"}, {Key: "c", Value: "c"}},
			},
			{
				caseName: "AddNil",
				elements: nil,
				want:     nil,
			},
		}
		for _, tc := range testcases {
			t.Run(tc.caseName, func(t *testing.T) {
				s := NewSortedKeyValues()
				s.AddAll(tc.elements)
				assert.ElementsMatch(t, s.ToArray(), tc.want)
			})
		}
	})

	t.Run("Iterator", func(t *testing.T) {
		testcases := []struct {
			caseName string
			elements map[string]string
			want     map[string]string
		}{
			{
				caseName: "AddAll",
				elements: map[string]string{"a": "a", "b": "b", "c": "c"},
				want:     map[string]string{"a": "a", "b": "b", "c": "c"},
			},
			{
				caseName: "AddNil",
				elements: nil,
				want:     nil,
			},
		}
		for _, tc := range testcases {
			t.Run(tc.caseName, func(t *testing.T) {
				s := NewSortedKeyValues()
				s.AddAll(tc.elements)
				assert.EqualValues(t, s.Iterator(), tc.want)
			})
		}
	})

	t.Run("Contains", func(t *testing.T) {
		testcases := []struct {
			caseName string
			elements map[string]string
			key      string
			want     bool
		}{
			{
				caseName: "AddAll",
				elements: map[string]string{"a": "a", "b": "b", "c": "c"},
				key:      "a",
				want:     true,
			},
			{
				caseName: "AddNil",
				elements: nil,
				key:      "a",
				want:     false,
			},
		}
		for _, tc := range testcases {
			t.Run(tc.caseName, func(t *testing.T) {
				s := NewSortedKeyValues()
				s.AddAll(tc.elements)
				assert.Equal(t, s.Contains(tc.key), tc.want)
			})
		}
	})

	t.Run("Delete", func(t *testing.T) {
		testcases := []struct {
			caseName string
			elements map[string]string
			key      string
			want     []KeyValue[string]
		}{
			{
				caseName: "AddAll",
				elements: map[string]string{"a": "a", "b": "b", "c": "c"},
				key:      "a",
				want:     []KeyValue[string]{{Key: "b", Value: "b"}, {Key: "c", Value: "c"}},
			},
			{
				caseName: "AddNil",
				elements: nil,
				key:      "a",
				want:     nil,
			},
		}
		for _, tc := range testcases {
			t.Run(tc.caseName, func(t *testing.T) {
				s := NewSortedKeyValues()
				s.AddAll(tc.elements)
				s.Delete(tc.key)
				assert.ElementsMatch(t, s.ToArray(), tc.want)
			})
		}
	})
}

func BenchmarkTags(b *testing.B) {
	tagCount := 20
	b.Run(fmt.Sprintf("Add(size:%d)", tagCount), func(b *testing.B) {
		b.Run("Tags", func(b *testing.B) {
			b.ReportAllocs()
			b.ResetTimer()
			tags := NewTags()
			for i := 0; i < b.N; i++ {
				index := i % tagCount
				tags.Add(fmt.Sprintf("tag_%d", index), fmt.Sprintf("value_%d", i))
			}
		})

		b.Run("SortedTags", func(b *testing.B) {
			b.ReportAllocs()
			b.ResetTimer()
			tags := NewSortedKeyValues()
			for i := 0; i < b.N; i++ {
				index := i % tagCount
				tags.Add(fmt.Sprintf("tag_%d", index), fmt.Sprintf("value_%d", i))
			}
		})
	})

	b.Run(fmt.Sprintf("Iterator(size:%d)", tagCount), func(b *testing.B) {
		b.Run("Tags", func(b *testing.B) {
			tags := NewTags()
			for i := 0; i < tagCount; i++ {
				tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				_ = tags.Iterator()
			}
		})

		b.Run("SortedTags", func(b *testing.B) {
			tags := NewSortedKeyValues()
			for i := 0; i < tagCount; i++ {
				tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				_ = tags.Iterator()
			}
		})
	})

	b.Run(fmt.Sprintf("SortTo(size:%d)", tagCount), func(b *testing.B) {
		b.Run("Tags", func(b *testing.B) {
			tags := NewTags()
			for i := 0; i < tagCount; i++ {
				tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				_ = tags.SortTo(nil)
			}
		})

		b.Run("SortedTags", func(b *testing.B) {
			tags := NewSortedKeyValues()
			for i := 0; i < tagCount; i++ {
				tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				_ = tags.SortTo(nil)
			}
		})
	})

	b.Run(fmt.Sprintf("ToArray(size:%d)", tagCount), func(b *testing.B) {
		b.Run("Tags", func(b *testing.B) {
			tags := NewTags()
			for i := 0; i < tagCount; i++ {
				tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				_ = tags.ToArray()
			}
		})
		b.Run("SortedTags", func(b *testing.B) {
			tags := NewSortedKeyValues()
			for i := 0; i < tagCount; i++ {
				tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				_ = tags.ToArray()
			}
		})
	})

	b.Run(fmt.Sprintf("Delete(size:%d)", tagCount), func(b *testing.B) {
		b.Run("Tags", func(b *testing.B) {
			tags := NewTags()
			for i := 0; i < tagCount; i++ {
				tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				tags.Delete(fmt.Sprintf("tag_%d", 4))
			}
		})
		b.Run("SortedTags", func(b *testing.B) {
			tags := NewSortedKeyValues()
			for i := 0; i < tagCount; i++ {
				tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				tags.Delete(fmt.Sprintf("tag_%d", 4))
			}
		})
	})

}

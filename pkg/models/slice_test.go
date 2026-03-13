// Copyright 2025 iLogtail Authors
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
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestSortedTagSlice(t *testing.T) {
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
				s := NewTagSlice()
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
				s := NewTagSlice()
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
				s := NewTagSlice()
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
				s := NewTagSlice()
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
				s := NewTagSlice()
				s.AddAll(tc.elements)
				s.Delete(tc.key)
				assert.ElementsMatch(t, s.ToArray(), tc.want)
			})
		}
	})

	t.Run("Merge", func(t *testing.T) {
		testcases := []struct {
			caseName string
			elements map[string]string
			isSorted bool
			notEmpty bool
			want     []KeyValue[string]
		}{
			{
				caseName: "MergeAll_Slice",
				elements: map[string]string{"a": "a", "b": "b", "c": "c"},
				isSorted: true,
				want:     []KeyValue[string]{{Key: "a", Value: "a"}, {Key: "b", Value: "b"}, {Key: "c", Value: "c"}},
			},
			{
				caseName: "MergeAll_Map",
				elements: map[string]string{"a": "a", "b": "b", "c": "c"},
				isSorted: false,
				want:     []KeyValue[string]{{Key: "a", Value: "a"}, {Key: "b", Value: "b"}, {Key: "c", Value: "c"}},
			},
			{
				caseName: "MergeAll_Slice_NotEmpty",
				elements: map[string]string{"a": "a", "b": "b", "c": "c"},
				isSorted: true,
				notEmpty: true,
				want:     []KeyValue[string]{{Key: "a", Value: "a"}, {Key: "b", Value: "b"}, {Key: "c", Value: "c"}},
			},
			{
				caseName: "AddNil",
				elements: nil,
				isSorted: true,
				want:     nil,
			},
		}
		for _, tc := range testcases {
			t.Run(tc.caseName, func(t *testing.T) {
				var src Tags
				if tc.isSorted {
					src = NewTagSlice()
					src.AddAll(tc.elements)
				} else {
					src = NewTags()
					src.AddAll(tc.elements)
				}

				s := NewTagSlice()
				if tc.notEmpty {
					s.Add("a", "aa")
				}
				s.Merge(src)
				assert.ElementsMatch(t, s.ToArray(), tc.want)
			})
		}
	})

	t.Run("LoadOrStore", func(t *testing.T) {
		testcases := []struct {
			caseName string
			elements map[string]string
			key      string
			value    string
			want     string
			wantBool bool
		}{
			{
				caseName: "AddAll",
				elements: map[string]string{"a": "a", "b": "b", "c": "c"},
				key:      "a",
				value:    "aa",
				want:     "a",
				wantBool: true,
			},
			{
				caseName: "AddNil",
				elements: nil,
				key:      "a",
				value:    "aa",
				want:     "aa",
				wantBool: false,
			},
		}
		for _, tc := range testcases {
			t.Run(tc.caseName, func(t *testing.T) {
				s := NewTagSlice()
				s.AddAll(tc.elements)
				got, gotBool := s.LoadOrStore(tc.key, tc.value)
				assert.Equal(t, got, tc.want)
				assert.Equal(t, gotBool, tc.wantBool)

				m := NewTags()
				m.AddAll(tc.elements)
				got, gotBool = m.LoadOrStore(tc.key, tc.value)
				assert.Equal(t, got, tc.want)
				assert.Equal(t, gotBool, tc.wantBool)
			})
		}
	})

	t.Run("Range", func(t *testing.T) {
		t.Run("Tags", func(t *testing.T) {
			tags := NewTags()
			tags.Add("a", "a")
			tags.Add("b", "b")
			tags.Add("c", "c")
			count := 0
			tags.Range(func(key string, value string) bool {
				assert.Equal(t, key, value)
				count++
				return true
			})
			assert.Equal(t, count, 3)
		})

		t.Run("TagSlice", func(t *testing.T) {
			tags := NewTagSlice()
			tags.Add("a", "a")
			tags.Add("b", "b")
			tags.Add("c", "c")
			count := 0
			tags.Range(func(key string, value string) bool {
				assert.Equal(t, key, value)
				count++
				return true
			})
			assert.Equal(t, count, 3)
		})
	})

	t.Run("RangeMut", func(t *testing.T) {
		t.Run("Tags", func(t *testing.T) {
			tags := NewTags()
			tags.Add("a", "a")
			tags.Add("b", "b")
			tags.Add("c", "c")
			count := 0
			tags.RangeMut(func(key string, value string) MutResult[string] {
				assert.Equal(t, key, value)
				count++

				return MutResult[string]{
					NewKeyValue: KeyValue[string]{Key: key, Value: value + "_mut"},
					Replace:     true,
					Continue:    true,
				}
			})
			assert.Equal(t, count, 3)
			assert.Equal(t, tags.Get("a"), "a_mut")
			assert.Equal(t, tags.Get("b"), "b_mut")
			assert.Equal(t, tags.Get("c"), "c_mut")
		})

		t.Run("TagSlice", func(t *testing.T) {
			tags := NewTagSlice()
			tags.Add("a", "a")
			tags.Add("b", "b")
			tags.Add("c", "c")
			count := 0
			tags.RangeMut(func(key string, value string) MutResult[string] {
				assert.Equal(t, key, value)
				count++

				return MutResult[string]{
					NewKeyValue: KeyValue[string]{Key: key, Value: value + "_mut"},
					Replace:     true,
					Continue:    true,
				}
			})
			assert.Equal(t, count, 3)
			assert.Equal(t, tags.Get("a"), "a_mut")
			assert.Equal(t, tags.Get("b"), "b_mut")
			assert.Equal(t, tags.Get("c"), "c_mut")
		})
	})
}

func TestUnsortedKeyValueSlice(t *testing.T) {
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
				want:     []KeyValue[string]{{Key: "c", Value: "c"}, {Key: "b", Value: "b"}, {Key: "a", Value: "a"}},
			},
		}
		for _, tc := range testcases {
			t.Run(tc.caseName, func(t *testing.T) {
				s := NewKeyValuesSlice[string]()
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
				s := NewKeyValuesSlice[string]()
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
				s := NewKeyValuesSlice[string]()
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
				s := NewKeyValuesSlice[string]()
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
				s := NewKeyValuesSlice[string]()
				s.AddAll(tc.elements)
				s.Delete(tc.key)
				assert.ElementsMatch(t, s.ToArray(), tc.want)
			})
		}
	})

	t.Run("Merge", func(t *testing.T) {
		testcases := []struct {
			caseName string
			elements map[string]string
			isSorted bool
			notEmpty bool
			want     []KeyValue[string]
		}{
			{
				caseName: "MergeAll_Slice",
				elements: map[string]string{"a": "a", "b": "b", "c": "c"},
				isSorted: true,
				want:     []KeyValue[string]{{Key: "a", Value: "a"}, {Key: "b", Value: "b"}, {Key: "c", Value: "c"}},
			},
			{
				caseName: "MergeAll_Map",
				elements: map[string]string{"a": "a", "b": "b", "c": "c"},
				isSorted: false,
				want:     []KeyValue[string]{{Key: "a", Value: "a"}, {Key: "b", Value: "b"}, {Key: "c", Value: "c"}},
			},
			{
				caseName: "MergeAll_Slice_NotEmpty",
				elements: map[string]string{"a": "a", "b": "b", "c": "c"},
				isSorted: true,
				notEmpty: true,
				want:     []KeyValue[string]{{Key: "a", Value: "a"}, {Key: "b", Value: "b"}, {Key: "c", Value: "c"}},
			},
			{
				caseName: "AddNil",
				elements: nil,
				isSorted: true,
				want:     nil,
			},
		}
		for _, tc := range testcases {
			t.Run(tc.caseName, func(t *testing.T) {
				var src Tags
				if tc.isSorted {
					src = NewTagSlice()
					src.AddAll(tc.elements)
				} else {
					src = NewTags()
					src.AddAll(tc.elements)
				}

				s := NewKeyValuesSlice[string]()
				s.Merge(src)
				assert.ElementsMatch(t, s.ToArray(), tc.want)
			})
		}
	})

	t.Run("LoadOrStore", func(t *testing.T) {
		testcases := []struct {
			caseName string
			elements map[string]string
			key      string
			value    string
			want     string
			wantBool bool
		}{
			{
				caseName: "AddAll",
				elements: map[string]string{"a": "a", "b": "b", "c": "c"},
				key:      "a",
				value:    "aa",
				want:     "a",
				wantBool: true,
			},
			{
				caseName: "AddNil",
				elements: nil,
				key:      "a",
				value:    "aa",
				want:     "aa",
				wantBool: false,
			},
		}
		for _, tc := range testcases {
			t.Run(tc.caseName, func(t *testing.T) {
				s := NewKeyValuesSlice[string]()
				s.AddAll(tc.elements)
				got, gotBool := s.LoadOrStore(tc.key, tc.value)
				assert.Equal(t, got, tc.want)
				assert.Equal(t, gotBool, tc.wantBool)
			})
		}
	})

	t.Run("Range", func(t *testing.T) {
		t.Run("TagSlice", func(t *testing.T) {
			tags := NewKeyValuesSlice[string]()
			tags.Add("a", "a")
			tags.Add("b", "b")
			tags.Add("c", "c")
			count := 0
			tags.Range(func(key string, value string) bool {
				assert.Equal(t, key, value)
				count++
				return true
			})
			assert.Equal(t, count, 3)
		})
	})

	t.Run("RangeMut", func(t *testing.T) {

		t.Run("TagSlice", func(t *testing.T) {
			tags := NewKeyValuesSlice[string]()
			tags.Add("a", "a")
			tags.Add("b", "b")
			tags.Add("c", "c")
			count := 0
			tags.RangeMut(func(key string, value string) MutResult[string] {
				assert.Equal(t, key, value)
				count++

				return MutResult[string]{
					NewKeyValue: KeyValue[string]{Key: key, Value: value + "_mut"},
					Replace:     true,
					Continue:    true,
				}
			})
			assert.Equal(t, count, 3)
			assert.Equal(t, tags.Get("a"), "a_mut")
			assert.Equal(t, tags.Get("b"), "b_mut")
			assert.Equal(t, tags.Get("c"), "c_mut")
		})
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

		b.Run("SortedTagSlice", func(b *testing.B) {
			b.ReportAllocs()
			b.ResetTimer()
			tags := NewTagSlice()
			for i := 0; i < b.N; i++ {
				index := i % tagCount
				tags.Add(fmt.Sprintf("tag_%d", index), fmt.Sprintf("value_%d", i))
			}
		})

		b.Run("UnSortedTagSlice", func(b *testing.B) {
			b.ReportAllocs()
			b.ResetTimer()
			tags := NewKeyValuesSlice[string]()
			for i := 0; i < b.N; i++ {
				index := i % tagCount
				tags.Add(fmt.Sprintf("tag_%d", index), fmt.Sprintf("value_%d", i))
			}
		})
	})

	kvsToAdd := make([]KeyValue[string], 0, tagCount)
	for i := 0; i < tagCount; i++ {
		kvsToAdd = append(kvsToAdd, KeyValue[string]{Key: fmt.Sprintf("tag_%d", i), Value: fmt.Sprintf("value_%d", i)})
	}

	b.Run(fmt.Sprintf("AddAndReset(size:%d)", tagCount), func(b *testing.B) {
		b.Run("Tags", func(b *testing.B) {
			b.ReportAllocs()
			b.ResetTimer()
			tags := NewTags()
			for i := 0; i < b.N; i++ {
				for _, kv := range kvsToAdd {
					tags.Add(kv.Key, kv.Value)
				}
				tags.Reset()
			}
		})

		b.Run("SortedTagSlice", func(b *testing.B) {
			b.ReportAllocs()
			b.ResetTimer()
			tags := NewTagSlice()
			for i := 0; i < b.N; i++ {
				for _, kv := range kvsToAdd {
					tags.Add(kv.Key, kv.Value)
				}
				tags.Reset()
			}
		})

		b.Run("UnSortedTagSlice", func(b *testing.B) {
			b.ReportAllocs()
			b.ResetTimer()
			tags := NewKeyValuesSlice[string]()
			for i := 0; i < b.N; i++ {
				for _, kv := range kvsToAdd {
					tags.Add(kv.Key, kv.Value)
				}
				tags.Reset()
			}
		})
	})

	b.Run(fmt.Sprintf("Size(size:%d)", tagCount), func(b *testing.B) {
		b.Run("Tags", func(b *testing.B) {
			tags := NewTags()
			for i := 0; i < tagCount; i++ {
				tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				_ = tags.Size()
			}
		})

		b.Run("SortedTagSlice", func(b *testing.B) {
			tags := NewTagSlice()
			for i := 0; i < tagCount; i++ {
				tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				_ = tags.Size()
			}
		})

		b.Run("UnSortedTagSlice", func(b *testing.B) {
			tags := NewKeyValuesSlice[string]()
			for i := 0; i < tagCount; i++ {
				tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				_ = tags.Size()
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

		b.Run("SortedTagSlice", func(b *testing.B) {
			tags := NewTagSlice()
			for i := 0; i < tagCount; i++ {
				tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				_ = tags.Iterator()
			}
		})

		b.Run("UnSortedTagSlice", func(b *testing.B) {
			tags := NewKeyValuesSlice[string]()
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

		b.Run("SortedTagSlice", func(b *testing.B) {
			tags := NewTagSlice()
			for i := 0; i < tagCount; i++ {
				tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				_ = tags.SortTo(nil)
			}
		})

		b.Run("UnSortedTagSlice", func(b *testing.B) {
			tags := NewKeyValuesSlice[string]()
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
		b.Run("SortedTagSlice", func(b *testing.B) {
			tags := NewTagSlice()
			for i := 0; i < tagCount; i++ {
				tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				_ = tags.ToArray()
			}
		})

		b.Run("UnSortedTagSlice", func(b *testing.B) {
			tags := NewKeyValuesSlice[string]()
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

	b.Run(fmt.Sprintf("Merge(size:%d)", tagCount), func(b *testing.B) {
		b.Run("Tags", func(b *testing.B) {
			tags := NewTags()
			for i := 0; i < tagCount; i++ {
				tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				tags.Merge(NewTags())
			}
		})
		b.Run("SortedTagSlice", func(b *testing.B) {
			tags := NewTagSlice()
			for i := 0; i < tagCount; i++ {
				tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				tags.Merge(NewTags())
			}
		})

		b.Run("UnSortedTagSlice", func(b *testing.B) {
			tags := NewKeyValuesSlice[string]()
			for i := 0; i < tagCount; i++ {
				tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				tags.Merge(NewTags())
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
		b.Run("SortedTagSlice", func(b *testing.B) {
			tags := NewTagSlice()
			for i := 0; i < tagCount; i++ {
				tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
			}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				tags.Delete(fmt.Sprintf("tag_%d", 4))
			}
		})

		b.Run("UnSortedTagSlice", func(b *testing.B) {
			tags := NewKeyValuesSlice[string]()
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

func TestTags_Size(t *testing.T) {
	tags := NewTags()
	tags.Add("tag1", "value1")
	tags.Add("tag2", "value2")
	if tags.Size() != 20 {
		t.Errorf("Tags.Size() = %d, want 20", tags.Size())
	}

	kv := NewKeyValues[float64]()
	kv.Add("key1", 1)
	kv.Add("key2", 2)
	if kv.Size() != 24 {
		t.Errorf("KeyValues.Size() = %d, want 24", kv.Size())
	}
}

func TestClone(t *testing.T) {
	metadata := NewMetadata()
	metadata.Add("key1", "value1")
	metadata.Add("key2", "value2")
	clone := metadata.Clone()
	assert.Equal(t, metadata.Size(), clone.Size())
	assert.NotSame(t, metadata, clone)
	assert.Equal(t, metadata.SortTo(nil), clone.SortTo(nil))

	tags := NewTags()
	tags.Add("tag1", "value1")
	tags.Add("tag2", "value2")
	assert.Equal(t, tags.Len(), 2)
	clone = tags.Clone()
	assert.Equal(t, tags.Size(), clone.Size())
	assert.NotSame(t, tags, clone)
	assert.Equal(t, tags.SortTo(nil), clone.SortTo(nil))

	sortedTags := NewTagSlice()
	sortedTags.Merge(tags)
	clone = sortedTags.Clone()
	assert.Equal(t, sortedTags.Len(), 2)
	assert.Equal(t, clone.Size(), sortedTags.Size())
	assert.Equal(t, clone.SortTo(nil), sortedTags.SortTo(nil))

	unsortedKeyValueSlice := NewKeyValuesSlice[string]()
	unsortedKeyValueSlice.Merge(tags)
	clone = unsortedKeyValueSlice.Clone()
	assert.Equal(t, unsortedKeyValueSlice.Len(), 2)
	assert.Equal(t, clone.Size(), unsortedKeyValueSlice.Size())
	assert.Equal(t, clone.SortTo(nil), unsortedKeyValueSlice.SortTo(nil))
}

func BenchmarkSortedAndCheckSorted(b *testing.B) {
	tags := NewTagSlice().(*sortedStrKeyValuesSliceImpl)
	tagCount := 100
	for i := 0; i < tagCount; i++ {
		tags.Add(fmt.Sprintf("tag_%d", i), fmt.Sprintf("value_%d", i))
	}
	b.ReportAllocs()
	b.ResetTimer()

	b.Run("CheckSorted", func(b *testing.B) {
		for i := 0; i < b.N; i++ {
			tags.checkSorted()
		}
	})

	b.Run("Sort", func(b *testing.B) {
		for i := 0; i < b.N; i++ {
			sortKeyValues(tags.kvs)
		}
	})
}

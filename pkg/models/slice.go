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
	"sort"
	"unsafe"
)

var (
	_                   KeyValues[string]  = (*sortedKeyValuesSliceImpl[string])(nil)
	_                   Tags               = (*sortedStrKeyValuesSliceImpl)(nil)
	_                   KeyValues[float64] = (*keyValuesSliceImpl[float64])(nil)
	recycleCapThreshold                    = 1024 * 1024
)

// SetKeyValuesSliceRecycleCapThreshold sets the threshold for slice recycle.
// The threshold is used to determine whether the slice should be recycled.
// Note this func is not thread-safe and should be called in initialization.
func SetKeyValuesSliceRecycleCapThreshold(threshold int) {
	recycleCapThreshold = threshold
}

// sortedStrKeyValuesSliceImpl implements Tags.
// It is actually a wrapper of keyValuesSliceImpl[string] with extra Size() and Clone() method.
type sortedStrKeyValuesSliceImpl struct {
	sortedKeyValuesSliceImpl[string]
}

// Size returns the size of the keyValuesMapImpl.
func (s *sortedStrKeyValuesSliceImpl) Size() int {
	if s == nil || s.IsNil() {
		return 0
	}

	size := 0
	for _, kv := range s.kvs {
		size += len(kv.Key) + len(kv.Value)
	}
	return size
}

func (s *sortedStrKeyValuesSliceImpl) Clone() KeyValues[string] {
	if s == nil {
		return nil
	}
	newKVs := s.SortTo(nil)
	return &sortedStrKeyValuesSliceImpl{
		sortedKeyValuesSliceImpl: sortedKeyValuesSliceImpl[string]{kvs: newKVs},
	}
}

func NewTagSlice() Tags {
	return &sortedStrKeyValuesSliceImpl{
		sortedKeyValuesSliceImpl: sortedKeyValuesSliceImpl[string]{
			kvs: make([]KeyValue[string], 0),
		},
	}
}

func NewTagSliceWithMap(items map[string]string) Tags {
	tags := NewTagSlice()
	tags.AddAll(items)
	return tags
}

// NewTagSliceFrom creates a new keyValuesSliceImpl from the given kvs.
// If sorted is true, the kvs is assumed to be sorted.
// For many cases, like influxdb, prometheus, the kvs is already sorted, so we can avoid sorting it again.
func NewTagSliceFrom(kvs []KeyValue[string], sorted bool) Tags {
	s := &sortedStrKeyValuesSliceImpl{sortedKeyValuesSliceImpl[string]{kvs: kvs}}
	if sorted {
		return s
	}
	sortKeyValues(kvs)
	return s
}

func NewSortedKeyValuesSlice[TValue string | float64 | *TypedValue | any]() KeyValues[TValue] {
	return &sortedKeyValuesSliceImpl[TValue]{}
}

// sortedKeyValuesSliceImpl implements KeyValues.
// The elements are sorted by key lexicographically.
type sortedKeyValuesSliceImpl[TValue string | float64 | *TypedValue | any] struct {
	kvs []KeyValue[TValue] // kvs is sorted by key ascending.
}

func (s *sortedKeyValuesSliceImpl[TValue]) Size() int {
	size := 0
	for _, kv := range s.kvs {
		size += len(kv.Key) + doubleValueBytes
	}
	return size
}

func (s *sortedKeyValuesSliceImpl[TValue]) Add(key string, value TValue) {
	// In most cases, we add kvs lexicographically, which should directly append to the end of the slice.
	if len(s.kvs) == 0 || s.kvs[len(s.kvs)-1].Key < key {
		s.kvs = append(s.kvs, KeyValue[TValue]{Key: key, Value: value})
		return
	}

	// binary search the first key that is greater than or equal to key.
	index := sort.Search(len(s.kvs), func(i int) bool {
		return s.kvs[i].Key >= key
	})

	if s.kvs[index].Key == key {
		s.kvs[index].Value = value
	} else {
		// insert the key-value pair at the index.
		s.kvs = append(s.kvs, KeyValue[TValue]{})
		copy(s.kvs[index+1:], s.kvs[index:])
		s.kvs[index] = KeyValue[TValue]{Key: key, Value: value}
	}
}

func (s *sortedKeyValuesSliceImpl[TValue]) AddAll(items map[string]TValue) {
	if len(items) == 0 {
		return
	}

	newKavs := make([]KeyValue[TValue], 0, len(items))
	for k, v := range items {
		newKavs = append(newKavs, KeyValue[TValue]{Key: k, Value: v})
	}
	sortKeyValues(newKavs)
	s.mergeSorted(newKavs)
}

func (s *sortedKeyValuesSliceImpl[TValue]) Get(key string) TValue {
	var value TValue
	index := sort.Search(len(s.kvs), func(i int) bool {
		return s.kvs[i].Key >= key
	})
	if index < len(s.kvs) && s.kvs[index].Key == key {
		value = s.kvs[index].Value
	}
	return value
}

func (s *sortedKeyValuesSliceImpl[TValue]) Contains(key string) bool {
	index := sort.Search(len(s.kvs), func(i int) bool {
		return s.kvs[i].Key >= key
	})
	return index < len(s.kvs) && s.kvs[index].Key == key
}

func (s *sortedKeyValuesSliceImpl[TValue]) LoadOrStore(key string, value TValue) (TValue, bool) {
	index := sort.Search(len(s.kvs), func(i int) bool {
		return s.kvs[i].Key >= key
	})
	if index < len(s.kvs) && s.kvs[index].Key == key {
		return s.kvs[index].Value, true
	}
	s.kvs = append(s.kvs, KeyValue[TValue]{})
	copy(s.kvs[index+1:], s.kvs[index:])
	s.kvs[index] = KeyValue[TValue]{Key: key, Value: value}
	return value, false
}

func (s *sortedKeyValuesSliceImpl[TValue]) Delete(key string) {
	index := sort.Search(len(s.kvs), func(i int) bool {
		return s.kvs[i].Key >= key
	})
	if index < len(s.kvs) && s.kvs[index].Key == key {
		s.kvs = append(s.kvs[:index], s.kvs[index+1:]...)
	}
}

func (s *sortedKeyValuesSliceImpl[TValue]) Merge(other KeyValues[TValue]) {
	if other.IsNil() {
		return
	}
	if other.IsSorted() {
		s.mergeSorted(other.ToArray())
	} else {
		s.AddAll(other.Iterator())
	}
}

func (s *sortedKeyValuesSliceImpl[TValue]) mergeSorted(otherSorted []KeyValue[TValue]) {
	if len(otherSorted) == 0 {
		return
	}

	originLen := len(s.kvs)
	totalLen := originLen + len(otherSorted)
	s.kvs = append(s.kvs, otherSorted...)

	i := originLen - 1
	j := len(otherSorted) - 1
	k := totalLen - 1
	duplicates := 0

	// directly use s.kvs to store the merged result to avoid allocation and memmove in most cases
	// +-----------------+--------------------+
	// |     origin      |  other to merge    |
	// +-----------------+--------------------+
	// |                 |                    |
	// 0-----------------i--------------------k
	//
	// +--------------------+
	// |    raw  other      |
	// 0--------------------+
	for i >= 0 && j >= 0 {
		switch {
		case s.kvs[i].Key > otherSorted[j].Key:
			s.kvs[k] = s.kvs[i]
			i--
			k--
		case s.kvs[i].Key < otherSorted[j].Key:
			s.kvs[k] = otherSorted[j]
			j--
			k--
		default:
			// keys are equal, keep the one from otherSorted, which has higher priority.
			s.kvs[k] = otherSorted[j]
			i-- // skip the originone from s.kvs
			j--
			k--
			duplicates++ // mark there is a duplicate key
		}
	}

	for i >= 0 {
		s.kvs[k] = s.kvs[i]
		i--
		k--
	}

	for j >= 0 {
		s.kvs[k] = otherSorted[j]
		j--
		k--
	}

	// The first $duplicates kvs are the useless ones from s.kvs and may be polluted.
	// we need to remove them, just update the slice header to skip them.
	s.kvs = s.kvs[duplicates:totalLen]
}

func (s *sortedKeyValuesSliceImpl[TValue]) Iterator() map[string]TValue {
	if len(s.kvs) == 0 {
		return nil
	}

	res := make(map[string]TValue, len(s.kvs))
	for _, kv := range s.kvs {
		res[kv.Key] = kv.Value
	}
	return res
}

func (s *sortedKeyValuesSliceImpl[TValue]) Range(f func(key string, value TValue) bool) {
	// reverse iterate to handle deletion
	for i := len(s.kvs) - 1; i >= 0; i-- {
		kv := s.kvs[i]
		if !f(kv.Key, kv.Value) {
			break
		}
	}
}

func (s *sortedKeyValuesSliceImpl[TValue]) RangeMut(f func(key string, value TValue) MutResult[TValue]) {
	sorted := true
	// reverse iterate to handle deletion
	for i := len(s.kvs) - 1; i >= 0; i-- {
		kv := s.kvs[i]
		result := f(kv.Key, kv.Value)

		if result.Replace {
			if result.NewKeyValue.Key == kv.Key {
				s.kvs[i].Value = result.NewKeyValue.Value
			} else {
				s.kvs[i] = result.NewKeyValue
				sorted = false
			}
		}

		if !result.Continue {
			break
		}
	}

	if !sorted {
		s.sort()
	}
}

func (s *sortedKeyValuesSliceImpl[TValue]) checkSorted() bool {
	kvs := s.kvs
	n := len(kvs)
	if n < 2 {
		return true
	}

	for i := 0; i < n-1; i++ {
		if kvs[i].Key > kvs[i+1].Key {
			return false
		}
	}
	return true
}

func (s *sortedKeyValuesSliceImpl[TValue]) sort() {
	sorted := s.checkSorted()
	if sorted {
		return
	}
	sortKeyValues(s.kvs)
}

func (s *sortedKeyValuesSliceImpl[TValue]) ToArray() []KeyValue[TValue] {
	return s.kvs
}

func (s *sortedKeyValuesSliceImpl[TValue]) IsSorted() bool {
	return true
}

func (s *sortedKeyValuesSliceImpl[TValue]) SortTo(buf []KeyValue[TValue]) []KeyValue[TValue] {
	return append(buf, s.kvs...)
}

func (s *sortedKeyValuesSliceImpl[TValue]) Len() int {
	return len(s.kvs)
}

func (s *sortedKeyValuesSliceImpl[TValue]) IsNil() bool {
	return s == nil || len(s.kvs) == 0
}

func (s *sortedKeyValuesSliceImpl[TValue]) Reset() {
	if s.IsNil() {
		return
	}
	if cap(s.kvs) > recycleCapThreshold {
		s.kvs = make([]KeyValue[TValue], 0)
		return
	}
	s.kvs = s.kvs[:0]
}

func (s *sortedKeyValuesSliceImpl[TValue]) Clone() KeyValues[TValue] {
	if s == nil {
		return nil
	}

	newKvs := make([]KeyValue[TValue], len(s.kvs))
	copy(newKvs, s.kvs)
	return &sortedKeyValuesSliceImpl[TValue]{
		kvs: newKvs,
	}
}

// NewKeyValuesSlice creates a new KeyValues slice.
// The KeyValues slice is not sorted, Users should keep keys are unique.
func NewKeyValuesSlice[TValue string | float64 | *TypedValue | any]() KeyValues[TValue] {
	return &keyValuesSliceImpl[TValue]{}
}

// NewKeyValuesSliceFrom creates a new KeyValues slice from a slice of KeyValue.
// The KeyValues slice is not sorted and de-duplicated.
// Users should make sure keys are unique.
func NewKeyValuesSliceFrom[TValue string | float64 | *TypedValue | any](kvs []KeyValue[TValue], needDeDuplicate bool) KeyValues[TValue] {
	kv := &keyValuesSliceImpl[TValue]{
		kvs: kvs,
	}
	if needDeDuplicate {
		kv.deDuplicate()
	}
	return kv
}

// keyValuesSliceImpl is a slice of KeyValue, not sorted.
// Note users must make sure the key is unique
type keyValuesSliceImpl[TValue string | float64 | *TypedValue | any] struct {
	kvs []KeyValue[TValue]
}

func (k *keyValuesSliceImpl[TValue]) deDuplicate() {
	if len(k.kvs) == 0 {
		return
	}
	seen := make(map[string]struct{})
	newKvs := k.kvs[:0]
	for _, kv := range k.kvs {
		if _, ok := seen[kv.Key]; !ok {
			seen[kv.Key] = struct{}{}
			newKvs = append(newKvs, kv)
		}
	}
	k.kvs = newKvs
}

func (k *keyValuesSliceImpl[TValue]) Add(key string, value TValue) {
	// reverse order
	for i := len(k.kvs) - 1; i >= 0; i-- {
		kv := k.kvs[i]
		if kv.Key == key {
			k.kvs[i].Value = value
			return
		}
	}
	k.kvs = append(k.kvs, KeyValue[TValue]{Key: key, Value: value})
}

func (k *keyValuesSliceImpl[TValue]) AddAll(items map[string]TValue) {
	for key, value := range items {
		k.Add(key, value)
	}
}

func (k *keyValuesSliceImpl[TValue]) Get(key string) TValue {
	var value TValue
	for _, kv := range k.kvs {
		if kv.Key == key {
			value = kv.Value
			break
		}
	}
	return value
}

func (k *keyValuesSliceImpl[TValue]) Contains(key string) bool {
	for _, kv := range k.kvs {
		if kv.Key == key {
			return true
		}
	}
	return false
}

func (k *keyValuesSliceImpl[TValue]) Delete(key string) {
	// reverse order
	for i := len(k.kvs) - 1; i >= 0; i-- {
		kv := k.kvs[i]
		if kv.Key == key {
			k.kvs = append(k.kvs[:i], k.kvs[i+1:]...)
		}
	}
}

func (k *keyValuesSliceImpl[TValue]) Merge(other KeyValues[TValue]) {
	if other.IsNil() {
		return
	}

	other.Range(func(key string, value TValue) bool {
		k.Add(key, value)
		return true
	})
}

func (k *keyValuesSliceImpl[TValue]) Iterator() map[string]TValue {
	if len(k.kvs) == 0 {
		return nil
	}
	res := make(map[string]TValue, len(k.kvs))
	for _, kv := range k.kvs {
		res[kv.Key] = kv.Value
	}
	return res
}

func (k *keyValuesSliceImpl[TValue]) ToArray() []KeyValue[TValue] {
	return k.kvs
}

func (k *keyValuesSliceImpl[TValue]) LoadOrStore(key string, value TValue) (TValue, bool) {
	for _, kv := range k.kvs {
		if kv.Key == key {
			return kv.Value, true
		}
	}
	k.Add(key, value)
	return value, false
}

func (k *keyValuesSliceImpl[TValue]) Range(f func(key string, value TValue) bool) {
	// reverse order
	for i := len(k.kvs) - 1; i >= 0; i-- {
		kv := k.kvs[i]
		if !f(kv.Key, kv.Value) {
			break
		}
	}
}

func (k *keyValuesSliceImpl[TValue]) RangeMut(f func(key string, value TValue) MutResult[TValue]) {
	// reverse order
	for i := len(k.kvs) - 1; i >= 0; i-- {
		kv := k.kvs[i]
		result := f(kv.Key, kv.Value)

		if result.Replace {
			if result.NewKeyValue.Key == kv.Key {
				k.kvs[i].Value = result.NewKeyValue.Value
			} else {
				k.kvs[i] = result.NewKeyValue
			}
		}

		if !result.Continue {
			break
		}
	}
}

func (k *keyValuesSliceImpl[TValue]) IsSorted() bool {
	return false
}

func (k *keyValuesSliceImpl[TValue]) SortTo(buf []KeyValue[TValue]) []KeyValue[TValue] {
	buf = append(buf, k.kvs...)
	sortKeyValues(buf)
	return buf
}

func (k *keyValuesSliceImpl[TValue]) Len() int {
	return len(k.kvs)
}

func (k *keyValuesSliceImpl[TValue]) Size() int {
	size := 0
	for _, kv := range k.kvs {
		size += len(kv.Key)
	}
	return size
}

func (k *keyValuesSliceImpl[TValue]) IsNil() bool {
	return k == nil || len(k.kvs) == 0
}

func (k *keyValuesSliceImpl[TValue]) Reset() {
	if k.IsNil() {
		return
	}

	if cap(k.kvs) > recycleCapThreshold {
		k.kvs = make([]KeyValue[TValue], 0)
		return
	}
	k.kvs = k.kvs[:0]
}

func (k *keyValuesSliceImpl[TValue]) Clone() KeyValues[TValue] {
	if k == nil {
		return nil
	}

	newKvs := make([]KeyValue[TValue], len(k.kvs))
	copy(newKvs, k.kvs)

	return &keyValuesSliceImpl[TValue]{
		kvs: newKvs,
	}
}

// noescape hides a pointer from escape analysis. It is the identity function
// but escape analysis doesn't think the output depends on the input.
// noescape is inlined and currently compiles down to zero instructions.
// USE CAREFULLY!
// This was copied from the runtime; see issues 23382 and 7921.
//
//go:nosplit
//go:nocheckptr
func noescape(p unsafe.Pointer) unsafe.Pointer {
	x := uintptr(p)
	/* #nosec G103 */
	return unsafe.Pointer(x ^ 0) //nolint:all
}

func sortKeyValues[TValue string | float64 | *TypedValue | any](kvs []KeyValue[TValue]) {
	/* #nosec G103 */
	sort.Sort((*KeyValueSlice[TValue])(noescape(unsafe.Pointer(&kvs))))
}

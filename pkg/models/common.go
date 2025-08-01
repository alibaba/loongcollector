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
	"sort"
)

type EventType int

const (
	_ EventType = iota
	EventTypeMetric
	EventTypeSpan
	EventTypeLogging
	EventTypeByteArray
)

type ValueType int

const (
	_ ValueType = iota
	ValueTypeString
	ValueTypeBoolean
	ValueTypeArray
	ValueTypeMap

	ContentKey = "content"
	BodyKey    = ContentKey
)

const (
	intValueBytes    = 4
	longValueBytes   = 8
	doubleValueBytes = 8
)

var (
	NilStringValues    = &keyValuesNil[string]{m: make(map[string]string)}
	NilTypedValues     = &keyValuesNil[*TypedValue]{m: make(map[string]*TypedValue)}
	NilFloatValues     = &keyValuesNil[float64]{m: make(map[string]float64)}
	NilInterfaceValues = &keyValuesNil[interface{}]{m: make(map[string]interface{})}
)

type TypedValue struct {
	Type  ValueType
	Value interface{}
}

type KeyValue[TValue string | float64 | *TypedValue | any] struct {
	Key   string
	Value TValue
}

type KeyValueSlice[TValue string | float64 | *TypedValue | any] []KeyValue[TValue]

func (x KeyValueSlice[TValue]) Len() int           { return len(x) }
func (x KeyValueSlice[TValue]) Less(i, j int) bool { return x[i].Key < x[j].Key }
func (x KeyValueSlice[TValue]) Swap(i, j int)      { x[i], x[j] = x[j], x[i] }

type KeyValues[TValue string | float64 | *TypedValue | any] interface {
	Add(key string, value TValue)

	AddAll(items map[string]TValue)

	Get(key string) TValue

	Contains(key string) bool

	Delete(key string)

	Merge(other KeyValues[TValue])

	Iterator() map[string]TValue

	ToArray() []KeyValue[TValue]

	IsSorted() bool

	SortTo(buf []KeyValue[TValue]) []KeyValue[TValue]

	Len() int

	IsNil() bool
}

type Tags KeyValues[string]

type Metadata KeyValues[string]

type keyValuesImpl[TValue string | float64 | *TypedValue | any] struct {
	keyValues map[string]TValue
}

func (kv *keyValuesImpl[TValue]) values() (map[string]TValue, bool) {
	if kv == nil || kv.keyValues == nil {
		return nil, false
	}
	return kv.keyValues, true
}

func (kv *keyValuesImpl[TValue]) Add(key string, value TValue) {
	if values, ok := kv.values(); ok {
		values[key] = value
	}
}

func (kv *keyValuesImpl[TValue]) AddAll(items map[string]TValue) {
	if values, ok := kv.values(); ok {
		for key, value := range items {
			values[key] = value
		}
	}
}

func (kv *keyValuesImpl[TValue]) Get(key string) TValue {
	if values, ok := kv.values(); ok {
		return values[key]
	}
	var null TValue
	return null
}

func (kv *keyValuesImpl[TValue]) Contains(key string) bool {
	if values, ok := kv.values(); ok {
		_, valueOk := values[key]
		return valueOk
	}
	return false
}

func (kv *keyValuesImpl[TValue]) Delete(key string) {
	if _, ok := kv.values(); ok {
		delete(kv.keyValues, key)
	}
}

func (kv *keyValuesImpl[TValue]) Merge(other KeyValues[TValue]) {
	if values, ok := kv.values(); ok {
		for k, v := range other.Iterator() {
			values[k] = v
		}
	}
}

func (kv *keyValuesImpl[TValue]) Iterator() map[string]TValue {
	if values, ok := kv.values(); ok {
		return values
	}
	return nil
}

func (kv *keyValuesImpl[TValue]) ToArray() []KeyValue[TValue] {
	if values, ok := kv.values(); ok {
		array := make([]KeyValue[TValue], 0, len(values))
		for k, v := range values {
			array = append(array, KeyValue[TValue]{Key: k, Value: v})
		}
		return array
	}
	return nil
}

func (kv *keyValuesImpl[TValue]) Len() int {
	if values, ok := kv.values(); ok {
		return len(values)
	}
	return 0
}

func (kv *keyValuesImpl[TValue]) IsNil() bool {
	return false
}

func (kv *keyValuesImpl[TValue]) IsSorted() bool {
	return false
}

func (kv *keyValuesImpl[TValue]) SortTo(buf []KeyValue[TValue]) []KeyValue[TValue] {
	values, ok := kv.values()
	if !ok {
		buf = buf[:0]
		return buf
	}
	if buf == nil {
		buf = make([]KeyValue[TValue], 0, len(values))
	} else {
		buf = buf[:0]
	}

	for k, v := range values {
		buf = append(buf, KeyValue[TValue]{Key: k, Value: v})
	}
	sort.Sort(KeyValueSlice[TValue](buf))
	return buf
}

type keyValuesNil[TValue string | float64 | *TypedValue | any] struct {
	m map[string]TValue
}

func (kv *keyValuesNil[TValue]) Add(key string, value TValue) {
}

func (kv *keyValuesNil[TValue]) AddAll(items map[string]TValue) {
}

func (kv *keyValuesNil[TValue]) Get(key string) TValue {
	var null TValue
	return null
}

func (kv *keyValuesNil[TValue]) Contains(key string) bool {
	return false
}

func (kv *keyValuesNil[TValue]) Delete(key string) {
}

func (kv *keyValuesNil[TValue]) Merge(other KeyValues[TValue]) {
}

func (kv *keyValuesNil[TValue]) Iterator() map[string]TValue {
	return kv.m
}

func (kv *keyValuesNil[TValue]) ToArray() []KeyValue[TValue] {
	return nil
}

func (kv *keyValuesNil[TValue]) Len() int {
	return 0
}

func (kv *keyValuesNil[TValue]) IsNil() bool {
	return true
}

func (kv *keyValuesNil[TValue]) IsSorted() bool {
	return true
}

func (kv *keyValuesNil[TValue]) SortTo(buf []KeyValue[TValue]) []KeyValue[TValue] {
	return nil
}

func NewKeyValues[TValue string | float64 | *TypedValue | any]() KeyValues[TValue] {
	return &keyValuesImpl[TValue]{
		keyValues: make(map[string]TValue),
	}
}

var _ KeyValues[string] = (*sortedKeyValues)(nil)

type sortedKeyValues struct {
	kvs []KeyValue[string] // kvs is sorted by key ascending.
}

func NewSortedKeyValues() KeyValues[string] {
	return &sortedKeyValues{}
}

// NewSortedKeyValuesFrom creates a new sortedKeyValues from the given kvs.
// If sorted is true, the kvs is assumed to be sorted.
// For many cases, like influxdb, prometheus, the kvs is already sorted, so we can avoid sorting it again.
func NewSortedKeyValuesFrom(kvs []KeyValue[string], sorted bool) KeyValues[string] {
	s := &sortedKeyValues{kvs: kvs}
	if sorted {
		return s
	}
	sort.Sort(KeyValueSlice[string](s.kvs))
	return s
}

func (s *sortedKeyValues) Add(key string, value string) {
	// In most cases, we add kvs lexicographically, which should directly append to the end of the slice.
	if len(s.kvs) == 0 || s.kvs[len(s.kvs)-1].Key < key {
		s.kvs = append(s.kvs, KeyValue[string]{Key: key, Value: value})
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
		if cap(s.kvs) == len(s.kvs) {
			newKvs := make([]KeyValue[string], len(s.kvs)+1)
			copy(newKvs, s.kvs[:index])
			newKvs[index] = KeyValue[string]{Key: key, Value: value}
			copy(newKvs[index+1:], s.kvs[index:])
			s.kvs = newKvs
		} else {
			s.kvs = s.kvs[:len(s.kvs)+1]
			copy(s.kvs[index+1:], s.kvs[index:])
			s.kvs[index] = KeyValue[string]{Key: key, Value: value}
		}
	}
}

func (s *sortedKeyValues) AddAll(items map[string]string) {
	for k, v := range items {
		s.kvs = append(s.kvs, KeyValue[string]{Key: k, Value: v})
	}
	sort.Sort(KeyValueSlice[string](s.kvs))
}

func (s *sortedKeyValues) Get(key string) string {
	var value string
	index := sort.Search(len(s.kvs), func(i int) bool {
		return s.kvs[i].Key >= key
	})
	if index < len(s.kvs) && s.kvs[index].Key == key {
		value = s.kvs[index].Value
	}
	return value
}

func (s *sortedKeyValues) Contains(key string) bool {
	index := sort.Search(len(s.kvs), func(i int) bool {
		return s.kvs[i].Key >= key
	})
	return index < len(s.kvs) && s.kvs[index].Key == key
}

func (s *sortedKeyValues) Delete(key string) {
	index := sort.Search(len(s.kvs), func(i int) bool {
		return s.kvs[i].Key >= key
	})
	if index < len(s.kvs) && s.kvs[index].Key == key {
		s.kvs = append(s.kvs[:index], s.kvs[index+1:]...)
	}
}

func (s *sortedKeyValues) Merge(other KeyValues[string]) {
	s.AddAll(other.Iterator())
}

func (s *sortedKeyValues) Iterator() map[string]string {
	if len(s.kvs) == 0 {
		return nil
	}

	res := make(map[string]string, len(s.kvs))
	for _, kv := range s.kvs {
		res[kv.Key] = kv.Value
	}
	return res
}

func (s *sortedKeyValues) ToArray() []KeyValue[string] {
	return s.kvs
}

func (s *sortedKeyValues) IsSorted() bool {
	return true
}

func (s *sortedKeyValues) SortTo(buf []KeyValue[string]) []KeyValue[string] {
	return append(buf, s.kvs...)
}

func (s *sortedKeyValues) Len() int {
	return len(s.kvs)
}

func (s *sortedKeyValues) IsNil() bool {
	return s == nil || len(s.kvs) == 0
}

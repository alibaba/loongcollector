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
	// Add adds a key-value pair to the inner collector.
	// If the key already exists, the value will be updated.
	Add(key string, value TValue)

	// AddAll adds multiple key-value pairs to the inner collector.
	// If a key already exists, the value will be updated.
	AddAll(items map[string]TValue)

	// Get returns the value for the key if it exists,
	// otherwise it returns the zero value.
	Get(key string) TValue

	// Contains returns true if the key exists.
	Contains(key string) bool

	// Delete deletes the key-value pair for the key.
	// If the key does not exist, the inner collector is unchanged.
	Delete(key string)

	// Merge merges the key-value pairs from the other KeyValues into this KeyValues's inner collector.
	// If a key already exists, the value will be updated.
	Merge(other KeyValues[TValue])

	// Iterator returns a map of all key-value pairs in the inner collector.
	// Recommend using Range instead unless you have to get a hashmap.
	Iterator() map[string]TValue

	// ToArray returns a slice of all key-value pairs in the inner collector.
	// Recommend using Range instead unless you have to get a slice.
	ToArray() []KeyValue[TValue]

	// LoadOrStore returns the value for the key if it exists,
	// otherwise it stores the given value and returns the given value.
	// The loaded result is true if the value was loaded, false if stored.
	LoadOrStore(key string, value TValue) (TValue, bool)

	// Range calls f sequentially for each key and value present in the inner collector.
	// If f returns false, range stops the iteration.
	// Note:
	// anonymous function will cause heap allocation.
	// the order of iteration is not guaranteed.
	Range(f func(key string, value TValue) bool)

	// RangeMut calls f sequentially for each key and value present in the inner collector.
	// f can return a new value, a boolean to indicate if the value should be replaced, and a boolean to indicate if the iteration should continue.
	// If f returns false, range stops the iteration.
	// Note:
	// anonymous function will cause heap allocation.
	// the order of iteration is not guaranteed.
	RangeMut(f func(key string, value TValue) MutResult[TValue])

	// IsSorted returns true if the map is sorted by key.
	IsSorted() bool

	// SortTo sorts the map by key and returns a slice of all key-value pairs in the map.
	// It will not modify the original inner collector.
	SortTo(buf []KeyValue[TValue]) []KeyValue[TValue]

	// Len returns the number of key-value pairs in the map.
	Len() int

	// Size returns the estimated size of the keyValuesMap implementation.
	// Note: This size is for statistical purposes and does not represent the actual memory usage.
	Size() int

	// IsNil returns true if the map is nil or empty.
	IsNil() bool

	// Reset resets the inner collector for reusage.
	Reset()

	// Clone returns a deep copy of the kvs.
	Clone() KeyValues[TValue]
}

// MutResult contains the result of a mutation operation in RangeMut
// It provides clear field names instead of positional return values
type MutResult[TValue any] struct {
	NewKeyValue KeyValue[TValue] // The new value to store if Replace is true
	Replace     bool             // Whether to replace the current value with NewValue
	Continue    bool             // Whether to continue iterating to the next key
}

func NewMutResult[TValue any](newValue KeyValue[TValue], replace bool, cont bool) MutResult[TValue] {
	return MutResult[TValue]{
		NewKeyValue: newValue,
		Replace:     replace,
		Continue:    cont,
	}
}

type Tags = KeyValues[string]

type Metadata = KeyValues[string]

type keyValuesMapImpl[TValue string | float64 | *TypedValue | any] struct {
	keyValues map[string]TValue
}

func (kv *keyValuesMapImpl[TValue]) values() (map[string]TValue, bool) {
	if kv == nil || kv.keyValues == nil {
		return nil, false
	}
	return kv.keyValues, true
}

func (kv *keyValuesMapImpl[TValue]) Add(key string, value TValue) {
	if values, ok := kv.values(); ok {
		values[key] = value
	}
}

func (kv *keyValuesMapImpl[TValue]) AddAll(items map[string]TValue) {
	if values, ok := kv.values(); ok {
		for key, value := range items {
			values[key] = value
		}
	}
}

func (kv *keyValuesMapImpl[TValue]) Get(key string) TValue {
	if values, ok := kv.values(); ok {
		return values[key]
	}
	var null TValue
	return null
}

func (kv *keyValuesMapImpl[TValue]) LoadOrStore(key string, value TValue) (TValue, bool) {
	if values, ok := kv.values(); ok {
		if v, ok := values[key]; ok {
			return v, true
		}
	}
	kv.Add(key, value)
	return value, false
}

func (kv *keyValuesMapImpl[TValue]) IsSorted() bool {
	return false
}

func (kv *keyValuesMapImpl[TValue]) Contains(key string) bool {
	if values, ok := kv.values(); ok {
		_, valueOk := values[key]
		return valueOk
	}
	return false
}

func (kv *keyValuesMapImpl[TValue]) Delete(key string) {
	if _, ok := kv.values(); ok {
		delete(kv.keyValues, key)
	}
}

func (kv *keyValuesMapImpl[TValue]) Merge(other KeyValues[TValue]) {
	if values, ok := kv.values(); ok {
		for k, v := range other.Iterator() {
			values[k] = v
		}
	}
}

func (kv *keyValuesMapImpl[TValue]) Iterator() map[string]TValue {
	if values, ok := kv.values(); ok {
		return values
	}
	return nil
}

func (kv *keyValuesMapImpl[TValue]) Range(f func(key string, value TValue) bool) {
	if values, ok := kv.values(); ok {
		for k, v := range values {
			if !f(k, v) {
				break
			}
		}
	}
}

func (kv *keyValuesMapImpl[TValue]) RangeMut(f func(key string, value TValue) MutResult[TValue]) {
	if values, ok := kv.values(); ok {
		for k, v := range values {
			result := f(k, v)

			if result.Replace {
				if result.NewKeyValue.Key == k {
					values[k] = result.NewKeyValue.Value
				} else {
					delete(values, k)
					values[result.NewKeyValue.Key] = result.NewKeyValue.Value
				}
			}

			if !result.Continue {
				break
			}
		}
	}
}

func (kv *keyValuesMapImpl[TValue]) ToArray() []KeyValue[TValue] {
	if values, ok := kv.values(); ok {
		array := make([]KeyValue[TValue], 0, len(values))
		for k, v := range values {
			array = append(array, KeyValue[TValue]{Key: k, Value: v})
		}
		return array
	}
	return nil
}

func (kv *keyValuesMapImpl[TValue]) Len() int {
	if values, ok := kv.values(); ok {
		return len(values)
	}
	return 0
}

func (kv *keyValuesMapImpl[TValue]) Size() int {
	size := 0
	if values, ok := kv.values(); ok {
		for k := range values {
			size += len(k)
			size += doubleValueBytes
		}
	}
	return size
}

func (kv *keyValuesMapImpl[TValue]) IsNil() bool {
	return kv == nil || len(kv.keyValues) == 0
}

func (kv *keyValuesMapImpl[TValue]) SortTo(buf []KeyValue[TValue]) []KeyValue[TValue] {
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

func (kv *keyValuesMapImpl[TValue]) Reset() {
	if kv.IsNil() {
		return
	}
	kv.keyValues = make(map[string]TValue)
}

func (kv *keyValuesMapImpl[TValue]) Clone() KeyValues[TValue] {
	if values, ok := kv.values(); ok {
		newValues := make(map[string]TValue, len(values))
		for k, v := range values {
			newValues[k] = v
		}
		return &keyValuesMapImpl[TValue]{
			keyValues: newValues,
		}
	}
	return nil
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

func (kv *keyValuesNil[TValue]) Size() int {
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

func (kv *keyValuesNil[TValue]) Range(f func(key string, value TValue) bool) {}

func (kv *keyValuesNil[TValue]) RangeMut(f func(key string, value TValue) MutResult[TValue]) {}

func (kv *keyValuesNil[TValue]) LoadOrStore(key string, value TValue) (TValue, bool) {
	var null TValue
	return null, false
}

func (kv *keyValuesNil[TValue]) Reset() {
}

func (kv *keyValuesNil[TValue]) Clone() KeyValues[TValue] {
	return kv
}

func NewKeyValues[TValue string | float64 | *TypedValue | any]() KeyValues[TValue] {
	return &keyValuesMapImpl[TValue]{
		keyValues: make(map[string]TValue),
	}
}

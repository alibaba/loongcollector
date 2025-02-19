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
	ValueTypeInteger
	ValueTypeUnsigned

	ContentKey = "content"
	BodyKey    = ContentKey
)

var (
	NilStringValues    = &keyValuesNil[string]{}
	NilTypedValues     = &keyValuesNil[*TypedValue]{}
	NilFloatValues     = &keyValuesNil[float64]{}
	NilInterfaceValues = &keyValuesNil[interface{}]{}
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

func (kv *keyValuesImpl[TValue]) Len() int {
	if values, ok := kv.values(); ok {
		return len(values)
	}
	return 0
}

func (kv *keyValuesImpl[TValue]) IsNil() bool {
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
	return make(map[string]TValue)
}

func (kv *keyValuesNil[TValue]) Len() int {
	return 0
}

func (kv *keyValuesNil[TValue]) IsNil() bool {
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

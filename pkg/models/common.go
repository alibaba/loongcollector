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
)

var (
	noopStringValues = &keyValuesNoop[string]{}
	noopTypedValues  = &keyValuesNoop[*TypedValue]{}
	noopFloatValues  = &keyValuesNoop[float64]{}
)

type TypedValue struct {
	Type  ValueType
	Value interface{}
}

type KeyValues[TValue string | float64 | *TypedValue] interface {
	Add(key string, value TValue)

	AddAll(items map[string]TValue)

	Get(key string) TValue

	Contains(key string) bool

	Delete(key string)

	Merge(other KeyValues[TValue])

	Iterator() map[string]TValue

	Len() int
}

type Tags interface {
	KeyValues[string]
}

type Metadata interface {
	KeyValues[string]
}

type keyValuesImpl[TValue string | float64 | *TypedValue] struct {
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
		_, ok := values[key]
		return ok
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

type keyValuesNoop[TValue string | float64 | *TypedValue] struct {
}

func (kv *keyValuesNoop[TValue]) Add(key string, value TValue) {
}

func (kv *keyValuesNoop[TValue]) AddAll(items map[string]TValue) {
}

func (kv *keyValuesNoop[TValue]) Get(key string) TValue {
	var null TValue
	return null
}

func (kv *keyValuesNoop[TValue]) Contains(key string) bool {
	return false
}

func (kv *keyValuesNoop[TValue]) Delete(key string) {
}

func (kv *keyValuesNoop[TValue]) Merge(other KeyValues[TValue]) {
}

func (kv *keyValuesNoop[TValue]) Iterator() map[string]TValue {
	return make(map[string]TValue)
}

func (kv *keyValuesNoop[TValue]) Len() int {
	return 0
}

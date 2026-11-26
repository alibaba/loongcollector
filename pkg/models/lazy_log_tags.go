// Copyright 2026 iLogtail Authors
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

import "sync/atomic"

// Native logs are decoded without individual tags on the wire. Defer the map
// until a plugin adds tags or asks for the writable map.
type lazyLogTags struct {
	values atomic.Pointer[keyValuesImpl[string]]
}

func (t *lazyLogTags) materialize() *keyValuesImpl[string] {
	if values := t.values.Load(); values != nil {
		return values
	}
	values := &keyValuesImpl[string]{keyValues: make(map[string]string)}
	if t.values.CompareAndSwap(nil, values) {
		return values
	}
	return t.values.Load()
}

func (t *lazyLogTags) Add(key, value string) { t.materialize().Add(key, value) }

func (t *lazyLogTags) AddAll(values map[string]string) {
	if len(values) > 0 {
		t.materialize().AddAll(values)
	}
}

func (t *lazyLogTags) Merge(other KeyValues[string]) {
	if other.Len() > 0 {
		t.materialize().Merge(other)
	}
}

func (t *lazyLogTags) Get(key string) string { return t.values.Load().Get(key) }

func (t *lazyLogTags) Contains(key string) bool { return t.values.Load().Contains(key) }

func (t *lazyLogTags) Delete(key string) { t.values.Load().Delete(key) }

func (t *lazyLogTags) Iterator() map[string]string { return t.materialize().Iterator() }

func (t *lazyLogTags) SortTo(buf []KeyValue[string]) []KeyValue[string] {
	values := t.values.Load()
	if values == nil && buf == nil {
		return []KeyValue[string]{}
	}
	return values.SortTo(buf)
}

func (t *lazyLogTags) Len() int { return t.values.Load().Len() }

func (*lazyLogTags) IsNil() bool { return false }

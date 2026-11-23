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
	"iter"
	"slices"
	"sort"
	"sync/atomic"
)

type orderedLogContents struct {
	keyValuesImpl[interface{}]
	keys      []string
	bodyOwned atomic.Bool
}

// NewOrderedLogContents preserves field order for inputs with an ordered row
// or a protobuf list of fields. The other log contents implementation is a map.
func NewOrderedLogContents(capacity int) LogContents {
	return &orderedLogContents{
		keyValuesImpl: keyValuesImpl[interface{}]{keyValues: make(map[string]interface{}, capacity)},
		keys:          make([]string, 0, capacity),
	}
}

// SetOwnedLogBody stores a freshly allocated body in ordered contents. A
// processor can reuse the buffer as immutable text until an API exposes it.
func SetOwnedLogBody(log *Log, body []byte) {
	log.SetBody(body)
	if ordered, ok := log.Contents.(*orderedLogContents); ok {
		ordered.bodyOwned.Store(true)
	}
}

// LookupLogContentForReplacement acquires the value and reports whether an
// unread body buffer may be reused as immutable text after replacing it.
// The caller must remove or replace the body before publishing the text.
func LookupLogContentForReplacement(contents LogContents, key string) (value any, found, owned bool) {
	if ordered, ok := contents.(*orderedLogContents); ok {
		value, found = ordered.keyValues[key]
		if key == BodyKey {
			owned = ordered.bodyOwned.Swap(false)
		}
		return
	}
	return contents.Get(key), contents.Contains(key), false
}

// ReserveLogContentKeys avoids growing the ordered key slice for each new
// field when a processor has determined how many fields it will add.
func ReserveLogContentKeys(contents LogContents, count int) {
	if ordered, ok := contents.(*orderedLogContents); ok {
		ordered.keys = slices.Grow(ordered.keys, count)
	}
}

// FirstLogContentKey reports the first remaining field in ordered contents.
func FirstLogContentKey(contents LogContents) (string, bool) {
	if ordered, ok := contents.(*orderedLogContents); ok && len(ordered.keys) > 0 {
		return ordered.keys[0], true
	}
	return "", false
}

// LogContentEntries iterates fields in input order when the log preserves it.
func LogContentEntries(contents LogContents) iter.Seq2[string, any] {
	return func(yield func(string, any) bool) {
		if ordered, ok := contents.(*orderedLogContents); ok {
			for _, key := range ordered.keys {
				if !yield(key, ordered.Get(key)) {
					return
				}
			}
			return
		}
		for key, value := range contents.Iterator() {
			if !yield(key, value) {
				return
			}
		}
	}
}

// RenameLogContent changes a field name while keeping its position in ordered contents.
func RenameLogContent(contents LogContents, source, destination string) {
	if source == destination || !contents.Contains(source) {
		return
	}
	value := contents.Get(source)
	if ordered, ok := contents.(*orderedLogContents); ok {
		ordered.Delete(destination)
		if index := slices.Index(ordered.keys, source); index >= 0 {
			ordered.keys[index] = destination
			ordered.keyValuesImpl.Add(destination, value)
			ordered.keyValuesImpl.Delete(source)
		}
		return
	}
	contents.Add(destination, value)
	contents.Delete(source)
}

func (c *orderedLogContents) releaseBodyOwnership() {
	if c.bodyOwned.Load() {
		c.bodyOwned.Store(false)
	}
}

func (c *orderedLogContents) Get(key string) any {
	if key == BodyKey {
		c.releaseBodyOwnership()
	}
	return c.keyValuesImpl.Get(key)
}

func (c *orderedLogContents) Iterator() map[string]any {
	c.releaseBodyOwnership()
	return c.keyValuesImpl.Iterator()
}

func (c *orderedLogContents) SortTo(buf []KeyValue[any]) []KeyValue[any] {
	c.releaseBodyOwnership()
	return c.keyValuesImpl.SortTo(buf)
}

func (c *orderedLogContents) Add(key string, value interface{}) {
	if key == BodyKey {
		c.releaseBodyOwnership()
	}
	if !c.Contains(key) {
		c.keys = append(c.keys, key)
	}
	c.keyValuesImpl.Add(key, value)
}

func (c *orderedLogContents) AddAll(items map[string]interface{}) {
	keys := make([]string, 0, len(items))
	for key := range items {
		keys = append(keys, key)
	}
	sort.Strings(keys)
	for _, key := range keys {
		c.Add(key, items[key])
	}
}

func (c *orderedLogContents) Merge(other KeyValues[interface{}]) {
	if ordered, ok := other.(*orderedLogContents); ok {
		for _, key := range ordered.keys {
			c.Add(key, ordered.Get(key))
		}
		return
	}
	c.AddAll(other.Iterator())
}

func (c *orderedLogContents) Delete(key string) {
	if key == BodyKey {
		c.releaseBodyOwnership()
	}
	if index := slices.Index(c.keys, key); index >= 0 {
		c.keys = slices.Delete(c.keys, index, index+1)
	}
	c.keyValuesImpl.Delete(key)
}

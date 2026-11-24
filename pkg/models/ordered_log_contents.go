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

const smallLogFieldCount = 4

type orderedLogMap struct {
	values map[string]any
	keys   []string
}

func (m *orderedLogMap) init(capacity int) {
	m.values = make(map[string]any, capacity)
	m.keys = make([]string, 0, capacity)
}

type orderedLogContents struct {
	packed    []KeyValue[any]
	extra     *KeyValue[any] // One more field after the full inline row.
	mapped    atomic.Pointer[orderedLogMap]
	bodyOwned atomic.Bool
}

type smallLogFields struct {
	contents orderedLogContents
	storage  [smallLogFieldCount]KeyValue[any]
}

func (f *smallLogFields) init() *orderedLogContents {
	f.contents.packed = f.storage[:0]
	return &f.contents
}

// NewOrderedLogContents preserves field order for inputs with an ordered row
// or a protobuf list of fields. Small rows keep fields together in one allocation.
func NewOrderedLogContents(capacity int) LogContents {
	if capacity <= smallLogFieldCount {
		return new(smallLogFields).init()
	}
	entry := new(struct {
		fields orderedLogContents
		mapped orderedLogMap
	})
	entry.mapped.init(capacity)
	entry.fields.mapped.Store(&entry.mapped)
	return &entry.fields
}

// NewLogWithOrderedContents builds a writable log. Flat input rows can
// allocate the log, its ordered fields, and its tag container together.
func NewLogWithOrderedContents(fieldCount int) *Log {
	if fieldCount > 0 && fieldCount <= smallLogFieldCount {
		entry := new(struct {
			log    Log
			fields smallLogFields
			tags   lazyLogTags
		})
		entry.log.Tags = &entry.tags
		entry.log.Contents = entry.fields.init()
		return &entry.log
	}
	if fieldCount == 0 {
		return &Log{Tags: NewTags()}
	}
	entry := new(struct {
		log    Log
		fields orderedLogContents
		mapped orderedLogMap
		tags   lazyLogTags
	})
	entry.mapped.init(fieldCount)
	entry.fields.mapped.Store(&entry.mapped)
	entry.log.Tags = &entry.tags
	entry.log.Contents = &entry.fields
	return &entry.log
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
		value, found = ordered.lookup(key)
		if key == BodyKey {
			owned = ordered.bodyOwned.Swap(false)
		}
		return
	}
	return contents.Get(key), contents.Contains(key), false
}

// ReserveLogContentKeys reserves fields that a processor is about to add.
// Modest growth can stay compact instead of creating a key slice and a map.
func ReserveLogContentKeys(contents LogContents, count int) {
	const maxReservedPackedFields = 8
	if ordered, ok := contents.(*orderedLogContents); ok {
		if mapped := ordered.mapped.Load(); mapped != nil {
			mapped.keys = slices.Grow(mapped.keys, count)
			return
		}
		length := ordered.packedLen()
		capacity := cap(ordered.packed)
		if capacity == smallLogFieldCount {
			// The next field can keep the original four inline slots.
			capacity++
		}
		if count > capacity-length && count <= maxReservedPackedFields-length {
			fields := make([]KeyValue[any], length, length+count)
			copy(fields, ordered.packed)
			if ordered.extra != nil {
				fields[length-1] = *ordered.extra
			}
			ordered.clearPacked()
			ordered.packed = fields
		}
	}
}

// FirstLogContentKey reports the first remaining field in ordered contents.
func FirstLogContentKey(contents LogContents) (string, bool) {
	if c, ok := contents.(*orderedLogContents); ok {
		if mapped := c.mapped.Load(); mapped != nil {
			if len(mapped.keys) > 0 {
				return mapped.keys[0], true
			}
		} else if len(c.packed) > 0 {
			return c.packed[0].Key, true
		}
	}
	return "", false
}

// LogContentEntries iterates fields in input order when the log preserves it.
func LogContentEntries(contents LogContents) iter.Seq2[string, any] {
	return func(yield func(string, any) bool) {
		if c, ok := contents.(*orderedLogContents); ok {
			c.entries(yield)
			return
		}
		for key, value := range contents.Iterator() {
			if !yield(key, value) {
				return
			}
		}
	}
}

func (c *orderedLogContents) entries(yield func(string, any) bool) {
	c.releaseBodyOwnership()
	if mapped := c.mapped.Load(); mapped != nil {
		for _, key := range mapped.keys {
			if !yield(key, mapped.values[key]) {
				return
			}
		}
		return
	}
	for _, field := range c.packed {
		if !yield(field.Key, field.Value) {
			return
		}
	}
	if field := c.extra; field != nil {
		yield(field.Key, field.Value)
	}
}

// RenameLogContent changes a field name while keeping its position in ordered contents.
func RenameLogContent(contents LogContents, source, destination string) {
	if source == destination || !contents.Contains(source) {
		return
	}
	value := contents.Get(source)
	if c, ok := contents.(*orderedLogContents); ok {
		c.Delete(destination)
		if mapped := c.mapped.Load(); mapped != nil {
			if index := slices.Index(mapped.keys, source); index >= 0 {
				mapped.keys[index] = destination
				mapped.values[destination] = value
				delete(mapped.values, source)
			}
		} else if index := c.packedIndex(source); index >= 0 {
			c.packedField(index).Key = destination
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

func (c *orderedLogContents) packedIndex(key string) int {
	for i := range c.packed {
		if c.packed[i].Key == key {
			return i
		}
	}
	if c.extra != nil && c.extra.Key == key {
		return len(c.packed)
	}
	return -1
}

func (c *orderedLogContents) packedLen() int {
	if c.extra != nil {
		return len(c.packed) + 1
	}
	return len(c.packed)
}

func (c *orderedLogContents) packedField(index int) *KeyValue[any] {
	if index == len(c.packed) {
		return c.extra
	}
	return &c.packed[index]
}

func (c *orderedLogContents) lookup(key string) (any, bool) {
	if mapped := c.mapped.Load(); mapped != nil {
		value, found := mapped.values[key]
		return value, found
	}
	if index := c.packedIndex(key); index >= 0 {
		return c.packedField(index).Value, true
	}
	return nil, false
}

func (c *orderedLogContents) Get(key string) any {
	if key == BodyKey {
		c.releaseBodyOwnership()
	}
	value, _ := c.lookup(key)
	return value
}

func (c *orderedLogContents) Contains(key string) bool {
	_, found := c.lookup(key)
	return found
}

func (c *orderedLogContents) Len() int {
	if mapped := c.mapped.Load(); mapped != nil {
		return len(mapped.values)
	}
	return c.packedLen()
}

func (*orderedLogContents) IsNil() bool { return false }

// materializeMap also serves Iterator, whose callers may retain and modify
// the returned map. Concurrent readers must see the same first map.
func (c *orderedLogContents) materializeMap(capacity int) *orderedLogMap {
	if mapped := c.mapped.Load(); mapped != nil {
		return mapped
	}
	mapped := new(orderedLogMap)
	mapped.init(capacity)
	for _, field := range c.packed {
		mapped.keys = append(mapped.keys, field.Key)
		mapped.values[field.Key] = field.Value
	}
	if field := c.extra; field != nil {
		mapped.keys = append(mapped.keys, field.Key)
		mapped.values[field.Key] = field.Value
	}
	if c.mapped.CompareAndSwap(nil, mapped) {
		return mapped
	}
	return c.mapped.Load()
}

func (c *orderedLogContents) Iterator() map[string]any {
	c.releaseBodyOwnership()
	return c.materializeMap(c.packedLen()).values
}

func (c *orderedLogContents) SortTo(buf []KeyValue[any]) []KeyValue[any] {
	c.releaseBodyOwnership()
	if buf == nil {
		buf = make([]KeyValue[any], 0, c.Len())
	} else {
		buf = buf[:0]
	}
	if mapped := c.mapped.Load(); mapped != nil {
		buf = slices.Grow(buf, len(mapped.values))
		for key, value := range mapped.values {
			buf = append(buf, KeyValue[any]{Key: key, Value: value})
		}
	} else {
		buf = append(buf, c.packed...)
		if c.extra != nil {
			buf = append(buf, *c.extra)
		}
	}
	sort.Sort(KeyValueSlice[any](buf))
	return buf
}

func (c *orderedLogContents) Add(key string, value any) {
	if key == BodyKey {
		c.releaseBodyOwnership()
	}
	mapped := c.mapped.Load()
	if mapped == nil {
		if index := c.packedIndex(key); index >= 0 {
			c.packedField(index).Value = value
			return
		}
		if len(c.packed) < cap(c.packed) {
			c.packed = append(c.packed, KeyValue[any]{Key: key, Value: value})
			return
		}
		if len(c.packed) == smallLogFieldCount && c.extra == nil {
			c.extra = &KeyValue[any]{Key: key, Value: value}
			return
		}
		mapped = c.materializeMap(c.packedLen() + 1)
	}
	c.clearPacked()
	if _, found := mapped.values[key]; !found {
		mapped.keys = append(mapped.keys, key)
	}
	mapped.values[key] = value
}

func (c *orderedLogContents) clearPacked() {
	// Iterator may have published the map during a read. Clear the old
	// references on the next mutation, when there are no concurrent readers.
	clear(c.packed)
	c.packed = nil
	c.extra = nil
}

func (c *orderedLogContents) AddAll(items map[string]any) {
	keys := make([]string, 0, len(items))
	for key := range items {
		keys = append(keys, key)
	}
	sort.Strings(keys)
	for _, key := range keys {
		c.Add(key, items[key])
	}
}

func (c *orderedLogContents) Merge(other KeyValues[any]) {
	if ordered, ok := other.(*orderedLogContents); ok {
		ordered.entries(func(key string, value any) bool {
			c.Add(key, value)
			return true
		})
		return
	}
	c.AddAll(other.Iterator())
}

func (c *orderedLogContents) Delete(key string) {
	if key == BodyKey {
		c.releaseBodyOwnership()
	}
	if mapped := c.mapped.Load(); mapped != nil {
		c.clearPacked()
		if index := slices.Index(mapped.keys, key); index >= 0 {
			mapped.keys = slices.Delete(mapped.keys, index, index+1)
		}
		delete(mapped.values, key)
	} else if index := c.packedIndex(key); index >= 0 {
		if index < len(c.packed) {
			c.packed = slices.Delete(c.packed, index, index+1)
			if c.extra != nil {
				c.packed = append(c.packed, *c.extra)
			}
		}
		c.extra = nil
	}
}

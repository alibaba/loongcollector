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

import (
	"maps"
	"sync/atomic"
)

// GroupInfoTemplate holds defaults for groups from the same source. Each group
// keeps its own writable tags and metadata while sharing the initial values.
type GroupInfoTemplate struct {
	metadata groupValueDefaults
	tags     groupValueDefaults
}

// NewGroupInfoTemplate snapshots the supplied default metadata and tags.
func NewGroupInfoTemplate(metadata, tags map[string]string) *GroupInfoTemplate {
	return &GroupInfoTemplate{
		metadata: groupValueDefaults{values: maps.Clone(metadata)},
		tags:     groupValueDefaults{values: maps.Clone(tags), canEnrich: true},
	}
}

// NewSingleEventGroup keeps the group, its values, and the one-event slice
// together. The caller still sends one independent group per event.
func (t *GroupInfoTemplate) NewSingleEventGroup(event PipelineEvent) *PipelineGroupEvents {
	one := new(struct {
		group    PipelineGroupEvents
		info     GroupInfo
		metadata groupDefaultValues
		tags     groupDefaultValues
		events   [1]PipelineEvent
	})
	one.metadata.defaults = &t.metadata
	one.tags.defaults = &t.tags
	one.info.Metadata = &one.metadata
	one.info.Tags = &one.tags
	one.events[0] = event
	one.group.Group = &one.info
	one.group.Events = one.events[:]
	return &one.group
}

type groupValueDefaults struct {
	values    map[string]string
	enriched  atomic.Pointer[groupTagEnrichment]
	canEnrich bool
}

type groupTagEnrichment struct {
	additions map[string]string
	merged    groupValueDefaults
}

// groupDefaultValues clones the defaults before mutation or providing the
// writable Iterator map. Reads can use the source template directly.
type groupDefaultValues struct {
	defaults *groupValueDefaults
	owned    atomic.Pointer[keyValuesImpl[string]]
}

func (v *groupDefaultValues) read() map[string]string {
	if owned := v.owned.Load(); owned != nil {
		return owned.keyValues
	}
	return v.defaults.values
}

func (v *groupDefaultValues) mutable() *keyValuesImpl[string] {
	if owned := v.owned.Load(); owned != nil {
		return owned
	}
	var values map[string]string
	if len(v.defaults.values) == 0 {
		values = make(map[string]string)
	} else {
		values = maps.Clone(v.defaults.values)
	}
	owned := &keyValuesImpl[string]{keyValues: values}
	if v.owned.CompareAndSwap(nil, owned) {
		return owned
	}
	return v.owned.Load()
}

func (v *groupDefaultValues) Add(key, value string) { v.mutable().Add(key, value) }

func (v *groupDefaultValues) AddAll(values map[string]string) {
	if len(values) != 0 {
		v.mutable().AddAll(values)
	}
}

func (v *groupDefaultValues) Get(key string) string { return v.read()[key] }

func (v *groupDefaultValues) Contains(key string) bool {
	_, ok := v.read()[key]
	return ok
}

func (v *groupDefaultValues) Delete(key string) {
	if v.Contains(key) {
		v.mutable().Delete(key)
	}
}

func (v *groupDefaultValues) Merge(other KeyValues[string]) {
	if other.Len() != 0 {
		v.AddAll(readStringValues(other))
	}
}

func (v *groupDefaultValues) Iterator() map[string]string { return v.mutable().keyValues }

func (v *groupDefaultValues) SortTo(buf []KeyValue[string]) []KeyValue[string] {
	view := keyValuesImpl[string]{keyValues: v.read()}
	if view.keyValues == nil && buf == nil {
		return []KeyValue[string]{}
	}
	return view.SortTo(buf)
}

func (v *groupDefaultValues) Len() int { return len(v.read()) }

func (*groupDefaultValues) IsNil() bool { return false }

// TryApplyGroupTagDefaults applies the runner's tags to a group whose original
// container defaults have not been exposed for writing. Later writes still
// obtain a private map. Other tag containers should receive the ordinary adds.
func TryApplyGroupTagDefaults(tags Tags, additions map[string]string) bool {
	if len(additions) == 0 {
		return true
	}
	values, ok := tags.(*groupDefaultValues)
	if !ok || values.owned.Load() != nil || !values.defaults.canEnrich {
		return false
	}
	base := values.defaults
	cached := base.enriched.Load()
	if cached == nil || !maps.Equal(cached.additions, additions) {
		// Copy the entries into storage owned by the cache. The caller's
		// temporary map is only read and can remain on the stack.
		snapshot := make(map[string]string, len(additions))
		merged := make(map[string]string, len(base.values)+len(additions))
		maps.Copy(merged, base.values)
		for key, value := range additions {
			snapshot[key] = value
			merged[key] = value
		}
		cached = &groupTagEnrichment{
			additions: snapshot,
			merged:    groupValueDefaults{values: merged},
		}
		// Keep only the latest result for this container template. Previously
		// emitted groups still refer to their own immutable snapshot.
		base.enriched.Store(cached)
	}
	values.defaults = &cached.merged
	return true
}

// EqualStringValues compares group fields without turning shared defaults into
// per-group maps. Iterator still supplies a writable map to plugins.
func EqualStringValues(a, b KeyValues[string]) bool {
	if first, ok := a.(*groupDefaultValues); ok {
		if second, ok := b.(*groupDefaultValues); ok && first.defaults == second.defaults &&
			first.owned.Load() == nil && second.owned.Load() == nil {
			return true
		}
	}
	return a.Len() == b.Len() && maps.Equal(readStringValues(a), readStringValues(b))
}

func readStringValues(values KeyValues[string]) map[string]string {
	if defaults, ok := values.(*groupDefaultValues); ok {
		return defaults.read()
	}
	return values.Iterator()
}

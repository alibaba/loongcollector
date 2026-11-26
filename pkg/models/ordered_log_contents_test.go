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
	"testing"

	"github.com/stretchr/testify/require"
)

func TestOrderedLogContentsWithCloneRenameAndDelete(t *testing.T) {
	log := &Log{Contents: NewOrderedLogContents(3), Tags: NewTagsWithKeyValues("tag", "source"), RawSize: 40}
	log.Contents.Add("z", "first")
	log.Contents.Add("b", "second")
	log.Contents.Add("a", "third")
	log.Contents.Add("z", "updated")
	log.Contents.AddAll(map[string]any{"a": "updated third"})

	clone := log.Clone().(*Log)
	require.Equal(t, uint64(40), clone.RawSize)
	RenameLogContent(clone.Contents, "z", "b")
	key, ok := FirstLogContentKey(clone.Contents)
	require.True(t, ok)
	require.Equal(t, "b", key)
	require.Equal(t, "updated", clone.Contents.Get(key))
	require.Len(t, clone.Contents.Iterator(), 2)
	clone.Tags.Add("tag", "clone")
	require.Equal(t, "source", log.Tags.Get("tag"))
	clone.Contents.Delete("b")
	key, ok = FirstLogContentKey(clone.Contents)
	require.True(t, ok)
	require.Equal(t, "a", key)

	key, ok = FirstLogContentKey(log.Contents)
	require.True(t, ok)
	require.Equal(t, "z", key)
	require.Equal(t, "second", log.Contents.Get("b"))
}

func TestLogCloneKeepsIndependentBodyFieldsAndTags(t *testing.T) {
	log := NewSimpleLog([]byte("original"), NewTagsWithKeyValues("log", "a"), 123)
	clone := log.Clone().(*Log)
	clone.GetBody()[0] = 'X'
	clone.Contents.Add("added", "new field")
	clone.Tags.Add("log", "b")
	require.Equal(t, "original", string(log.GetBody()))
	require.False(t, log.Contents.Contains("added"))
	require.Equal(t, "a", log.GetTags().Get("log"))
}

func TestLogSizeKeepsBodyOwnership(t *testing.T) {
	log := &Log{Contents: NewOrderedLogContents(1)}
	SetOwnedLogBody(log, []byte("test"))
	require.EqualValues(t, len(BodyKey)+len("test"), log.GetSize())
	_, found, reusable := LookupLogContentForReplacement(log.GetIndices(), BodyKey)
	require.True(t, found)
	require.True(t, reusable)
}

func TestGroupSizeTracksCurrentLogFieldsAndTags(t *testing.T) {
	for name, contents := range map[string]LogContents{
		"compact": NewOrderedLogContents(4),
		"mapped":  NewOrderedLogContents(8),
		"map":     NewLogContents(),
	} {
		t.Run(name, func(t *testing.T) {
			log := &Log{Contents: contents, Tags: NewTagsWithKeyValues("source", "x")}
			log.SetBody([]byte("raw"))
			contents.Delete(BodyKey)
			contents.Add("message", "你好")
			contents.Add("status", 200)
			contents.Add("binary", []byte{0, 1})
			contents.Add("remove", "a")
			contents.Add("extra", "b")
			group := &PipelineGroupEvents{
				Group:  NewGroup(NewMetadata(), NewTagsWithKeyValues("env", "prod")),
				Events: []PipelineEvent{log, log},
			}
			require.EqualValues(t, 7+2*(7+13+9+8+7+6), group.GetSize())

			fields := contents.Iterator()
			delete(fields, "remove")
			fields["extra"] = "updated"
			fields["new"] = "row"
			require.EqualValues(t, 7+2*(7+13+9+8+12+6), group.GetSize())
		})
	}
}

func TestGroupSizeKeepsSharedTagDefaults(t *testing.T) {
	template := NewGroupInfoTemplate(nil, map[string]string{"env": "prod"})
	log := NewLogWithOrderedContents(1)
	SetOwnedLogBody(log, []byte("hello"))
	group := template.NewSingleEventGroup(log)
	require.EqualValues(t, 19, group.GetSize())
	require.Nil(t, group.Group.Tags.(*groupDefaultValues).owned.Load())

	require.True(t, TryApplyGroupTagDefaults(group.Group.Tags, map[string]string{"host": "node"}))
	require.EqualValues(t, 27, group.GetSize())
	require.Nil(t, group.Group.Tags.(*groupDefaultValues).owned.Load())

	group.Group.Tags.Iterator()["env"] = "development"
	require.EqualValues(t, 34, group.GetSize())
}

func TestCompactLogFieldsPreserveOrderAfterMaterializing(t *testing.T) {
	log := NewLogWithOrderedContents(1)
	for _, key := range []string{"z", "b", "c", "a"} {
		log.Contents.Add(key, key)
	}
	// Growing beyond the embedded capacity preserves field order and allows
	// Iterator callers to update the live values.
	log.Contents.Add("e", "e")
	fields := log.Contents.Iterator()
	fields["c"] = "updated"
	log.Contents.Delete("b")
	RenameLogContent(log.Contents, "z", "e")
	var keys []string
	for key := range LogContentEntries(log.Contents) {
		keys = append(keys, key)
	}
	require.Equal(t, []string{"e", "c", "a"}, keys)
	require.Equal(t, "updated", log.Contents.Get("c"))
	require.Equal(t, "z", fields["e"])
	require.False(t, log.Contents.Contains("b"))
}

func TestCompactLogContentsPublishesOneLiveMap(t *testing.T) {
	fields := NewOrderedLogContents(1)
	fields.Add("key", "old")
	results := make(chan map[string]any, 2)
	for i := 0; i < 2; i++ {
		go func() { results <- fields.Iterator() }()
	}
	first, second := <-results, <-results
	first["key"] = "new"
	require.Equal(t, "new", second["key"])
	require.Equal(t, "new", fields.Get("key"))
}

func TestNativeLogTagsKeepWritableMap(t *testing.T) {
	log := NewLogWithOrderedContents(1)
	tags := log.GetTags().(*lazyLogTags)
	require.Zero(t, tags.Len())
	require.Empty(t, tags.Get("key"))
	require.Empty(t, tags.SortTo(nil))
	require.Nil(t, tags.values.Load())

	fields := tags.Iterator()
	fields["key"] = "value"
	require.Equal(t, "value", tags.Get("key"))
	tags.Add("key", "updated")
	require.Equal(t, "updated", fields["key"])
}

func TestReserveCompactLogContentsPreservesFieldOrder(t *testing.T) {
	fields := NewOrderedLogContents(3).(*orderedLogContents)
	fields.Add("content", "message")
	fields.Add("_time_", "time")
	fields.Add("_source_", "stdout")
	fields.Delete("content")
	ReserveLogContentKeys(fields, 3)
	for _, key := range []string{"id", "level", "message"} {
		fields.Add(key, key)
	}
	require.Nil(t, fields.mapped.Load())
	var keys []string
	for key := range LogContentEntries(fields) {
		keys = append(keys, key)
	}
	require.Equal(t, []string{"_time_", "_source_", "id", "level", "message"}, keys)
	fields.Iterator()["message"] = "updated"
	require.Equal(t, "updated", fields.Get("message"))
}

func TestOrderedLogContentsEditsTheExtraField(t *testing.T) {
	fields := NewOrderedLogContents(4)
	for _, key := range []string{"a", "b", "c", "d", "e"} {
		fields.Add(key, key)
	}
	RenameLogContent(fields, "e", "renamed")
	fields.Add("renamed", "updated")
	fields.Delete("b")
	fields.Add("last", "last")
	var keys []string
	for key := range LogContentEntries(fields) {
		keys = append(keys, key)
	}
	require.Equal(t, []string{"a", "c", "d", "renamed", "last"}, keys)
	require.Equal(t, "updated", fields.Get("renamed"))
	require.Equal(t, "last", fields.SortTo(nil)[3].Key)

	ReserveLogContentKeys(fields, 2)
	fields.Add("f", "f")
	fields.Add("g", "g")
	require.Len(t, fields.Iterator(), 7)
	require.Equal(t, "updated", fields.Get("renamed"))
	require.Equal(t, "last", fields.Get("last"))
}

func TestOrderedLogContentsTracksLiveMapChanges(t *testing.T) {
	fields := NewOrderedLogContents(2)
	fields.Add("first", "old")
	fields.Add("second", "kept")

	live := fields.Iterator()
	delete(live, "first")
	live["z"] = "renamed"
	live["a"] = "added"

	key, ok := FirstLogContentKey(fields)
	require.True(t, ok)
	require.Equal(t, "second", key)
	var keys []string
	for key := range LogContentEntries(fields) {
		keys = append(keys, key)
	}
	require.Equal(t, []string{"second", "a", "z"}, keys)

	RenameLogContent(fields, "z", "renamed")
	require.Equal(t, "renamed", live["renamed"])
	require.NotContains(t, live, "z")

	fields.Add("first", "readded")
	keys = keys[:0]
	for key := range LogContentEntries(fields) {
		keys = append(keys, key)
	}
	require.Equal(t, []string{"second", "a", "renamed", "first"}, keys)
}

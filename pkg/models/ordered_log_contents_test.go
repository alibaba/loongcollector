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
	require.EqualValues(t, 4, log.GetSize())
	_, found, reusable := LookupLogContentForReplacement(log.GetIndices(), BodyKey)
	require.True(t, found)
	require.True(t, reusable)
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

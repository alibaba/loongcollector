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

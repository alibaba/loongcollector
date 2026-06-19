// Copyright 2024 iLogtail Authors
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

package checkpoint

import (
	"sync"
	"testing"
	"time"

	"github.com/stretchr/testify/require"
)

type commitRecord struct {
	sourceID string
	offset   int64
}

func newRecordingTracker() (*Tracker, func() []commitRecord) {
	var mu sync.Mutex
	var records []commitRecord
	t := NewTracker(func(configName, sourceID, logPath string, offset int64) {
		mu.Lock()
		defer mu.Unlock()
		records = append(records, commitRecord{sourceID: sourceID, offset: offset})
	})
	return t, func() []commitRecord {
		mu.Lock()
		defer mu.Unlock()
		out := make([]commitRecord, len(records))
		copy(out, records)
		return out
	}
}

func lastOffset(records []commitRecord, sourceID string) (int64, bool) {
	last := int64(0)
	found := false
	for _, r := range records {
		if r.sourceID == sourceID {
			last = r.offset
			found = true
		}
	}
	return last, found
}

func TestTrackerOutOfOrderDoesNotSkipGap(t *testing.T) {
	tr, snap := newRecordingTracker()
	g1 := tr.AddGroup("s1", "cfg", "/a.log", 100, 1)
	g2 := tr.AddGroup("s1", "cfg", "/a.log", 200, 1)
	g3 := tr.AddGroup("s1", "cfg", "/a.log", 300, 1)

	tr.Ack("s1", g3)
	tr.Ack("s1", g2)
	_, ok := lastOffset(snap(), "s1")
	require.False(t, ok)

	tr.Ack("s1", g1)
	off, ok := lastOffset(snap(), "s1")
	require.True(t, ok)
	require.Equal(t, int64(300), off)
}

func TestTrackerMultiMessageGroup(t *testing.T) {
	tr, snap := newRecordingTracker()
	g := tr.AddGroup("s1", "cfg", "/a.log", 100, 3)

	tr.Ack("s1", g)
	tr.Ack("s1", g)
	_, ok := lastOffset(snap(), "s1")
	require.False(t, ok)

	tr.Ack("s1", g)
	off, ok := lastOffset(snap(), "s1")
	require.True(t, ok)
	require.Equal(t, int64(100), off)
}

func TestTrackerFailureBlocksCheckpoint(t *testing.T) {
	tr, snap := newRecordingTracker()
	g1 := tr.AddGroup("s1", "cfg", "/a.log", 100, 1)
	g2 := tr.AddGroup("s1", "cfg", "/a.log", 200, 1)

	tr.Fail("s1", g1)
	tr.Ack("s1", g2)
	_, ok := lastOffset(snap(), "s1")
	require.False(t, ok)
}

func TestTrackerCommitOutsideLocks(t *testing.T) {
	done := make(chan struct{})
	var tr *Tracker
	tr = NewTracker(func(configName, sourceID, logPath string, offset int64) {
		require.NotNil(t, tr.AddGroup("s2", "cfg", "/b.log", 200, 1))
		close(done)
	})
	g := tr.AddGroup("s1", "cfg", "/a.log", 100, 1)
	require.NotNil(t, g)

	tr.Ack("s1", g)

	select {
	case <-done:
	case <-time.After(time.Second):
		t.Fatal("commit callback appears to run while holding tracker locks")
	}
}

func TestCommitterCoalescesBySource(t *testing.T) {
	var mu sync.Mutex
	var records []commitRecord
	c := NewCommitter(func(configName, sourceID, logPath string, offset int64) {
		mu.Lock()
		defer mu.Unlock()
		records = append(records, commitRecord{sourceID: sourceID, offset: offset})
	})

	c.Add("cfg", "s1", "/a.log", 100)
	c.Add("cfg", "s1", "/a.log", 200)
	c.Add("cfg", "s2", "/b.log", 50)
	c.Stop()

	mu.Lock()
	defer mu.Unlock()
	require.Len(t, records, 2)
	off1, ok1 := lastOffset(records, "s1")
	off2, ok2 := lastOffset(records, "s2")
	require.True(t, ok1)
	require.True(t, ok2)
	require.Equal(t, int64(200), off1)
	require.Equal(t, int64(50), off2)
}

func TestTrackerRejectsNewGroupsAfterHeadFailure(t *testing.T) {
	tr, _ := newRecordingTracker()
	g1 := tr.AddGroup("s1", "cfg", "/a.log", 100, 1)
	require.NotNil(t, g1)
	tr.Fail("s1", g1)

	require.Nil(t, tr.AddGroup("s1", "cfg", "/a.log", 200, 1))
}

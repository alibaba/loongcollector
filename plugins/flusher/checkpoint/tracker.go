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
	"time"

	"github.com/alibaba/ilogtail/pkg/models"
)

const (
	MetaKeyLogFilePath = "1"
	MetaKeySourceID    = "25"

	maxTrackedSources          = 4096
	maxPendingGroupsPerSource  = 1 << 16
	compactGroupsThreshold     = 1024
	commitFlushInterval        = 200 * time.Millisecond
	maxPendingCheckpointCommit = 1024
)

type Group struct {
	endOffset int64
	remaining int
	delivered bool
	failed    bool
}

type sourceTracker struct {
	mu         sync.Mutex
	configName string
	logPath    string
	groups     []*Group
	committed  int64
}

type offsetCommit struct {
	configName string
	logPath    string
	offset     int64
	ok         bool
}

type CommitFunc func(configName, sourceID, logPath string, offset int64)

type Tracker struct {
	mu      sync.Mutex
	sources map[string]*sourceTracker
	commit  CommitFunc
}

type Committer struct {
	commit CommitFunc

	mu      sync.Mutex
	pending map[string]offsetCommit
	stopped bool

	flushCh chan struct{}
	stopCh  chan struct{}
	doneCh  chan struct{}
	once    sync.Once
}

func NewCommitter(commit CommitFunc) *Committer {
	c := &Committer{
		commit:  commit,
		pending: make(map[string]offsetCommit),
		flushCh: make(chan struct{}, 1),
		stopCh:  make(chan struct{}),
		doneCh:  make(chan struct{}),
	}
	go c.run()
	return c
}

func (c *Committer) Add(configName, sourceID, logPath string, offset int64) {
	if sourceID == "" || c == nil || c.commit == nil {
		return
	}

	c.mu.Lock()
	if c.stopped {
		c.mu.Unlock()
		c.commit(configName, sourceID, logPath, offset)
		return
	}
	if existing, ok := c.pending[sourceID]; !ok || existing.offset < offset {
		c.pending[sourceID] = offsetCommit{configName: configName, logPath: logPath, offset: offset, ok: true}
	}
	shouldFlush := len(c.pending) >= maxPendingCheckpointCommit
	c.mu.Unlock()

	if shouldFlush {
		select {
		case c.flushCh <- struct{}{}:
		default:
		}
	}
}

func (c *Committer) Stop() {
	if c == nil {
		return
	}
	c.once.Do(func() {
		c.mu.Lock()
		c.stopped = true
		c.mu.Unlock()
		close(c.stopCh)
		<-c.doneCh
	})
}

func (c *Committer) run() {
	ticker := time.NewTicker(commitFlushInterval)
	defer ticker.Stop()
	defer close(c.doneCh)
	for {
		select {
		case <-ticker.C:
			c.flush()
		case <-c.flushCh:
			c.flush()
		case <-c.stopCh:
			c.flush()
			return
		}
	}
}

func (c *Committer) flush() {
	c.mu.Lock()
	if len(c.pending) == 0 {
		c.mu.Unlock()
		return
	}
	commits := make(map[string]offsetCommit, len(c.pending))
	for sourceID, commit := range c.pending {
		commits[sourceID] = commit
	}
	clear(c.pending)
	c.mu.Unlock()

	for sourceID, commit := range commits {
		c.commit(commit.configName, sourceID, commit.logPath, commit.offset)
	}
}

func NewTracker(commit CommitFunc) *Tracker {
	return &Tracker{
		sources: make(map[string]*sourceTracker),
		commit:  commit,
	}
}

func BuildGroup(tracker *Tracker, sourceID, configName, logPath string, groupEvents *models.PipelineGroupEvents, msgCount int) *Group {
	if tracker == nil || sourceID == "" || msgCount <= 0 || groupEvents == nil {
		return nil
	}
	endOffset := int64(-1)
	for _, event := range groupEvents.Events {
		logEvent, ok := event.(*models.Log)
		if !ok {
			continue
		}
		end := int64(logEvent.GetOffset() + logEvent.GetRawSize())
		if end > endOffset {
			endOffset = end
		}
	}
	if endOffset < 0 {
		return nil
	}
	return tracker.AddGroup(sourceID, configName, logPath, endOffset, msgCount)
}

func (t *Tracker) AddGroup(sourceID, configName, logPath string, endOffset int64, msgCount int) *Group {
	if msgCount <= 0 {
		return nil
	}
	t.mu.Lock()
	st := t.sources[sourceID]
	if st == nil {
		if len(t.sources) >= maxTrackedSources {
			t.mu.Unlock()
			return nil
		}
		st = &sourceTracker{configName: configName, logPath: logPath}
		t.sources[sourceID] = st
	}
	t.mu.Unlock()

	st.mu.Lock()
	defer st.mu.Unlock()
	if len(st.groups) > 0 && st.groups[0].failed {
		return nil
	}
	if len(st.groups) >= maxPendingGroupsPerSource {
		return nil
	}
	g := &Group{endOffset: endOffset, remaining: msgCount}
	st.groups = append(st.groups, g)
	return g
}

func (t *Tracker) Ack(sourceID string, g *Group) {
	if g == nil {
		return
	}
	st := t.getSource(sourceID)
	if st == nil {
		return
	}
	var commit offsetCommit
	shouldReclaim := false
	st.mu.Lock()
	if g.remaining > 0 {
		g.remaining--
	}
	if g.remaining == 0 && !g.failed {
		g.delivered = true
		commit, shouldReclaim = st.advanceLocked()
	}
	st.mu.Unlock()
	if shouldReclaim {
		t.reclaimSource(sourceID, st)
	}
	if commit.ok && t.commit != nil {
		t.commit(commit.configName, sourceID, commit.logPath, commit.offset)
	}
}

func (t *Tracker) Fail(sourceID string, g *Group) {
	if g == nil {
		return
	}
	st := t.getSource(sourceID)
	if st == nil {
		return
	}
	st.mu.Lock()
	defer st.mu.Unlock()
	if g.remaining > 0 {
		g.remaining--
	}
	g.failed = true
}

func (t *Tracker) getSource(sourceID string) *sourceTracker {
	t.mu.Lock()
	defer t.mu.Unlock()
	return t.sources[sourceID]
}

func (t *Tracker) reclaimSource(sourceID string, st *sourceTracker) {
	t.mu.Lock()
	defer t.mu.Unlock()
	if t.sources[sourceID] != st {
		return
	}
	st.mu.Lock()
	defer st.mu.Unlock()
	if len(st.groups) == 0 {
		delete(t.sources, sourceID)
	}
}

func (st *sourceTracker) advanceLocked() (offsetCommit, bool) {
	progressed := false
	i := 0
	for i < len(st.groups) && st.groups[i].delivered {
		if st.groups[i].endOffset > st.committed {
			st.committed = st.groups[i].endOffset
			progressed = true
		}
		i++
	}
	if i > 0 {
		st.groups = st.groups[i:]
		if i >= compactGroupsThreshold {
			groups := make([]*Group, len(st.groups))
			copy(groups, st.groups)
			st.groups = groups
		}
	}
	shouldReclaim := len(st.groups) == 0
	return offsetCommit{configName: st.configName, logPath: st.logPath, offset: st.committed, ok: progressed}, shouldReclaim
}

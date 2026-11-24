// Copyright 2021 iLogtail Authors
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

package baseagg

import (
	"fmt"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
)

const (
	maxPendingBytes     = 10 * 1024 * 1024
	packCleanupInterval = 10 * time.Minute
)

// Batcher accumulates events by metadata and tags. Only events with the same
// attributes can share an output group. Its limits bound retained events
// even when a routing key has a high cardinality.
type Batcher struct {
	mu           sync.Mutex
	pending      map[string][]*eventBatch
	pendingBytes int64
	maxEvents    int
	maxGroups    int
	packPrefix   string
	packSequence uint64

	sourcePacks     map[string]sourcePack
	packTolerance   int
	lastPackCleanup time.Time
}

type sourcePack struct {
	sequence uint64
	lastUsed time.Time
}

type eventBatch struct {
	group models.PipelineGroupEvents
	size  int64
}

func (b *Batcher) Init(ctx pipeline.Context, maxEvents, maxGroups int, packID bool) error {
	if maxEvents <= 0 || maxGroups <= 0 {
		return fmt.Errorf("MaxLogCount and MaxLogGroupCount must be positive")
	}
	b.pending = make(map[string][]*eventBatch)
	b.maxEvents = maxEvents
	b.maxGroups = maxGroups
	if packID {
		b.packPrefix = util.NewPackIDPrefix(ctx.GetConfigName())
	}
	return nil
}

// UseSourcePackIDs keeps the supplied source prefixes and their independent
// sequences for context aggregation.
func (b *Batcher) UseSourcePackIDs(tolerance int) {
	if b.packPrefix != "" {
		b.sourcePacks = make(map[string]sourcePack)
		b.packTolerance = tolerance
		b.lastPackCleanup = time.Now()
	}
}

func (b *Batcher) Collect(ctx pipeline.PipelineContext, info *models.GroupInfo, events []models.PipelineEvent) {
	b.CollectPartition(ctx, "", info, events)
}

// CollectPartition applies batch capacity independently for each routing key.
func (b *Batcher) CollectPartition(ctx pipeline.PipelineContext, key string, info *models.GroupInfo, events []models.PipelineEvent) {
	if len(events) == 0 {
		return
	}
	b.mu.Lock()
	defer b.mu.Unlock()

	collector := ctx.Collector()
	var batch *eventBatch
	pending := b.pending[key]
	if len(pending) > 0 {
		if last := pending[len(pending)-1]; sameGroup(last.group.Group, info) {
			batch = last
		}
	}
	for index, event := range events {
		size := eventSize(event)
		if b.pendingBytes > 0 && b.pendingBytes+size > maxPendingBytes {
			b.collectPending(collector)
			batch = nil
			pending = nil
		}
		if batch == nil || len(batch.group.Events) >= b.maxEvents || batch.size+size > MaxLogGroupSize {
			// Reserve space for incoming events. For small deliveries, reuse
			// the count already reached by the preceding batch for this group.
			capacity := len(events) - index
			if batch != nil {
				capacity = max(capacity, len(batch.group.Events))
			}
			capacity = min(capacity, b.maxEvents, MaxLogCount, max(1, int(MaxLogGroupSize/size)))
			if len(pending) >= b.maxGroups {
				b.emit(collector, key, &pending[0].group)
				b.pendingBytes -= pending[0].size
				copy(pending, pending[1:])
				pending[len(pending)-1] = nil
				pending = pending[:len(pending)-1]
			}
			batch = &eventBatch{group: models.PipelineGroupEvents{
				Group:  info,
				Events: make([]models.PipelineEvent, 0, capacity),
			}}
			pending = append(pending, batch)
			b.pending[key] = pending
		}
		batch.group.Events = append(batch.group.Events, event)
		batch.size += size
		b.pendingBytes += size
	}
}

func (b *Batcher) CollectPending(ctx pipeline.PipelineContext) {
	b.mu.Lock()
	defer b.mu.Unlock()
	b.collectPending(ctx.Collector())
}

func (b *Batcher) collectPending(collector pipeline.PipelineCollector) {
	for key, pending := range b.pending {
		for _, batch := range pending {
			b.emit(collector, key, &batch.group)
		}
		delete(b.pending, key)
	}
	b.pendingBytes = 0
	now := time.Now()
	if len(b.sourcePacks) > b.packTolerance && now.Sub(b.lastPackCleanup) >= packCleanupInterval {
		expiry := 30 * 24 * time.Hour
		if len(b.sourcePacks) > 100000 {
			expiry = 24 * time.Hour
		}
		for source, pack := range b.sourcePacks {
			if now.Sub(pack.lastUsed) > expiry {
				delete(b.sourcePacks, source)
			}
		}
		b.lastPackCleanup = now
	}
}

func (b *Batcher) Reset() {
	b.mu.Lock()
	defer b.mu.Unlock()
	clear(b.pending)
	b.pendingBytes = 0
}

func (b *Batcher) emit(collector pipeline.PipelineCollector, key string, group *models.PipelineGroupEvents) {
	if b.packPrefix != "" && !group.Group.GetTags().Contains(util.PackIDTagKey) {
		tags := models.NewTags()
		tags.Merge(group.Group.GetTags())
		tags.Add(util.PackIDTagKey, b.nextPackID(key))
		group.Group = &models.GroupInfo{
			Metadata:            group.Group.GetMetadata(),
			Tags:                tags,
			DestinationLogstore: group.Group.GetDestinationLogstore(),
		}
	}
	collector.CollectList(group)
}

func (b *Batcher) nextPackID(key string) string {
	prefix := b.packPrefix
	sequence := b.packSequence
	if b.sourcePacks != nil {
		if key != "" {
			prefix = key
			if !strings.HasSuffix(prefix, "-") {
				prefix += "-"
			}
		}
		pack := b.sourcePacks[key]
		pack.sequence++
		pack.lastUsed = time.Now()
		b.sourcePacks[key] = pack
		sequence = pack.sequence
	} else {
		b.packSequence++
	}
	return prefix + strings.ToUpper(strconv.FormatUint(sequence, 16))
}

func eventSize(event models.PipelineEvent) int64 {
	log, ok := event.(*models.Log)
	if !ok {
		return max(event.GetSize(), 1)
	}
	// Log.GetSize counts only the raw body. Include structured fields when
	// enforcing the batch limit.
	size := int64(6)
	for key, value := range models.LogContentEntries(log.GetIndices()) {
		size += int64(5 + len(key))
		switch value := value.(type) {
		case string:
			size += int64(len(value))
		case []byte:
			size += int64(len(value))
		default:
			size += int64(len(pipeline.GetStringValue(value)))
		}
	}
	return size
}

func sameGroup(a, b *models.GroupInfo) bool {
	return a == b || (a.GetDestinationLogstore() == b.GetDestinationLogstore() &&
		models.EqualStringValues(a.GetMetadata(), b.GetMetadata()) &&
		models.EqualStringValues(a.GetTags(), b.GetTags()))
}

// GroupForRoute adds routing metadata and group tags without changing the
// source group, which may also be consumed by other aggregators.
func GroupForRoute(info *models.GroupInfo, logstore, topic string, extraTags map[string]string) *models.GroupInfo {
	if logstore == "" && topic == "" && len(extraTags) == 0 {
		return info
	}
	if logstore == "" {
		logstore = info.GetDestinationLogstore()
	}
	tags := info.GetTags()
	if topic != "" || len(extraTags) != 0 {
		tags = models.NewTags()
		tags.Merge(info.GetTags())
		tags.AddAll(extraTags)
		if topic != "" {
			tags.Add(pipeline.TopicTagKey, topic)
		}
	}
	return &models.GroupInfo{Metadata: info.GetMetadata(), Tags: tags, DestinationLogstore: logstore}
}

// CollectRoute groups tagged events for a fixed destination.
func (b *Batcher) CollectRoute(ctx pipeline.PipelineContext, info *models.GroupInfo, events []models.PipelineEvent, logstore, topic string) {
	if len(events) > 0 {
		b.CollectPartition(ctx, logstore, GroupForRoute(info, logstore, topic, nil), events)
	}
}

// Copyright 2022 iLogtail Authors
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

package metadatagroup

import (
	"fmt"
	"strconv"
	"strings"
	"sync"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

const (
	maxEventsLength = 1024
	maxBytesLength  = 3 * 1024 * 1024
)

type metadataGroup struct {
	info      *models.GroupInfo
	events    []models.PipelineEvent
	byteCount int
}

type AggregatorMetadataGroup struct {
	context             pipeline.Context
	GroupMetadataKeys   []string `json:"GroupMetadataKeys,omitempty" comment:"group by metadata keys"`
	GroupMaxEventLength int      `json:"GroupMaxEventLength,omitempty" comment:"max count of events in a pipelineGroupEvents"`
	GroupMaxByteLength  int      `json:"GroupMaxByteLength,omitempty" comment:"max sum of byte length of events in a pipelineGroupEvents"`
	DropOversizeEvent   bool     `json:"DropOversizeEvent,omitempty" comment:"drop event when the event over GroupMaxByteLength"`

	mu     sync.Mutex
	groups map[string]*metadataGroup
}

func (g *AggregatorMetadataGroup) Init(ctx pipeline.Context) (int, error) {
	g.context = ctx
	if g.GroupMaxEventLength <= 0 {
		return 0, fmt.Errorf("unknown GroupMaxEventLength")
	}
	if g.GroupMaxByteLength <= 0 {
		return 0, fmt.Errorf("unknown GroupMaxByteLength")
	}
	return 0, nil
}

func (*AggregatorMetadataGroup) Description() string {
	return "aggregator that recombines event groups by group metadata"
}

func (g *AggregatorMetadataGroup) Reset() {
	g.mu.Lock()
	defer g.mu.Unlock()
	clear(g.groups)
}

func (g *AggregatorMetadataGroup) Record(input *models.PipelineGroupEvents, ctx pipeline.PipelineContext) error {
	if input == nil || len(input.Events) == 0 {
		return nil
	}

	g.mu.Lock()
	defer g.mu.Unlock()
	key := g.groupKey(input.Group)
	batch := g.groups[key]
	if batch == nil {
		metadata := models.NewMetadata()
		for _, name := range g.GroupMetadataKeys {
			metadata.Add(name, input.Group.GetMetadata().Get(name))
		}
		batch = &metadataGroup{info: &models.GroupInfo{
			Metadata:            metadata,
			Tags:                input.Group.GetTags(),
			DestinationLogstore: input.Group.GetDestinationLogstore(),
		}}
		if g.groups == nil {
			g.groups = make(map[string]*metadataGroup)
		}
		g.groups[key] = batch
	}

	collector := ctx.Collector()
	for _, event := range input.Events {
		var size int
		if bytes, ok := event.(models.ByteArray); ok {
			size = len(bytes)
			if size > g.GroupMaxByteLength && g.DropOversizeEvent {
				logger.Warningf(g.context.GetRuntimeContext(), selfmonitor.AggregateOversizeAlarm,
					"event[%s] size [%d] exceeds group limit %d, dropping event", event.GetName(), size, g.GroupMaxByteLength)
				continue
			}
		}
		if len(batch.events) >= g.GroupMaxEventLength || batch.byteCount+size > g.GroupMaxByteLength {
			batch.emit(collector)
		}
		batch.events = append(batch.events, event)
		batch.byteCount += size
		if size > g.GroupMaxByteLength {
			batch.emit(collector)
		}
	}
	return nil
}

func (g *AggregatorMetadataGroup) GetResult(ctx pipeline.PipelineContext) error {
	g.mu.Lock()
	defer g.mu.Unlock()
	collector := ctx.Collector()
	for _, group := range g.groups {
		group.emit(collector)
	}
	clear(g.groups)
	return nil
}

func (g *metadataGroup) emit(collector pipeline.PipelineCollector) {
	if len(g.events) > 0 {
		collector.Collect(g.info, g.events...)
		g.events = nil
		g.byteCount = 0
	}
}

func (g *AggregatorMetadataGroup) groupKey(info *models.GroupInfo) string {
	var key strings.Builder
	appendValue := func(value string) {
		key.WriteString(strconv.Itoa(len(value)))
		key.WriteByte(':')
		key.WriteString(value)
	}
	appendValue(info.GetDestinationLogstore())
	for _, name := range g.GroupMetadataKeys {
		appendValue(info.GetMetadata().Get(name))
	}
	for _, tag := range info.GetTags().SortTo(nil) {
		appendValue(tag.Key)
		appendValue(tag.Value)
	}
	return key.String()
}

func NewAggregatorMetadataGroup() *AggregatorMetadataGroup {
	return &AggregatorMetadataGroup{
		GroupMaxByteLength:  maxBytesLength,
		GroupMaxEventLength: maxEventsLength,
		DropOversizeEvent:   true,
	}
}

func init() {
	pipeline.Aggregators["aggregator_metadata_group"] = func() pipeline.Aggregator {
		return NewAggregatorMetadataGroup()
	}
}

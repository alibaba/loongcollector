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
	"strings"
	"sync"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

const (
	connector = "_"

	maxEventsLength = 1024
	maxBytesLength  = 3 * 1024 * 1024
)

type metadataGroup struct {
	context pipeline.Context
	group   *models.GroupInfo
	events  []models.PipelineEvent

	nowEventsLength     int
	nowEventsByteLength int
	maxEventsLength     int
	maxEventsByteLength int
	dropOversizeEvent   bool

	lock *sync.Mutex
}

func (g *metadataGroup) Record(group *models.PipelineGroupEvents, ctx pipeline.PipelineContext) error {
	g.lock.Lock()
	defer g.lock.Unlock()
	// check by bytes size, currently only works at `models.ByteArray`
	t := group.Events[0].GetType()
	switch t {
	case models.EventTypeByteArray:
		g.collectByLengthAndBytesChecker(group, ctx)
	default:
		g.collectByLengthChecker(group, ctx)
	}
	return nil
}

func (g *metadataGroup) collectByLengthChecker(group *models.PipelineGroupEvents, ctx pipeline.PipelineContext) {
	for {
		if len(group.Events) == 0 {
			break
		}
		inputSize := len(group.Events)
		availableSize := g.maxEventsLength - g.nowEventsLength
		if availableSize < 0 {
			logger.Error(g.context.GetRuntimeContext(), "RUNTIME_ALARM", "availableSize is negative")
			_ = g.GetResultWithoutLock(ctx)
			continue
		}

		if availableSize >= inputSize {
			g.events = append(g.events, group.Events...)
			g.nowEventsLength += inputSize
			break
		} else {
			g.events = append(g.events, group.Events[0:availableSize]...)
			_ = g.GetResultWithoutLock(ctx)
			group.Events = group.Events[availableSize:]
		}
	}

}

func (g *metadataGroup) collectByLengthAndBytesChecker(group *models.PipelineGroupEvents, ctx pipeline.PipelineContext) {
	for {
		if len(group.Events) == 0 {
			break
		}
		availableBytesSize := g.maxEventsByteLength - g.nowEventsByteLength
		availableLenSize := g.maxEventsLength - g.nowEventsLength
		if availableLenSize < 0 || availableBytesSize < 0 {
			logger.Error(g.context.GetRuntimeContext(), "RUNTIME_ALARM", "availableSize or availableLength is negative")
			_ = g.GetResultWithoutLock(ctx)
			continue
		}

		bytes := 0
		num := 0
		oversize := false
		for _, event := range group.Events {
			byteArray, ok := event.(models.ByteArray)
			if !ok {
				continue
			}
			if bytes+len(byteArray) > availableBytesSize || num+1 > availableLenSize {
				break
			}
			bytes += len(byteArray)
			num++
		}
		if len(g.events) == 0 && num == 0 {
			// means the first event oversize the maximum group size
			if !g.dropOversizeEvent {
				num = 1
				oversize = true
			} else {
				logger.Errorf(g.context.GetRuntimeContext(), "AGGREGATE_OVERSIZE_ALARM", "event[%s] size [%d] is over the limit size %d, the event would be dropped",
					group.Events[0].GetName(),
					len(group.Events[0].(models.ByteArray)),
					g.maxEventsByteLength)
				group.Events = group.Events[1:]
				continue
			}

		}

		if num >= len(group.Events) {
			g.events = append(g.events, group.Events...)
			g.nowEventsByteLength += bytes
			g.nowEventsLength += num
			if oversize {
				_ = g.GetResultWithoutLock(ctx)
			}
			break
		} else {
			g.events = append(g.events, group.Events[0:num]...)
			_ = g.GetResultWithoutLock(ctx)
			group.Events = group.Events[num:]
		}
	}

}

func (g *metadataGroup) GetResult(ctx pipeline.PipelineContext) error {
	g.lock.Lock()
	defer g.lock.Unlock()
	return g.GetResultWithoutLock(ctx)
}

func (g *metadataGroup) GetResultWithoutLock(ctx pipeline.PipelineContext) error {
	if len(g.events) == 0 {
		return nil
	}
	// reset
	ctx.Collector().Collect(g.group, g.events...)
	g.Reset()
	return nil
}

func (g *metadataGroup) Reset() {
	g.nowEventsLength = 0
	g.nowEventsByteLength = 0
	g.events = make([]models.PipelineEvent, 0, g.maxEventsLength)
}

type AggregatorMetadataGroup struct {
	context             pipeline.Context
	GroupMetadataKeys   []string `json:"GroupMetadataKeys,omitempty" comment:"group by metadata keys"`
	GroupMaxEventLength int      `json:"GroupMaxEventLength,omitempty" comment:"max count of events in a pipelineGroupEvents"`
	GroupMaxByteLength  int      `json:"GroupMaxByteLength,omitempty" comment:"max sum of byte length of events in a pipelineGroupEvents"`
	DropOversizeEvent   bool     `json:"DropOversizeEvent,omitempty" comment:"drop event when the event over GroupMaxByteLength"`

	groupAgg sync.Map
}

func (g *AggregatorMetadataGroup) Init(context pipeline.Context, que pipeline.LogGroupQueue) (int, error) {
	g.context = context
	if g.GroupMaxEventLength <= 0 {
		return 0, fmt.Errorf("unknown GroupMaxEventLength")
	}
	if g.GroupMaxByteLength <= 0 {
		return 0, fmt.Errorf("unknown GroupMaxByteLength")
	}
	return 0, nil
}

func (g *AggregatorMetadataGroup) Description() string {
	return "aggregator that recombine the groupEvents by group metadata"
}

func (g *AggregatorMetadataGroup) Reset() {
	g.groupAgg.Range(func(key, value any) bool {
		metaGroup := value.(*metadataGroup)
		metaGroup.Reset()
		return true
	})
}

func (g *AggregatorMetadataGroup) Record(event *models.PipelineGroupEvents, ctx pipeline.PipelineContext) error {
	return g.getOrCreateMetadataGroup(event).Record(event, ctx)
}

func (g *AggregatorMetadataGroup) GetResult(ctx pipeline.PipelineContext) error {
	g.groupAgg.Range(func(key, value any) bool {
		metaGroup := value.(*metadataGroup)
		return metaGroup.GetResult(ctx) == nil
	})
	return nil
}

func (g *AggregatorMetadataGroup) getOrCreateMetadataGroup(event *models.PipelineGroupEvents) *metadataGroup {
	aggKey, metadata := g.buildGroupKey(event)
	group, ok := g.groupAgg.Load(aggKey)
	if !ok {
		newGroup := &metadataGroup{
			group:               &models.GroupInfo{Metadata: metadata},
			maxEventsLength:     g.GroupMaxEventLength,
			maxEventsByteLength: g.GroupMaxByteLength,
			dropOversizeEvent:   g.DropOversizeEvent,
			lock:                &sync.Mutex{},
			context:             g.context,
		}
		group, _ = g.groupAgg.LoadOrStore(aggKey, newGroup)
	}
	return group.(*metadataGroup)
}

func (g *AggregatorMetadataGroup) buildGroupKey(event *models.PipelineGroupEvents) (string, models.Metadata) {
	metadataValues := strings.Builder{}
	metadataKeyValues := map[string]string{}
	for index, key := range g.GroupMetadataKeys {
		value := event.Group.GetMetadata().Get(key)
		metadataKeyValues[key] = value
		if index != 0 {
			metadataValues.WriteString(connector)
		}
		metadataValues.WriteString(value)
	}
	return metadataValues.String(), models.NewMetadataWithMap(metadataKeyValues)
}

func NewAggregatorMetadataGroup() *AggregatorMetadataGroup {
	return &AggregatorMetadataGroup{
		GroupMetadataKeys:   make([]string, 0),
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

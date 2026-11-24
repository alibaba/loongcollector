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

package helper

import (
	"slices"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

// Observable pipeline data collector, which stores data based on channal and can be subscribed by multiple consumers
type observePipeCollector struct {
	groupChan chan *models.PipelineGroupEvents
}

func (p *observePipeCollector) Collect(group *models.GroupInfo, events ...models.PipelineEvent) {
	if len(events) == 0 {
		return
	}
	p.groupChan <- &models.PipelineGroupEvents{
		Group:  group,
		Events: events,
	}
}

func (p *observePipeCollector) CollectList(groups ...*models.PipelineGroupEvents) {
	if len(groups) == 0 {
		return
	}
	for _, g := range groups {
		p.groupChan <- g
	}
}

func (p *observePipeCollector) ToArray() []*models.PipelineGroupEvents {
	totalCount := len(p.groupChan)
	results := make([]*models.PipelineGroupEvents, totalCount)
	for i := 0; i < totalCount; i++ {
		results[i] = <-p.groupChan
	}
	return results
}

func (p *observePipeCollector) Observe() chan *models.PipelineGroupEvents {
	return p.groupChan
}

func (p *observePipeCollector) Close() {
	close(p.groupChan)
}

// groupedPipeCollector group the collected PipelineEvent by groupInfo.
// The limitation is that it cannot be subscribed as it always returns an empty chan.
// so it can only return all the grouped data at one time.
type groupedPipeCollector struct {
	groupEvents map[*models.GroupInfo][]models.PipelineEvent
	firstGroup  *models.PipelineGroupEvents
}

func (p *groupedPipeCollector) Collect(group *models.GroupInfo, events ...models.PipelineEvent) {
	if len(events) == 0 {
		return
	}
	if first := p.firstGroup; first != nil {
		p.groupEvents[first.Group] = append([]models.PipelineEvent(nil), first.Events...)
		p.firstGroup = nil
	}
	p.groupEvents[group] = append(p.groupEvents[group], events...)
}

func (p *groupedPipeCollector) CollectList(groups ...*models.PipelineGroupEvents) {
	for _, group := range groups {
		p.CollectGroup(group)
	}
}

func (p *groupedPipeCollector) CollectGroup(group *models.PipelineGroupEvents) {
	if len(group.Events) == 0 {
		return
	}
	if p.firstGroup == nil && len(p.groupEvents) == 0 {
		// An in-place processor can forward the group it received.
		p.firstGroup = group
		return
	}
	p.Collect(group.Group, group.Events...)
}

func (p *groupedPipeCollector) ToArray() []*models.PipelineGroupEvents {
	return p.appendDrained(make([]*models.PipelineGroupEvents, 0, len(p.groupEvents)))
}

// AppendDrainedPipelineGroups lets the runner reuse its output slice between
// processors. It retains the ordering and ownership of the collected groups.
func AppendDrainedPipelineGroups(collector pipeline.PipelineCollector, dst []*models.PipelineGroupEvents) []*models.PipelineGroupEvents {
	if grouped, ok := collector.(*groupedPipeCollector); ok {
		return grouped.appendDrained(dst)
	}
	return append(dst, collector.ToArray()...)
}

func (p *groupedPipeCollector) appendDrained(dst []*models.PipelineGroupEvents) []*models.PipelineGroupEvents {
	if group := p.firstGroup; group != nil {
		p.firstGroup = nil
		return append(dst, group)
	}
	dst = slices.Grow(dst, len(p.groupEvents))
	for group, events := range p.groupEvents {
		dst = append(dst, &models.PipelineGroupEvents{
			Group:  group,
			Events: events,
		})
	}
	switch len(p.groupEvents) {
	case 0, 1:
		clear(p.groupEvents)
	default:
		// Keep the common single-group map, but release storage after fan-out.
		p.groupEvents = make(map[*models.GroupInfo][]models.PipelineEvent)
	}
	return dst
}

func (p *groupedPipeCollector) Observe() chan *models.PipelineGroupEvents {
	return nil
}

func (p *groupedPipeCollector) Close() {
	p.firstGroup = nil
	clear(p.groupEvents)
}

// noopPipeCollector is an empty collector implementation.
type noopPipeCollector struct {
}

func (p *noopPipeCollector) Collect(group *models.GroupInfo, events ...models.PipelineEvent) {
}

func (p *noopPipeCollector) CollectList(groups ...*models.PipelineGroupEvents) {
}

func (p *noopPipeCollector) ToArray() []*models.PipelineGroupEvents {
	return nil
}

func (p *noopPipeCollector) Observe() chan *models.PipelineGroupEvents {
	return nil
}

func (p *noopPipeCollector) Close() {
}

type defaultPipelineContext struct {
	collector pipeline.PipelineCollector
}

func (p *defaultPipelineContext) Collector() pipeline.PipelineCollector {
	return p.collector
}

func NewObservePipelineContext(queueSize int) pipeline.PipelineContext {
	return newPipelineContext(&observePipeCollector{
		groupChan: make(chan *models.PipelineGroupEvents, queueSize),
	})
}

func NewGroupedPipelineContext() pipeline.PipelineContext {
	return newPipelineContext(&groupedPipeCollector{
		groupEvents: make(map[*models.GroupInfo][]models.PipelineEvent),
	})
}

func NewNoopPipelineContext() pipeline.PipelineContext {
	return newPipelineContext(&noopPipeCollector{})
}

func newPipelineContext(collector pipeline.PipelineCollector) pipeline.PipelineContext {
	return &defaultPipelineContext{collector: collector}
}

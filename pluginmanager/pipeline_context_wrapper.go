// Copyright 2025 iLogtail Authors
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

package pluginmanager

import (
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

// PipelineContextWrapper is a wrapper of pipeline.PipelineContext
// When call Collector(), it replaces the original pipeline.PipelineCollector with PipelineCollectorWrapper
// for counting the traffic of the plugin
type PipelineContextWrapper struct {
	PipelineCollectorWrapper
}

func (wrapper *PipelineContextWrapper) Collector() pipeline.PipelineCollector {
	return wrapper.PipelineCollectorWrapper
}

// PipelineCollectorWrapper is a wrapper of pipeline.PipelineCollector with extra telemetry metrics.
type PipelineCollectorWrapper struct {
	inner pipeline.PipelineCollector

	outEventsTotal      selfmonitor.CounterMetric
	outEventGroupsTotal selfmonitor.CounterMetric
	outSizeBytes        selfmonitor.CounterMetric
}

// Collect single group and events data belonging to this group
func (collectorWrapper PipelineCollectorWrapper) Collect(groupInfo *models.GroupInfo, eventList ...models.PipelineEvent) {
	if collectorWrapper.inner == nil {
		return
	}

	collectorWrapper.outEventsTotal.Add(int64(len(eventList)))
	collectorWrapper.outEventGroupsTotal.Add(1)
	size := int64(0)
	for _, event := range eventList {
		size += event.GetSize()
	}
	collectorWrapper.outSizeBytes.Add(size)
	collectorWrapper.inner.Collect(groupInfo, eventList...)
}

// CollectList collect GroupEvents list that have been grouped
func (collectorWrapper PipelineCollectorWrapper) CollectList(groupEventsList ...*models.PipelineGroupEvents) {
	if collectorWrapper.inner == nil {
		return
	}
	var eventCount int64
	var size int64
	for _, groupEvents := range groupEventsList {
		eventCount += groupEvents.GetEventCount()
		size += groupEvents.GetSize()
	}
	collectorWrapper.outEventGroupsTotal.Add(int64(len(groupEventsList)))
	collectorWrapper.outEventsTotal.Add(eventCount)
	collectorWrapper.outSizeBytes.Add(size)
	collectorWrapper.inner.CollectList(groupEventsList...)
}

// ToArray returns an array containing all of the PipelineGroupEvents in this collector.
func (collectorWrapper PipelineCollectorWrapper) ToArray() []*models.PipelineGroupEvents {
	if collectorWrapper.inner == nil {
		return nil
	}
	return collectorWrapper.inner.ToArray()
}

// Observe returns a chan that can consume PipelineGroupEvents from this collector.
func (collectorWrapper PipelineCollectorWrapper) Observe() chan *models.PipelineGroupEvents {
	if collectorWrapper.inner == nil {
		return nil
	}
	return collectorWrapper.inner.Observe()
}

// Close closes the collector.
func (collectorWrapper PipelineCollectorWrapper) Close() {
	if collectorWrapper.inner == nil {
		return
	}
	collectorWrapper.inner.Close()
}

func newPipelineContextWrapper(pipelineContext pipeline.PipelineContext, outEventsTotal selfmonitor.CounterMetric, outEventGroupsTotal selfmonitor.CounterMetric, outSizeBytes selfmonitor.CounterMetric) *PipelineContextWrapper {
	w := &PipelineContextWrapper{}

	var innerCollector pipeline.PipelineCollector
	if pipelineContext != nil {
		innerCollector = pipelineContext.Collector()
	}

	w.PipelineCollectorWrapper = PipelineCollectorWrapper{
		inner:               innerCollector,
		outEventsTotal:      outEventsTotal,
		outEventGroupsTotal: outEventGroupsTotal,
		outSizeBytes:        outSizeBytes,
	}
	return w
}

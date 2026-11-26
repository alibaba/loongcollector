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

package pluginmanager

import (
	"time"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

type ProcessorPluginWrapper struct {
	ProcessorWrapper
	Processor pipeline.Processor

	inEventGroupsTotal   selfmonitor.CounterMetric
	outEventGroupsTotal  selfmonitor.CounterMetric
	processTimeRemainder time.Duration
}

func (wrapper *ProcessorPluginWrapper) Init(pluginMeta *pipeline.PluginMeta) error {
	wrapper.InitMetricRecord(pluginMeta)
	wrapper.inEventGroupsTotal = selfmonitor.NewCounterMetricAndRegister(wrapper.MetricRecord, selfmonitor.MetricPluginInEventGroupsTotal)
	wrapper.outEventGroupsTotal = selfmonitor.NewCounterMetricAndRegister(wrapper.MetricRecord, selfmonitor.MetricPluginOutEventGroupsTotal)

	return wrapper.Processor.Init(wrapper.Config.Context)
}

func (wrapper *ProcessorPluginWrapper) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	wrapper.inEventGroupsTotal.Add(1)
	wrapper.inEventsTotal.Add(in.GetEventCount())
	wrapper.inSizeBytes.Add(in.GetSize())

	start := time.Now()
	wrapper.Processor.Process(in, context)
	elapsed := time.Since(start) + wrapper.processTimeRemainder
	if elapsed >= time.Millisecond {
		wrapper.totalProcessTimeMs.Add(elapsed.Milliseconds())
	}
	wrapper.processTimeRemainder = elapsed % time.Millisecond
}

// recordOutput counts the groups collected by the processor, including new groups it emitted.
func (wrapper *ProcessorPluginWrapper) recordOutput(groups []*models.PipelineGroupEvents) {
	if len(groups) == 0 {
		return
	}
	var eventCount, size int64
	for _, group := range groups {
		eventCount += group.GetEventCount()
		size += group.GetSize()
	}
	wrapper.outEventGroupsTotal.Add(int64(len(groups)))
	wrapper.outEventsTotal.Add(eventCount)
	wrapper.outSizeBytes.Add(size)
}

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

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

type ProcessorWrapperV2 struct {
	ProcessorWrapper
	Processor pipeline.ProcessorV2

	inEventGroupsTotal  pipeline.CounterMetric
	outEventGroupsTotal pipeline.CounterMetric
}

func (wrapper *ProcessorWrapperV2) Init(pluginMeta *pipeline.PluginMeta) error {
	wrapper.InitMetricRecord(pluginMeta)
	wrapper.inEventGroupsTotal = helper.NewCounterMetricAndRegister(wrapper.MetricRecord, helper.MetricPluginInEventGroupsTotal)
	wrapper.outEventGroupsTotal = helper.NewCounterMetricAndRegister(wrapper.MetricRecord, helper.MetricPluginOutEventGroupsTotal)

	return wrapper.Processor.Init(wrapper.Config.Context)
}

func (wrapper *ProcessorWrapperV2) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	startTime := time.Now().UnixMilli()

	wrapper.inEventGroupsTotal.Add(1)
	wrapper.inEventsTotal.Add(int64(len(in.Events)))
	for _, event := range in.Events {
		wrapper.inSizeBytes.Add(event.GetSize())
	}

	wrapper.Processor.Process(in, context)

	wrapper.outEventGroupsTotal.Add(1)
	wrapper.outEventsTotal.Add(int64(len(in.Events)))
	for _, event := range in.Events {
		wrapper.outSizeBytes.Add(event.GetSize())
	}
	wrapper.totalProcessTimeMs.Add(time.Now().UnixMilli() - startTime)
}

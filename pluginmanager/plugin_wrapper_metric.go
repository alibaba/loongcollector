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

	"github.com/alibaba/ilogtail/pkg/pipeline"
)

type MetricInputWrapper struct {
	MetricWrapper
	pipelineCtxWrapper *PipelineContextWrapper // keep the pointer to avoid alloc and GC
	Input              pipeline.MetricInput
	collector          *inputCollector
}

func (wrapper *MetricInputWrapper) Init(pluginMeta *pipeline.PluginMeta, inputInterval int) error {
	if _, ok := wrapper.Input.(pipeline.MetricInputV2); !ok {
		if _, ok := wrapper.Input.(pipeline.MetricInputV1); !ok {
			return pluginUnImplementError(pluginMetricInput, pluginMeta.PluginType)
		}
	}
	wrapper.InitMetricRecord(pluginMeta)

	interval, err := wrapper.Input.Init(wrapper.Config.Context)
	if err != nil {
		return err
	}
	if interval == 0 {
		interval = inputInterval
	}
	wrapper.Interval = time.Duration(interval) * time.Millisecond
	wrapper.pipelineCtxWrapper = newPipelineContextWrapper(nil, wrapper.outEventsTotal, wrapper.outEventGroupsTotal, wrapper.outSizeBytes)
	wrapper.collector = &inputCollector{
		context:        wrapper.pipelineCtxWrapper,
		configTags:     wrapper.Tags,
		runtimeContext: wrapper.Config.Context.GetRuntimeContext(),
	}
	return nil
}

func (wrapper *MetricInputWrapper) Read(pipelineContext pipeline.PipelineContext) error {
	wrapper.pipelineCtxWrapper.PipelineCollectorWrapper.inner = pipelineContext.Collector()
	if input, ok := wrapper.Input.(pipeline.MetricInputV2); ok {
		return input.Read(wrapper.pipelineCtxWrapper)
	}
	return wrapper.Input.(pipeline.MetricInputV1).Collect(wrapper.collector)
}

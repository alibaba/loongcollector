// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
package pluginmanager

import (
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

type ServiceInputWrapper struct {
	ServiceWrapper
	pipelineCtxWrapper *PipelineContextWrapper
	Input              pipeline.ServiceInput
}

func (wrapper *ServiceInputWrapper) Init(pluginMeta *pipeline.PluginMeta) error {
	if _, ok := wrapper.Input.(pipeline.ServiceInputV2); !ok {
		if _, ok := wrapper.Input.(pipeline.ServiceInputV1); !ok {
			return pluginUnImplementError(pluginServiceInput, pluginMeta.PluginType)
		}
	}
	wrapper.InitMetricRecord(pluginMeta)

	_, err := wrapper.Input.Init(wrapper.Config.Context)
	return err
}

func (wrapper *ServiceInputWrapper) StartService(pipelineContext pipeline.PipelineContext) error {
	wrapper.pipelineCtxWrapper = newPipelineContextWrapper(pipelineContext, wrapper.outEventsTotal, wrapper.outEventGroupsTotal, wrapper.outSizeBytes)
	if input, ok := wrapper.Input.(pipeline.ServiceInputV2); ok {
		return input.StartService(wrapper.pipelineCtxWrapper)
	}
	return wrapper.Input.(pipeline.ServiceInputV1).Start(&inputCollector{context: wrapper.pipelineCtxWrapper})
}

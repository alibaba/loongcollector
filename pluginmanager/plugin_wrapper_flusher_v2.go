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
)

type FlusherWrapperV2 struct {
	FlusherWrapper
	Flusher pipeline.FlusherV2
}

func (wrapper *FlusherWrapperV2) Init(pluginMeta *pipeline.PluginMeta) error {
	wrapper.InitMetricRecord(pluginMeta)

	return wrapper.Flusher.Init(wrapper.Config.Context)
}

func (wrapper *FlusherWrapperV2) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return wrapper.Flusher.IsReady(projectName, logstoreName, logstoreKey)
}

func (wrapper *FlusherWrapperV2) Export(pipelineGroupEvents []*models.PipelineGroupEvents, pipelineContext pipeline.PipelineContext) error {
	startTime := time.Now()
	for _, groups := range pipelineGroupEvents {
		wrapper.inEventsTotal.Add(int64(len(groups.Events)))
		wrapper.inEventGroupsTotal.Add(1)
		for _, event := range groups.Events {
			wrapper.inSizeBytes.Add(event.GetSize())
		}
	}

	err := wrapper.Flusher.Export(pipelineGroupEvents, pipelineContext)

	wrapper.totalDelayTimeMs.Add(time.Since(startTime).Milliseconds())
	return err
}

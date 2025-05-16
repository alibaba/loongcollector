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

package pluginmanager

import (
	"github.com/alibaba/ilogtail/pkg/helper/containercenter"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type InputContainer struct {
	context pipeline.Context
}

func (r *InputContainer) Init(context pipeline.Context) (int, error) {
	r.context = context
	containercenter.InitContainer()
	return 0, nil
}

func (r *InputContainer) Description() string {
	return "container input plugin for logtail"
}

func (r *InputContainer) Collect(collector pipeline.Collector) error {
	loggroup := &protocol.LogGroup{}

	CollectContainers(loggroup)
	CollectConfigResult(loggroup)

	if len(loggroup.Logs) > 0 && ContainerConfig != nil {
		for _, log := range loggroup.Logs {
			ContainerConfig.PluginRunner.ReceiveRawLog(&pipeline.LogWithContext{Log: log})
		}
	}
	return nil
}

func init() {
	pipeline.MetricInputs["metric_container"] = func() pipeline.MetricInput {
		return &InputContainer{}
	}
}

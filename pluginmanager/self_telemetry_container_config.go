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
	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

type InputContainer struct {
	context ilogtail.Context
}

func (r *InputContainer) Init(context ilogtail.Context) (int, error) {
	r.context = context
	util.InitContainer()
	return 0, nil
}

func (r *InputContainer) Description() string {
	return "container input plugin for logtail"
}

func (r *InputContainer) Collect(collector ilogtail.Collector) error {
	loggroup := &protocol.LogGroup{}

	CollectContainers(loggroup)
	CollectDeleteContainers(loggroup)
	CollectConfigResult(loggroup)

	if len(loggroup.Logs) > 0 && ContainerConfig != nil {
		for _, log := range loggroup.Logs {
			ContainerConfig.PluginRunner.ReceiveRawLog(&ilogtail.LogWithContext{Log: log})
		}
	}
	return nil
}

func init() {
	ilogtail.MetricInputs["metric_container"] = func() ilogtail.MetricInput {
		return &InputContainer{}
	}
}

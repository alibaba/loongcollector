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
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

type InputContainer struct{}

func (*InputContainer) Init(pipeline.Context) (int, error) {
	containercenter.InitContainer()
	return 0, nil
}

func (*InputContainer) Description() string {
	return "container input plugin for logtail"
}

func (*InputContainer) Read(ctx pipeline.PipelineContext) error {
	events := containercenter.AppendContainerConfigResultEvents(collectContainers(nil))
	for _, event := range events {
		// A full container snapshot can exceed the maximum wire group size.
		ctx.Collector().Collect(models.NewGroup(models.NewMetadata(), models.NewTags()), event)
	}
	return nil
}

func init() {
	pipeline.MetricInputs["metric_container"] = func() pipeline.MetricInput {
		return &InputContainer{}
	}
}

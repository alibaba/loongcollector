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

package aggregator

import (
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

// defaultAggregator forwards groups already formed by the input or processor.
type defaultAggregator struct{}

func (*defaultAggregator) Init(pipeline.Context) (int, error)       { return 0, nil }
func (*defaultAggregator) Description() string                      { return "default event group aggregator" }
func (*defaultAggregator) Reset()                                   {}
func (*defaultAggregator) GetResult(pipeline.PipelineContext) error { return nil }

func (*defaultAggregator) Record(group *models.PipelineGroupEvents, ctx pipeline.PipelineContext) error {
	if group != nil && len(group.Events) > 0 {
		ctx.Collector().CollectList(group)
	}
	return nil
}

func init() {
	pipeline.Aggregators["aggregator_default"] = func() pipeline.Aggregator {
		return &defaultAggregator{}
	}
}

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

package baseagg

import (
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

const (
	MaxLogCount     = 1024
	MaxLogGroupSize = 3 * 1024 * 1024
)

// AggregatorBase batches events with matching group metadata and tags.
type AggregatorBase struct {
	MaxLogGroupCount int
	MaxLogCount      int
	PackFlag         bool
	Topic            string

	batches Batcher
}

func (p *AggregatorBase) Init(ctx pipeline.Context) (int, error) {
	return 0, p.batches.Init(ctx, p.MaxLogCount, p.MaxLogGroupCount, p.PackFlag)
}

func (*AggregatorBase) Description() string {
	return "base aggregator for logtail"
}

func (p *AggregatorBase) Record(group *models.PipelineGroupEvents, ctx pipeline.PipelineContext) error {
	if group != nil {
		p.batches.Collect(ctx, GroupForRoute(group.Group, "", p.Topic, nil), group.Events)
	}
	return nil
}

func (p *AggregatorBase) GetResult(ctx pipeline.PipelineContext) error {
	p.batches.CollectPending(ctx)
	return nil
}

func (p *AggregatorBase) Reset() {
	p.batches.Reset()
}

func NewAggregatorBase() *AggregatorBase {
	return &AggregatorBase{
		MaxLogGroupCount: 4,
		MaxLogCount:      MaxLogCount,
		PackFlag:         true,
	}
}

func init() {
	pipeline.Aggregators["aggregator_base"] = func() pipeline.Aggregator {
		return NewAggregatorBase()
	}
}

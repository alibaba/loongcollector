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

package context

import (
	"strings"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/aggregator/baseagg"
)

// AggregatorContext groups events with matching source metadata and tags.
type AggregatorContext struct {
	MaxLogGroupCount int
	MaxLogCount      int
	Topic            string
	PackFlag         bool
	// ContextPreservationToleranceSize controls when inactive pack sequences are pruned.
	ContextPreservationToleranceSize int

	batches baseagg.Batcher
}

func (p *AggregatorContext) Init(ctx pipeline.Context) (int, error) {
	if err := p.batches.Init(ctx, p.MaxLogCount, p.MaxLogGroupCount, p.PackFlag); err != nil {
		return 0, err
	}
	p.batches.UseSourcePackIDs(p.ContextPreservationToleranceSize)
	return 0, nil
}

func (*AggregatorContext) Description() string {
	return "context aggregator for logtail"
}

func (p *AggregatorContext) Record(group *models.PipelineGroupEvents, ctx pipeline.PipelineContext) error {
	if group != nil {
		topic := p.Topic
		metadata := group.Group.GetMetadata()
		tags := group.Group.GetTags()
		if tags.Contains(pipeline.TopicTagKey) {
			topic = tags.Get(pipeline.TopicTagKey)
		} else if metadata.Contains("topic") {
			topic = metadata.Get("topic")
		}
		source := metadata.Get("source")
		if source == "" {
			source = metadata.Get("source_id")
		}
		if source != "" && !strings.HasSuffix(source, "-") {
			source += "-"
		}
		info := baseagg.GroupForRoute(group.Group, "", topic, nil)
		p.batches.CollectPartition(ctx, source, info, group.Events)
	}
	return nil
}

func (p *AggregatorContext) GetResult(ctx pipeline.PipelineContext) error {
	p.batches.CollectPending(ctx)
	return nil
}

func (p *AggregatorContext) Reset() {
	p.batches.Reset()
}

func NewAggregatorContext() *AggregatorContext {
	return &AggregatorContext{
		MaxLogGroupCount:                 2,
		MaxLogCount:                      baseagg.MaxLogCount,
		ContextPreservationToleranceSize: 10,
		PackFlag:                         true,
	}
}

func init() {
	pipeline.Aggregators["aggregator_context"] = func() pipeline.Aggregator {
		return NewAggregatorContext()
	}
}

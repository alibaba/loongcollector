// Copyright 2022 iLogtail Authors
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

package contentvaluegroup

import (
	"fmt"
	"strconv"
	"strings"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	"github.com/alibaba/ilogtail/plugins/aggregator/baseagg"
)

type AggregatorContentValueGroup struct {
	GroupKeys        []string
	Topic            string
	ErrIfKeyNotFound bool
	EnablePackID     bool

	context pipeline.Context
	batches baseagg.Batcher
}

func (*AggregatorContentValueGroup) Description() string {
	return "aggregator that groups logs by a set of keys"
}

func (g *AggregatorContentValueGroup) Init(ctx pipeline.Context) (int, error) {
	if len(g.GroupKeys) == 0 {
		return 0, fmt.Errorf("must specify GroupKeys")
	}
	g.context = ctx
	return 0, g.batches.Init(ctx, baseagg.MaxLogCount, 4, g.EnablePackID)
}

func (g *AggregatorContentValueGroup) Record(in *models.PipelineGroupEvents, ctx pipeline.PipelineContext) error {
	if in == nil {
		return nil
	}
	type partition struct {
		tags   map[string]string
		events []models.PipelineEvent
	}
	partitions := make(map[string]*partition)
	var passThrough []models.PipelineEvent
	for _, event := range in.Events {
		isLog := event.GetType() == models.EventTypeLogging
		var key strings.Builder
		var hasField bool
		tags := make(map[string]string, len(g.GroupKeys))
		for _, name := range g.GroupKeys {
			value, found := baseagg.FindEventField(event, name)
			hasField = hasField || found
			if !found && isLog && g.ErrIfKeyNotFound {
				logger.Warning(g.context.GetRuntimeContext(), selfmonitor.AggGroupAlarm, "aggregator could not find log field", name)
			}
			key.WriteString(strconv.Itoa(len(value)))
			key.WriteByte(':')
			key.WriteString(value)
			tags[name] = value
		}
		if !isLog && !hasField {
			passThrough = append(passThrough, event)
			continue
		}
		keyValue := key.String()
		group := partitions[keyValue]
		if group == nil {
			group = &partition{tags: tags}
			partitions[keyValue] = group
		}
		group.events = append(group.events, event)
	}
	for key, group := range partitions {
		g.batches.CollectPartition(ctx, key, baseagg.GroupForRoute(in.Group, "", g.Topic, group.tags), group.events)
	}
	ctx.Collector().Collect(in.Group, passThrough...)
	return nil
}

func (g *AggregatorContentValueGroup) GetResult(ctx pipeline.PipelineContext) error {
	g.batches.CollectPending(ctx)
	return nil
}

func (g *AggregatorContentValueGroup) Reset() {
	g.batches.Reset()
}

func init() {
	pipeline.Aggregators["aggregator_content_value_group"] = func() pipeline.Aggregator {
		return &AggregatorContentValueGroup{EnablePackID: true}
	}
}

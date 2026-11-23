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

package logstorerouter

import (
	"fmt"
	"regexp"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	"github.com/alibaba/ilogtail/plugins/aggregator/baseagg"
)

type AggregatorRouter struct {
	MaxLogGroupCount int
	MaxLogCount      int
	PackFlag         bool
	Topic            string
	DropDisMatch     bool
	SourceKey        string
	NoMatchError     bool
	RouterRegex      []string
	RouterLogstore   []string

	rules   []*regexp.Regexp
	context pipeline.Context
	batches baseagg.Batcher
}

func (p *AggregatorRouter) Init(ctx pipeline.Context) (int, error) {
	if len(p.RouterRegex) != len(p.RouterLogstore) {
		return 0, fmt.Errorf("router regex count %d, logstore count %d", len(p.RouterRegex), len(p.RouterLogstore))
	}
	p.rules = make([]*regexp.Regexp, len(p.RouterRegex))
	for i, expression := range p.RouterRegex {
		rule, err := regexp.Compile(expression)
		if err != nil {
			return 0, fmt.Errorf("compile router regex %q: %w", expression, err)
		}
		p.rules[i] = rule
	}
	p.context = ctx
	return 0, p.batches.Init(ctx, p.MaxLogCount, p.MaxLogGroupCount, p.PackFlag)
}

func (*AggregatorRouter) Description() string {
	return "aggregator router for logtail"
}

func (p *AggregatorRouter) Record(in *models.PipelineGroupEvents, ctx pipeline.PipelineContext) error {
	if in == nil {
		return nil
	}
	routes := make(map[string][]models.PipelineEvent)
	var passThrough []models.PipelineEvent
	for _, event := range in.Events {
		logstore, keep, handled := p.route(event)
		if !handled {
			passThrough = append(passThrough, event)
			continue
		}
		if keep {
			routes[logstore] = append(routes[logstore], event)
		}
	}
	for logstore, events := range routes {
		p.batches.CollectRoute(ctx, in.Group, events, logstore, p.Topic)
	}
	ctx.Collector().Collect(in.Group, passThrough...)
	return nil
}

func (p *AggregatorRouter) route(event models.PipelineEvent) (logstore string, keep, handled bool) {
	key := p.SourceKey
	if key == "" {
		switch typed := event.(type) {
		case *models.Log:
			key = defaultSourceKey(typed)
		case *models.Metric:
			key = "__name__"
		}
	}
	value, found := baseagg.FindEventField(event, key)
	if !found && event.GetType() != models.EventTypeLogging {
		return "", false, false
	}
	if found {
		for i, rule := range p.rules {
			if match := rule.FindStringIndex(value); match != nil && match[0] == 0 && match[1] == len(value) {
				return p.RouterLogstore[i], true, true
			}
		}
	}
	if p.DropDisMatch && p.NoMatchError {
		logger.Warning(p.context.GetRuntimeContext(), selfmonitor.NoMatchRouterAlarm, "no matching route", "drop this log")
	}
	return "", !p.DropDisMatch, true
}

func defaultSourceKey(log *models.Log) string {
	fields := log.GetIndices()
	if first, ordered := models.FirstLogContentKey(fields); ordered {
		return first
	}
	if fields.Contains(models.BodyKey) {
		return models.BodyKey
	}
	// An unordered native log has no first field. Pick a stable key.
	var key string
	first := true
	for candidate := range fields.Iterator() {
		if first || candidate < key {
			key = candidate
			first = false
		}
	}
	return key
}

func (p *AggregatorRouter) GetResult(ctx pipeline.PipelineContext) error {
	p.batches.CollectPending(ctx)
	return nil
}

func (p *AggregatorRouter) Reset() {
	p.batches.Reset()
}

func NewAggregatorRouter() *AggregatorRouter {
	return &AggregatorRouter{
		MaxLogGroupCount: 4,
		MaxLogCount:      baseagg.MaxLogCount,
		PackFlag:         true,
	}
}

func init() {
	pipeline.Aggregators["aggregator_logstore_router"] = func() pipeline.Aggregator {
		return NewAggregatorRouter()
	}
}

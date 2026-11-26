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

package skywalking

import (
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	"github.com/alibaba/ilogtail/plugins/aggregator/baseagg"
)

const (
	DefaultMetricsLogstore = "skywalking-metrics"
	DefaultTraceLogstore   = "skywalking-traces"
	DefaultLogLogstore     = "skywalking-logs"
)

type AggregatorSkywalking struct {
	MaxLogGroupCount int
	MaxLogCount      int
	PackFlag         bool
	Topic            string
	MetricsLogstore  string
	TraceLogstore    string
	LogLogstore      string

	context pipeline.Context
	batches baseagg.Batcher
}

func (p *AggregatorSkywalking) Init(ctx pipeline.Context) (int, error) {
	if p.MetricsLogstore == "" {
		p.MetricsLogstore = DefaultMetricsLogstore
	}
	if p.TraceLogstore == "" {
		p.TraceLogstore = DefaultTraceLogstore
	}
	if p.LogLogstore == "" {
		p.LogLogstore = DefaultLogLogstore
	}
	p.context = ctx
	return 0, p.batches.Init(ctx, p.MaxLogCount, p.MaxLogGroupCount, p.PackFlag)
}

func (*AggregatorSkywalking) Description() string {
	return "aggregator router for skywalking"
}

func (p *AggregatorSkywalking) Record(in *models.PipelineGroupEvents, ctx pipeline.PipelineContext) error {
	if in == nil {
		return nil
	}
	routes := make(map[string][]models.PipelineEvent, 3)
	for _, event := range in.Events {
		logstore := p.destination(event)
		routes[logstore] = append(routes[logstore], event)
	}
	for logstore, events := range routes {
		p.batches.CollectRoute(ctx, in.Group, events, logstore, p.Topic)
	}
	return nil
}

func (p *AggregatorSkywalking) destination(event models.PipelineEvent) string {
	switch event.GetType() {
	case models.EventTypeMetric:
		return p.MetricsLogstore
	case models.EventTypeSpan:
		return p.TraceLogstore
	case models.EventTypeLogging:
		log := event.(*models.Log)
		fields := log.GetIndices()
		key, ordered := models.FirstLogContentKey(fields)
		if !ordered {
			for _, candidate := range []string{"__name__", "links", "otlp.name"} {
				if fields.Contains(candidate) {
					key = candidate
					break
				}
			}
		}
		switch key {
		case "__name__":
			return p.MetricsLogstore
		case "links":
			return p.TraceLogstore
		case "otlp.name":
			return p.LogLogstore
		}
		logger.Warning(p.context.GetRuntimeContext(), selfmonitor.SkywalkingTopicNotRecognized, "topic not recognized")
		return p.LogLogstore
	default:
		return ""
	}
}

func (p *AggregatorSkywalking) GetResult(ctx pipeline.PipelineContext) error {
	p.batches.CollectPending(ctx)
	return nil
}

func (p *AggregatorSkywalking) Reset() {
	p.batches.Reset()
}

func NewAggregatorSkywalking() *AggregatorSkywalking {
	return &AggregatorSkywalking{
		MaxLogGroupCount: 4,
		MaxLogCount:      baseagg.MaxLogCount,
		PackFlag:         true,
	}
}

func init() {
	pipeline.Aggregators["aggregator_skywalking"] = func() pipeline.Aggregator {
		return NewAggregatorSkywalking()
	}
}

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

package opentelemetry

import (
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/aggregator/baseagg"
)

const (
	DefaultMetricsLogstore = "otlp-metrics"
	DefaultTraceLogstore   = "otlp-traces"
	DefaultLogLogstore     = "otlp-logs"
)

type AggregatorOpenTelemetry struct {
	MaxLogGroupCount int
	MaxLogCount      int
	PackFlag         bool
	Topic            string
	MetricsLogstore  string
	TraceLogstore    string
	LogLogstore      string

	batches baseagg.Batcher
}

func (p *AggregatorOpenTelemetry) Init(ctx pipeline.Context) (int, error) {
	if p.MetricsLogstore == "" {
		p.MetricsLogstore = DefaultMetricsLogstore
	}
	if p.TraceLogstore == "" {
		p.TraceLogstore = DefaultTraceLogstore
	}
	if p.LogLogstore == "" {
		p.LogLogstore = DefaultLogLogstore
	}
	return 0, p.batches.Init(ctx, p.MaxLogCount, p.MaxLogGroupCount, p.PackFlag)
}

func (*AggregatorOpenTelemetry) Description() string {
	return "aggregator router for opentelemetry"
}

func (p *AggregatorOpenTelemetry) Record(in *models.PipelineGroupEvents, ctx pipeline.PipelineContext) error {
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

func (p *AggregatorOpenTelemetry) destination(event models.PipelineEvent) string {
	switch event.GetType() {
	case models.EventTypeMetric:
		return p.MetricsLogstore
	case models.EventTypeSpan:
		return p.TraceLogstore
	case models.EventTypeLogging:
		fields := event.(*models.Log).GetIndices()
		// Older OTLP receivers could send flattened metrics or traces through
		// the raw log API. Ordinary logs may have few fields as well.
		switch {
		case fields.Contains("__name__") && fields.Contains("__time_nano__"):
			return p.MetricsLogstore
		case fields.Contains("traceID") && fields.Contains("spanID") && fields.Contains("start") && fields.Contains("end"):
			return p.TraceLogstore
		default:
			return p.LogLogstore
		}
	default:
		return ""
	}
}

func (p *AggregatorOpenTelemetry) GetResult(ctx pipeline.PipelineContext) error {
	p.batches.CollectPending(ctx)
	return nil
}

func (p *AggregatorOpenTelemetry) Reset() {
	p.batches.Reset()
}

func NewAggregatorOpenTelemetry() *AggregatorOpenTelemetry {
	return &AggregatorOpenTelemetry{
		MaxLogGroupCount: 4,
		MaxLogCount:      baseagg.MaxLogCount,
		PackFlag:         true,
	}
}

func init() {
	pipeline.Aggregators["aggregator_opentelemetry"] = func() pipeline.Aggregator {
		return NewAggregatorOpenTelemetry()
	}
}

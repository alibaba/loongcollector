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

package opentelemetry

import (
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
	baseagg "github.com/alibaba/ilogtail/plugins/aggregator/baseagg"
)

const (
	MaxLogCount     = 1024
	MaxLogGroupSize = 3 * 1024 * 1024
)

const DefaultMetricsLogstore = "otlp-metrics"
const DefaultTraceLogstore = "otlp-traces"
const DefaultLogLogstore = "otlp-logs"

type AggregatorOpenTelemetry struct {
	MaxLogGroupCount int
	MaxLogCount      int
	PackFlag         bool
	Topic            string
	MetricsLogstore  string
	TraceLogstore    string
	LogLogstore      string

	metricsAgg *baseagg.AggregatorBase
	traceAgg   *baseagg.AggregatorBase
	logAgg     *baseagg.AggregatorBase
	Lock       *sync.Mutex
	context    pipeline.Context
	queue      pipeline.LogGroupQueue
}

func (p *AggregatorOpenTelemetry) Init(context pipeline.Context, que pipeline.LogGroupQueue) (int, error) {
	if p.MetricsLogstore == "" {
		p.MetricsLogstore = DefaultMetricsLogstore
	}
	if p.TraceLogstore == "" {
		p.TraceLogstore = DefaultTraceLogstore
	}
	if p.LogLogstore == "" {
		p.LogLogstore = DefaultLogLogstore
	}

	p.context = context
	p.queue = que
	p.metricsAgg = baseagg.NewAggregatorBase()
	if _, err := p.metricsAgg.Init(context, que); err != nil {
		return 0, err
	}

	p.metricsAgg.InitInner(p.PackFlag, p.context.GetConfigName()+p.MetricsLogstore+util.GetIPAddress()+time.Now().String(), p.Lock, p.MetricsLogstore, p.Topic, p.MaxLogCount, p.MaxLogGroupCount)

	p.traceAgg = baseagg.NewAggregatorBase()
	if _, err := p.traceAgg.Init(context, que); err != nil {
		return 0, err
	}
	p.traceAgg.InitInner(p.PackFlag, p.context.GetConfigName()+p.TraceLogstore+util.GetIPAddress()+time.Now().String(), p.Lock, p.TraceLogstore, p.Topic, p.MaxLogCount, p.MaxLogGroupCount)

	p.logAgg = baseagg.NewAggregatorBase()
	if _, err := p.logAgg.Init(context, que); err != nil {
		return 0, err
	}
	p.logAgg.InitInner(p.PackFlag, p.context.GetConfigName()+p.LogLogstore+util.GetIPAddress()+time.Now().String(), p.Lock, p.LogLogstore, p.Topic, p.MaxLogCount, p.MaxLogGroupCount)
	return 0, nil
}

func (*AggregatorOpenTelemetry) Description() string {
	return "aggregator router for opentelemetry"
}

// Add adds @log to aggregator.
// Add use first content as route key
// Add returns any error encountered, nil means success.
func (p *AggregatorOpenTelemetry) Add(log *protocol.Log, ctx map[string]interface{}) error {
	if len(log.Contents) > 0 {
		if len(log.Contents) <= 5 {
			return p.metricsAgg.Add(log, ctx)
		}
		if len(log.Contents) >= 19 {
			return p.traceAgg.Add(log, ctx)
		}
		return p.logAgg.Add(log, ctx)
	}
	return nil
}

func (p *AggregatorOpenTelemetry) Flush() []*protocol.LogGroup {
	logGroups := []*protocol.LogGroup{}
	logGroups = append(logGroups, p.metricsAgg.Flush()...)
	logGroups = append(logGroups, p.traceAgg.Flush()...)
	logGroups = append(logGroups, p.logAgg.Flush()...)
	return logGroups
}

func (p *AggregatorOpenTelemetry) Reset() {
	p.metricsAgg.Reset()
	p.traceAgg.Reset()
	p.logAgg.Reset()
}

func NewAggregatorOpenTelemetry() *AggregatorOpenTelemetry {
	return &AggregatorOpenTelemetry{
		MaxLogGroupCount: 4,
		MaxLogCount:      MaxLogCount,
		PackFlag:         true,
		Lock:             &sync.Mutex{},
	}
}
func init() {
	pipeline.Aggregators["aggregator_opentelemetry"] = func() pipeline.Aggregator {
		return NewAggregatorOpenTelemetry()
	}
}

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

package skywalking

import (
	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/aggregator/defaultone"

	"sync"
	"time"
)

const (
	MaxLogCount     = 1024
	MaxLogGroupSize = 3 * 1024 * 1024
)

const DefaultMetricsLogstore = "skywalking-metrics"
const DefaultTraceLogstore = "skywalking-traces"
const DefaultLogLogstore = "skywalking-logs"

type AggregatorSkywalking struct {
	MaxLogGroupCount int
	MaxLogCount      int
	PackFlag         bool
	Topic            string
	MetricsLogstore  string
	TraceLogstore    string
	LogLogstore      string

	metricsAgg *defaultone.AggregatorDefault
	traceAgg   *defaultone.AggregatorDefault
	logAgg     *defaultone.AggregatorDefault
	Lock       *sync.Mutex
	context    ilogtail.Context
	queue      ilogtail.LogGroupQueue
}

func (p *AggregatorSkywalking) Init(context ilogtail.Context, que ilogtail.LogGroupQueue) (int, error) {
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
	p.metricsAgg = defaultone.NewAggregatorDefault()
	if _, err := p.metricsAgg.Init(context, que); err != nil {
		return 0, err
	}
	p.metricsAgg.InitInner(p.PackFlag, p.context.GetConfigName()+p.MetricsLogstore+util.GetIPAddress()+time.Now().String(), p.Lock, p.MetricsLogstore, p.Topic, p.MaxLogCount, p.MaxLogGroupCount)

	p.traceAgg = defaultone.NewAggregatorDefault()
	if _, err := p.traceAgg.Init(context, que); err != nil {
		return 0, err
	}
	p.traceAgg.InitInner(p.PackFlag, p.context.GetConfigName()+p.TraceLogstore+util.GetIPAddress()+time.Now().String(), p.Lock, p.TraceLogstore, p.Topic, p.MaxLogCount, p.MaxLogGroupCount)

	p.logAgg = defaultone.NewAggregatorDefault()
	if _, err := p.logAgg.Init(context, que); err != nil {
		return 0, err
	}
	p.logAgg.InitInner(p.PackFlag, p.context.GetConfigName()+p.LogLogstore+util.GetIPAddress()+time.Now().String(), p.Lock, p.LogLogstore, p.Topic, p.MaxLogCount, p.MaxLogGroupCount)
	return 0, nil
}

func (*AggregatorSkywalking) Description() string {
	return "aggregator router for skywalking"
}

// Add adds @log to aggregator.
// Add use first content as route key
// Add returns any error encountered, nil means success.
func (p *AggregatorSkywalking) Add(log *protocol.Log) error {
	if len(log.Contents) > 0 {
		routeKey := log.Contents[0]
		switch routeKey.Key {
		case "__name__":
			return p.metricsAgg.Add(log)
		case "links":
			return p.traceAgg.Add(log)
		case "otlp.name":
			return p.logAgg.Add(log)
		default:
			logger.Warning(p.context.GetRuntimeContext(), "SKYWALKING_TOPIC_NOT_RECOGNIZED", "error", "topic not recognized", "topic", routeKey.Value)
			return p.logAgg.Add(log)
		}
	}
	return nil
}

func (p *AggregatorSkywalking) Flush() []*protocol.LogGroup {
	logGroups := []*protocol.LogGroup{}
	logGroups = append(logGroups, p.metricsAgg.Flush()...)
	logGroups = append(logGroups, p.traceAgg.Flush()...)
	logGroups = append(logGroups, p.logAgg.Flush()...)
	return logGroups
}

func (p *AggregatorSkywalking) Reset() {
	p.metricsAgg.Reset()
	p.traceAgg.Reset()
	p.logAgg.Reset()
}

func NewAggregatorSkywalking() *AggregatorSkywalking {
	return &AggregatorSkywalking{
		MaxLogGroupCount: 4,
		MaxLogCount:      MaxLogCount,
		PackFlag:         true,
		Lock:             &sync.Mutex{},
	}
}
func init() {
	ilogtail.Aggregators["aggregator_skywalking"] = func() ilogtail.Aggregator {
		return NewAggregatorSkywalking()
	}
}

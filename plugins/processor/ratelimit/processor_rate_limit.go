// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
package ratelimit

import (
	"sort"
	"strings"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

type ProcessorRateLimit struct {
	Fields []string `comment:"Optional. Fields of value to be limited, for each unique result from combining these field values."`
	Limit  string   `comment:"Optional. Limit rate in the format of (number)/(time unit). Supported time unit: 's' (per second), 'm' (per minute), and 'h' (per hour)."`

	Algorithm   algorithm
	limitMetric selfmonitor.CounterMetric
	context     pipeline.Context
}

const pluginType = "processor_rate_limit"

func (p *ProcessorRateLimit) Init(context pipeline.Context) error {
	p.context = context

	limit := rate{}
	err := limit.Unpack(p.Limit)
	if err != nil {
		return err
	}
	p.Algorithm = newTokenBucket(limit)

	// Sort the limit-key fields once here rather than on every makeKey call:
	// Fields is fixed config, and sorting it in the hot path would both waste
	// work and race when processors run concurrently on the shared slice.
	sort.Strings(p.Fields)

	metricsRecord := p.context.GetMetricRecord()
	p.limitMetric = selfmonitor.NewCounterMetricAndRegister(metricsRecord, selfmonitor.MetricPluginDiscardedEventsTotal)
	return nil
}

func (*ProcessorRateLimit) Description() string {
	return "rate limit processor for logtail"
}

func init() {
	pipeline.Processors[pluginType] = func() pipeline.Processor {
		return &ProcessorRateLimit{}
	}
}

// makeKey joins the configured field values to select a token bucket.
func (p *ProcessorRateLimit) makeKey(log *models.Log) string {
	if len(p.Fields) == 0 {
		return ""
	}

	// Fields is already sorted in Init; start from len=0 (see makeKey) so the
	// key has exactly one segment per field with no empty leading segments.
	values := make([]string, 0, len(p.Fields))
	contents := log.GetIndices()
	for _, field := range p.Fields {
		value := ""
		if contents.Contains(field) {
			value = pipeline.GetStringValue(contents.Get(field))
		}
		values = append(values, value)
	}

	return strings.Join(values, "_")
}

// Process rate-limits logs and passes other event types through.
func (p *ProcessorRateLimit) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	if in == nil {
		return
	}
	survivors := make([]models.PipelineEvent, 0, len(in.Events))
	for _, event := range in.Events {
		if event.GetType() == models.EventTypeLogging {
			key := p.makeKey(event.(*models.Log))
			if p.Algorithm.IsAllowed(key) {
				survivors = append(survivors, event)
			} else {
				p.limitMetric.Add(1)
			}
			continue
		}
		// Metric/Span/other events are never dropped.
		survivors = append(survivors, event)
	}
	context.Collector().Collect(in.Group, survivors...)
}

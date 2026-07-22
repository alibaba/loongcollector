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
	"fmt"
	"sort"
	"strings"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
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
	// work and race when Process/ProcessLogs run concurrently on the shared slice.
	sort.Strings(p.Fields)

	metricsRecord := p.context.GetMetricRecord()
	p.limitMetric = selfmonitor.NewCounterMetricAndRegister(metricsRecord, selfmonitor.MetricPluginDiscardedEventsTotal)
	return nil
}

func (*ProcessorRateLimit) Description() string {
	return "rate limit processor for logtail"
}

// V1
func (p *ProcessorRateLimit) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	totalLen := len(logArray)
	nextIdx := 0
	for idx := 0; idx < totalLen; idx++ {
		key := p.makeKey(logArray[idx])
		if p.Algorithm.IsAllowed(key) {
			if idx != nextIdx {
				logArray[nextIdx] = logArray[idx]
			}
			nextIdx++
		} else {
			p.limitMetric.Add(1)
		}
	}
	logArray = logArray[:nextIdx]
	return logArray
}

func (p *ProcessorRateLimit) makeKey(log *protocol.Log) string {
	if len(p.Fields) == 0 {
		return ""
	}

	// Fields is already sorted in Init. Start from len=0 so the joined key holds
	// exactly one segment per field (append below), instead of len(p.Fields)
	// leading empty segments that would prefix every key with "_" separators.
	values := make([]string, 0, len(p.Fields))
	for _, field := range p.Fields {
		value := ""
		for _, logContent := range log.Contents {
			if field == logContent.GetKey() {
				value = fmt.Sprintf("%v", logContent.GetValue())
				break
			}
		}
		values = append(values, value)
	}

	return strings.Join(values, "_")
}

func init() {
	pipeline.Processors[pluginType] = func() pipeline.Processor {
		return &ProcessorRateLimit{}
	}
}

// makeKeyV2 mirrors makeKey for the v2 models.Log representation, building the
// limit key from the configured fields held in log.GetIndices().
func (p *ProcessorRateLimit) makeKeyV2(log *models.Log) string {
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

// Process implements the v2 ProcessorV2 interface. Log events are rate limited
// via the shared token-bucket algorithm (dropping logs when their limit key is
// exhausted, mirroring the v1 semantics), while Metric/Span and any other
// non-Log events always pass through unchanged. The original event order is
// preserved.
func (p *ProcessorRateLimit) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	if in == nil {
		return
	}
	survivors := make([]models.PipelineEvent, 0, len(in.Events))
	for _, event := range in.Events {
		if event.GetType() == models.EventTypeLogging {
			key := p.makeKeyV2(event.(*models.Log))
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

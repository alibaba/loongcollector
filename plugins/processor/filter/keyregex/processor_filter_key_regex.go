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

package keyregex

import (
	"regexp"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

const pluginType = "processor_filter_key_regex"

type ProcessorKeyFilter struct {
	Include []string
	Exclude []string

	includeRegex []*regexp.Regexp
	excludeRegex []*regexp.Regexp
	filterMetric selfmonitor.CounterMetric
	context      pipeline.Context
}

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorKeyFilter) Init(context pipeline.Context) error {
	p.context = context
	if len(p.Include) > 0 {
		p.includeRegex = make([]*regexp.Regexp, len(p.Include))
		for key, val := range p.Include {
			reg, err := regexp.Compile(val)
			if err != nil {
				logger.Warning(p.context.GetRuntimeContext(), selfmonitor.FilterInitAlarm, "init include filter error, key", key, "regex", val, "error", err)
				return err
			}
			p.includeRegex[key] = reg
		}
	}
	if len(p.Exclude) > 0 {
		p.excludeRegex = make([]*regexp.Regexp, len(p.Exclude))
		for key, val := range p.Exclude {
			reg, err := regexp.Compile(val)
			if err != nil {
				logger.Warning(p.context.GetRuntimeContext(), selfmonitor.FilterInitAlarm, "init exclude filter error, key", key, "regex", val, "error", err)
				return err
			}
			p.excludeRegex[key] = reg
		}
	}

	metricsRecord := p.context.GetMetricRecord()
	p.filterMetric = selfmonitor.NewCounterMetricAndRegister(metricsRecord, selfmonitor.MetricPluginDiscardedEventsTotal)
	return nil
}

func (*ProcessorKeyFilter) Description() string {
	return "key regex filter for logtail, if key is unmatched, drop this log"
}

func init() {
	pipeline.Processors[pluginType] = func() pipeline.Processor {
		return &ProcessorKeyFilter{}
	}
}

// isLogMatch tests the log field names against the configured patterns.
func (p *ProcessorKeyFilter) isLogMatch(log *models.Log) bool {
	contents := log.GetIndices()
	if p.includeRegex != nil {
	ForBlock:
		for _, reg := range p.includeRegex {
			for key := range contents.Iterator() {
				if reg.MatchString(key) {
					continue ForBlock
				}
			}
			return false
		}
	}

	if p.excludeRegex != nil {
		for key := range contents.Iterator() {
			for _, reg := range p.excludeRegex {
				// if match, return false
				if reg.MatchString(key) {
					return false
				}
			}
		}
	}
	return true
}

// Process filters logs by field name and passes other events through in order.
func (p *ProcessorKeyFilter) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	if in == nil {
		return
	}
	survivors := make([]models.PipelineEvent, 0, len(in.Events))
	for _, event := range in.Events {
		if event.GetType() == models.EventTypeLogging {
			if p.isLogMatch(event.(*models.Log)) {
				survivors = append(survivors, event)
			} else {
				p.filterMetric.Add(1)
			}
			continue
		}
		// Metric/Span/other events are never dropped.
		survivors = append(survivors, event)
	}
	context.Collector().Collect(in.Group, survivors...)
}

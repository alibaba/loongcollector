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

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const pluginType = "processor_filter_key_regex"

type ProcessorKeyFilter struct {
	Include []string
	Exclude []string

	includeRegex []*regexp.Regexp
	excludeRegex []*regexp.Regexp
	filterMetric pipeline.CounterMetric
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
				logger.Warning(p.context.GetRuntimeContext(), "FILTER_INIT_ALARM", "init include filter error, key", key, "regex", val, "error", err)
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
				logger.Warning(p.context.GetRuntimeContext(), "FILTER_INIT_ALARM", "init exclude filter error, key", key, "regex", val, "error", err)
				return err
			}
			p.excludeRegex[key] = reg
		}
	}

	metricsRecord := p.context.GetMetricRecord()
	p.filterMetric = helper.NewCounterMetricAndRegister(metricsRecord, helper.MetricPluginDiscardedEventsTotal)
	return nil
}

func (*ProcessorKeyFilter) Description() string {
	return "key regex filter for logtail, if key is unmatched, drop this log"
}

func (p *ProcessorKeyFilter) IsLogMatch(log *protocol.Log) bool {
	if p.includeRegex != nil {
	ForBlock:
		for _, reg := range p.includeRegex {
			for _, cont := range log.Contents {
				if reg.MatchString(cont.Key) {
					continue ForBlock
				}
			}
			return false
		}
	}

	if p.excludeRegex != nil {
		for _, cont := range log.Contents {
			for _, reg := range p.excludeRegex {
				// if match, return false
				if reg.MatchString(cont.Key) {
					return false
				}
			}
		}
	}
	return true
}

func (p *ProcessorKeyFilter) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	totalLen := len(logArray)
	nextIdx := 0
	for idx := 0; idx < totalLen; idx++ {
		if p.IsLogMatch(logArray[idx]) {
			if idx != nextIdx {
				logArray[nextIdx] = logArray[idx]
			}
			nextIdx++
		} else {
			p.filterMetric.Add(1)
		}
	}
	logArray = logArray[:nextIdx]
	return logArray
}

func init() {
	pipeline.Processors[pluginType] = func() pipeline.Processor {
		return &ProcessorKeyFilter{}
	}
}

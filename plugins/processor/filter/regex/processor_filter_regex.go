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

package regex

import (
	"fmt"
	"regexp"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const pluginType = "processor_filter_regex"

// ProcessorRegexFilter is a processor plugin to filter log according to the value of field.
// Include/Exclude are maps from string to string, key is used to search field in log, value
// is a regex to match the value of searched field.
// A log will be reserved only when its fields match all rules in Include and do not match
// any rule in Exclude.
type ProcessorRegexFilter struct {
	Include map[string]string
	Exclude map[string]string

	includeRegex    map[string]*regexp.Regexp
	excludeRegex    map[string]*regexp.Regexp
	filterMetric    pipeline.CounterMetric
	processedMetric pipeline.CounterMetric
	context         pipeline.Context
}

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorRegexFilter) Init(context pipeline.Context) error {
	p.context = context
	if p.Include != nil {
		p.includeRegex = make(map[string]*regexp.Regexp)
		for key, val := range p.Include {
			reg, err := regexp.Compile(val)
			if err != nil {
				logger.Warning(p.context.GetRuntimeContext(), "FILTER_INIT_ALARM", "init include filter error, key", key, "regex", val, "error", err)
				return err
			}
			p.includeRegex[key] = reg
		}
	}
	if p.Exclude != nil {
		p.excludeRegex = make(map[string]*regexp.Regexp)
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
	p.filterMetric = helper.NewCounterMetricAndRegister(metricsRecord, fmt.Sprintf("%v_filtered", pluginType))
	p.processedMetric = helper.NewCounterMetricAndRegister(metricsRecord, fmt.Sprintf("%v_processed", pluginType))
	return nil
}

func (*ProcessorRegexFilter) Description() string {
	return "regex filter for logtail"
}

func (p *ProcessorRegexFilter) IsLogMatch(log *protocol.Log) bool {
	if p.includeRegex != nil {
		includeCount := 0
		for _, cont := range log.Contents {
			if reg, ok := p.includeRegex[cont.Key]; ok {
				if reg.MatchString(cont.Value) {
					includeCount++
				} else {
					// not match
					// logger.Info("include not match", "kv", cont.GetKey(), cont.GetValue(), "match result", matchRst)
					return false
				}
			}
		}
		// not match all
		if includeCount < len(p.includeRegex) {
			// logger.Info("include count not match", includeCount)
			return false
		}
	}

	if p.excludeRegex != nil {
		for _, cont := range log.Contents {
			if reg, ok := p.excludeRegex[cont.Key]; ok {
				if reg.MatchString(cont.Value) {
					// if match, return false
					// logger.Info("exclude not match", *cont)
					return false
				}
			}
		}
	}

	return true
}

func (p *ProcessorRegexFilter) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
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
		p.processedMetric.Add(1)
	}
	logArray = logArray[:nextIdx]
	return logArray
}

func init() {
	pipeline.Processors[pluginType] = func() pipeline.Processor {
		return &ProcessorRegexFilter{}
	}
}

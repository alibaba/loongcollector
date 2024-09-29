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

package pickkey

import (
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const pluginType = "processor_pick_key"

// ProcessorPickKey is picker to select or drop specific keys in LogContents
type ProcessorPickKey struct {
	Include []string
	Exclude []string

	includeMap map[string]struct{}
	excludeMap map[string]struct{}
	includeLen int
	excludeLen int

	filterMetric pipeline.CounterMetric
	context      pipeline.Context
}

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorPickKey) Init(context pipeline.Context) error {
	p.context = context
	metricsRecord := p.context.GetMetricRecord()
	p.filterMetric = helper.NewCounterMetricAndRegister(metricsRecord, helper.MetricPluginDiscardedEventsTotal)

	if len(p.Include) > 0 {
		p.includeMap = make(map[string]struct{})
		for _, key := range p.Include {
			p.includeMap[key] = struct{}{}
		}
	}
	p.includeLen = len(p.Include)

	if len(p.Exclude) > 0 {
		p.excludeMap = make(map[string]struct{})
		for _, key := range p.Exclude {
			p.excludeMap[key] = struct{}{}
		}
	}
	p.excludeLen = len(p.Exclude)
	return nil
}

func (*ProcessorPickKey) Description() string {
	return "regex filter for logtail"
}

func (p *ProcessorPickKey) process(log *protocol.Log) bool {
	beginLen := len(log.Contents)
	if p.includeLen > 0 {
		tmpContent := log.Contents
		log.Contents = make([]*protocol.Log_Content, 0, p.includeLen)
		for _, cont := range tmpContent {
			if _, ok := p.includeMap[cont.Key]; ok {
				log.Contents = append(log.Contents, cont)
			}
		}
	}

	if p.excludeLen > 0 {
		tmpContent := log.Contents
		deltaLen := len(tmpContent) - p.excludeLen
		if deltaLen <= 4 {
			deltaLen = 4
		}
		log.Contents = make([]*protocol.Log_Content, 0, deltaLen)
		for _, cont := range tmpContent {
			if _, ok := p.excludeMap[cont.Key]; !ok {
				log.Contents = append(log.Contents, cont)
			}
		}
	}

	p.filterMetric.Add(int64(beginLen - len(log.Contents)))

	return len(log.Contents) != 0
}

func (p *ProcessorPickKey) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	totalLen := len(logArray)
	nextIdx := 0
	for idx := 0; idx < totalLen; idx++ {
		if p.process(logArray[idx]) {
			if nextIdx != idx {
				logArray[nextIdx] = logArray[idx]
			}
			nextIdx++
		}
	}
	logArray = logArray[:nextIdx]
	return logArray
}

func init() {
	pipeline.Processors[pluginType] = func() pipeline.Processor {
		return &ProcessorPickKey{}
	}
}

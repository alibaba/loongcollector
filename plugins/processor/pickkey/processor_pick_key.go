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
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
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

	filterMetric selfmonitor.CounterMetric
	context      pipeline.Context
}

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorPickKey) Init(context pipeline.Context) error {
	p.context = context
	metricsRecord := p.context.GetMetricRecord()
	p.filterMetric = selfmonitor.NewCounterMetricAndRegister(metricsRecord, selfmonitor.MetricPluginDiscardedEventsTotal)

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

// Process implements the v2 ProcessorV2 interface: it filters the fields of
// each Log event, keeping only Include keys and dropping Exclude keys (same
// field-level semantics as the v1 path). A Log whose fields are all removed is
// dropped, matching v1 process's return value. Metric/Span events pass through
// unchanged.
func (p *ProcessorPickKey) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	if in == nil {
		return
	}
	events := make([]models.PipelineEvent, 0, len(in.Events))
	for _, event := range in.Events {
		if event.GetType() != models.EventTypeLogging {
			// Metric/Span events pass through unchanged.
			events = append(events, event)
			continue
		}
		if p.processLogEvent(event.(*models.Log)) {
			events = append(events, event)
		}
	}
	context.Collector().Collect(in.Group, events...)
}

// processLogEvent filters the log contents per Include/Exclude and reports
// whether the log should be kept (still has at least one field), matching the
// v1 process return value.
func (p *ProcessorPickKey) processLogEvent(log *models.Log) bool {
	contents := log.GetIndices()
	beginLen := contents.Len()
	if p.includeLen > 0 {
		for key := range contents.Iterator() {
			if _, ok := p.includeMap[key]; !ok {
				contents.Delete(key)
			}
		}
	}
	if p.excludeLen > 0 {
		for _, key := range p.Exclude {
			contents.Delete(key)
		}
	}
	p.filterMetric.Add(int64(beginLen - contents.Len()))
	return contents.Len() != 0
}

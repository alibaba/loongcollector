// Copyright 2024 iLogtail Authors
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

package pluginmanager

import "github.com/alibaba/ilogtail/pkg/selfmonitor"

type pipelineInputMetrics struct {
	events selfmonitor.CounterMetric
	groups selfmonitor.CounterMetric
	bytes  selfmonitor.CounterMetric
}

func (m *pipelineInputMetrics) record(groups, events, bytes int64) {
	m.events.Add(events)
	m.groups.Add(groups)
	m.bytes.Add(bytes)
}

func (p *pluginRunner) initPipelineInputMetrics() {
	ctx := p.LogstoreConfig.Context
	record := ctx.RegisterLogstoreConfigMetricRecord([]selfmonitor.LabelPair{
		{Key: selfmonitor.MetricLabelKeyMetricCategory, Value: selfmonitor.MetricLabelValueMetricCategoryPipeline},
		{Key: selfmonitor.MetricLabelKeyProject, Value: ctx.GetProject()},
		{Key: selfmonitor.MetricLabelKeyLogstore, Value: ctx.GetLogstore()},
		{Key: selfmonitor.MetricLabelKeyPipelineName, Value: ctx.GetConfigName()},
	})
	// Native inputs have already been counted by CollectionPipeline::Process.
	if p.IsWithInputPlugin() {
		p.processorInputMetrics = &pipelineInputMetrics{
			events: selfmonitor.NewCounterMetricAndRegister(record, selfmonitor.MetricPipelineProcessorInEventsTotal),
			groups: selfmonitor.NewCounterMetricAndRegister(record, selfmonitor.MetricPipelineProcessorInEventGroupsTotal),
			bytes:  selfmonitor.NewCounterMetricAndRegister(record, selfmonitor.MetricPipelineProcessorInSizeBytes),
		}
	}

	// The first Go flusher sees each batch once, including when the runner
	// drains its retained groups on exit. Later flushers only count their own inputs.
	p.FlusherPlugins[0].pipelineInputMetrics = &pipelineInputMetrics{
		events: selfmonitor.NewCounterMetricAndRegister(record, selfmonitor.MetricPipelineFlusherInEventsTotal),
		groups: selfmonitor.NewCounterMetricAndRegister(record, selfmonitor.MetricPipelineFlusherInEventGroupsTotal),
		bytes:  selfmonitor.NewCounterMetricAndRegister(record, selfmonitor.MetricPipelineFlusherInSizeBytes),
	}
}

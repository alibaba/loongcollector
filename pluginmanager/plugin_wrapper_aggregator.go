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

import (
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

// AggregatorPluginWrapper wrappers Aggregator.
// It implements LogGroupQueue interface, and is passed to associated Aggregator.
// Aggregator uses Add function to pass log groups to wrapper, and then wrapper
// passes log groups to associated LogstoreConfig through channel the output queue.
// In fact, the output queue == (associated) LogstoreConfig.the output queue.
type AggregatorPluginWrapper struct {
	AggregatorWrapper
	Aggregator pipeline.AggregatorV2

	totalDelayTimeMs selfmonitor.CounterMetric
}

func (wrapper *AggregatorPluginWrapper) Init(pluginMeta *pipeline.PluginMeta) error {
	wrapper.InitMetricRecord(pluginMeta)
	wrapper.totalDelayTimeMs = selfmonitor.NewCounterMetricAndRegister(wrapper.MetricRecord, selfmonitor.MetricPluginTotalDelayMs)

	interval, err := wrapper.Aggregator.Init(wrapper.Config.Context, wrapper)
	if err != nil {
		logger.Error(wrapper.Config.Context.GetRuntimeContext(), selfmonitor.AggregatorInitErrorAlarm, "Aggregator failed to initialize", wrapper.Aggregator.Description(), "error", err)
		return err
	}
	if interval == 0 {
		interval = wrapper.Config.GlobalConfig.AggregatIntervalMs
	}
	wrapper.Interval = time.Millisecond * time.Duration(interval)
	return nil
}

func (wrapper *AggregatorPluginWrapper) Record(events *models.PipelineGroupEvents, context pipeline.PipelineContext) error {
	startTime := time.Now()

	err := wrapper.Aggregator.Record(events, context)
	if err == nil {
		wrapper.outEventsTotal.Add(events.GetEventCount())
		wrapper.outEventGroupsTotal.Add(1)
		wrapper.outSizeBytes.Add(events.GetSize())
	}
	wrapper.totalDelayTimeMs.Add(time.Since(startTime).Milliseconds())
	return err
}

func (wrapper *AggregatorPluginWrapper) GetResult(context pipeline.PipelineContext) error {
	return wrapper.Aggregator.GetResult(context)
}

func (wrapper *AggregatorPluginWrapper) Add(loggroup *protocol.LogGroup) error {
	return nil
}

func (wrapper *AggregatorPluginWrapper) AddWithWait(loggroup *protocol.LogGroup, duration time.Duration) error {
	return nil
}

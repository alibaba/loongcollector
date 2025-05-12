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

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

/*---------------------
Plugin Input
The input plugin is used for reading data.
---------------------*/

type InputWrapper struct {
	pipeline.PluginContext
	Config   *LogstoreConfig
	Tags     map[string]string
	Interval time.Duration

	outEventsTotal      selfmonitor.CounterMetric
	outEventGroupsTotal selfmonitor.CounterMetric
	outSizeBytes        selfmonitor.CounterMetric
}

func (wrapper *InputWrapper) InitMetricRecord(pluginMeta *pipeline.PluginMeta) {
	labels := pipeline.GetPluginCommonLabels(wrapper.Config.Context, pluginMeta)
	wrapper.MetricRecord = wrapper.Config.Context.RegisterMetricRecord(labels)

	wrapper.outEventsTotal = selfmonitor.NewCounterMetricAndRegister(wrapper.MetricRecord, selfmonitor.MetricPluginOutEventsTotal)
	wrapper.outEventGroupsTotal = selfmonitor.NewCounterMetricAndRegister(wrapper.MetricRecord, selfmonitor.MetricPluginOutEventGroupsTotal)
	wrapper.outSizeBytes = selfmonitor.NewCounterMetricAndRegister(wrapper.MetricRecord, selfmonitor.MetricPluginOutSizeBytes)
}

// The service plugin is an input plugin used for passively receiving data.
type ServiceWrapper struct {
	InputWrapper
}

// metric plugin is an input plugin used for actively pulling data.
type MetricWrapper struct {
	InputWrapper
}

/*---------------------
Plugin Processor
The processor plugin is used for reading data.
---------------------*/

type ProcessorWrapper struct {
	pipeline.PluginContext
	Config *LogstoreConfig

	inEventsTotal      selfmonitor.CounterMetric
	inSizeBytes        selfmonitor.CounterMetric
	outEventsTotal     selfmonitor.CounterMetric
	outSizeBytes       selfmonitor.CounterMetric
	totalProcessTimeMs selfmonitor.CounterMetric
}

func (wrapper *ProcessorWrapper) InitMetricRecord(pluginMeta *pipeline.PluginMeta) {
	labels := pipeline.GetPluginCommonLabels(wrapper.Config.Context, pluginMeta)
	wrapper.MetricRecord = wrapper.Config.Context.RegisterMetricRecord(labels)

	wrapper.inEventsTotal = selfmonitor.NewCounterMetricAndRegister(wrapper.MetricRecord, selfmonitor.MetricPluginInEventsTotal)
	wrapper.inSizeBytes = selfmonitor.NewCounterMetricAndRegister(wrapper.MetricRecord, selfmonitor.MetricPluginInSizeBytes)
	wrapper.outEventsTotal = selfmonitor.NewCounterMetricAndRegister(wrapper.MetricRecord, selfmonitor.MetricPluginOutEventsTotal)
	wrapper.outSizeBytes = selfmonitor.NewCounterMetricAndRegister(wrapper.MetricRecord, selfmonitor.MetricPluginOutSizeBytes)
	wrapper.totalProcessTimeMs = selfmonitor.NewCounterMetricAndRegister(wrapper.MetricRecord, selfmonitor.MetricPluginTotalProcessTimeMs)
}

/*---------------------
Plugin Aggregator
The aggregator plugin is used for aggregating data.
---------------------*/

type AggregatorWrapper struct {
	pipeline.PluginContext
	Config   *LogstoreConfig
	Interval time.Duration

	outEventsTotal      selfmonitor.CounterMetric
	outEventGroupsTotal selfmonitor.CounterMetric
	outSizeBytes        selfmonitor.CounterMetric
}

func (wrapper *AggregatorWrapper) InitMetricRecord(pluginMeta *pipeline.PluginMeta) {
	labels := pipeline.GetPluginCommonLabels(wrapper.Config.Context, pluginMeta)
	wrapper.MetricRecord = wrapper.Config.Context.RegisterMetricRecord(labels)

	wrapper.outEventsTotal = selfmonitor.NewCounterMetricAndRegister(wrapper.MetricRecord, selfmonitor.MetricPluginOutEventsTotal)
	wrapper.outEventGroupsTotal = selfmonitor.NewCounterMetricAndRegister(wrapper.MetricRecord, selfmonitor.MetricPluginOutEventGroupsTotal)
	wrapper.outSizeBytes = selfmonitor.NewCounterMetricAndRegister(wrapper.MetricRecord, selfmonitor.MetricPluginOutSizeBytes)
}

/*---------------------
Plugin Flusher
The flusher plugin is used for sending data.
---------------------*/

type FlusherWrapperInterface interface {
	Init(pluginMeta *pipeline.PluginMeta) error
	IsReady(projectName string, logstoreName string, logstoreKey int64) bool
}

type FlusherWrapper struct {
	pipeline.PluginContext
	Config   *LogstoreConfig
	Interval time.Duration

	inEventsTotal      selfmonitor.CounterMetric
	inEventGroupsTotal selfmonitor.CounterMetric
	inSizeBytes        selfmonitor.CounterMetric
	totalDelayTimeMs   selfmonitor.CounterMetric
}

func (wrapper *FlusherWrapper) InitMetricRecord(pluginMeta *pipeline.PluginMeta) {
	labels := pipeline.GetPluginCommonLabels(wrapper.Config.Context, pluginMeta)
	wrapper.MetricRecord = wrapper.Config.Context.RegisterMetricRecord(labels)

	wrapper.inEventsTotal = selfmonitor.NewCounterMetricAndRegister(wrapper.MetricRecord, selfmonitor.MetricPluginInEventsTotal)
	wrapper.inEventGroupsTotal = selfmonitor.NewCounterMetricAndRegister(wrapper.MetricRecord, selfmonitor.MetricPluginInEventGroupsTotal)
	wrapper.inSizeBytes = selfmonitor.NewCounterMetricAndRegister(wrapper.MetricRecord, selfmonitor.MetricPluginInSizeBytes)
	wrapper.totalDelayTimeMs = selfmonitor.NewCounterMetricAndRegister(wrapper.MetricRecord, selfmonitor.MetricPluginTotalDelayMs)
}

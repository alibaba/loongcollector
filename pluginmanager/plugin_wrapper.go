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

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/pipeline"
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

	outEventsTotal      pipeline.CounterMetric
	outEventGroupsTotal pipeline.CounterMetric
	outSizeBytes        pipeline.CounterMetric
}

func (wrapper *InputWrapper) InitMetricRecord(pluginMeta *pipeline.PluginMeta) {
	labels := helper.GetCommonLabels(wrapper.Config.Context, pluginMeta)
	wrapper.MetricRecord = wrapper.Config.Context.RegisterMetricRecord(labels)

	wrapper.outEventsTotal = helper.NewCounterMetricAndRegister(wrapper.MetricRecord, helper.MetricPluginOutEventsTotal)
	wrapper.outEventGroupsTotal = helper.NewCounterMetricAndRegister(wrapper.MetricRecord, helper.MetricPluginOutEventGroupsTotal)
	wrapper.outSizeBytes = helper.NewCounterMetricAndRegister(wrapper.MetricRecord, helper.MetricPluginOutSizeBytes)
}

// The service plugin is an input plugin used for passively receiving data.
type ServiceWrapper struct {
	InputWrapper
}

// metric plugin is an input plugin used for actively pulling data.
type MetricWrapper struct {
	InputWrapper
	LatencyMetric pipeline.LatencyMetric
}

/*---------------------
Plugin Processor
The processor plugin is used for reading data.
---------------------*/

type ProcessorWrapper struct {
	pipeline.PluginContext
	Config *LogstoreConfig

	inEventsTotal      pipeline.CounterMetric
	inSizeBytes        pipeline.CounterMetric
	outEventsTotal     pipeline.CounterMetric
	outSizeBytes       pipeline.CounterMetric
	totalProcessTimeMs pipeline.CounterMetric
}

func (wrapper *ProcessorWrapper) InitMetricRecord(pluginMeta *pipeline.PluginMeta) {
	labels := helper.GetCommonLabels(wrapper.Config.Context, pluginMeta)
	wrapper.MetricRecord = wrapper.Config.Context.RegisterMetricRecord(labels)

	wrapper.inEventsTotal = helper.NewCounterMetricAndRegister(wrapper.MetricRecord, helper.MetricPluginInEventsTotal)
	wrapper.inSizeBytes = helper.NewCounterMetricAndRegister(wrapper.MetricRecord, helper.MetricPluginInSizeBytes)
	wrapper.outEventsTotal = helper.NewCounterMetricAndRegister(wrapper.MetricRecord, helper.MetricPluginOutEventsTotal)
	wrapper.outSizeBytes = helper.NewCounterMetricAndRegister(wrapper.MetricRecord, helper.MetricPluginOutSizeBytes)
	wrapper.totalProcessTimeMs = helper.NewCounterMetricAndRegister(wrapper.MetricRecord, helper.MetricPluginTotalProcessTimeMs)
}

/*---------------------
Plugin Aggregator
The aggregator plugin is used for aggregating data.
---------------------*/

type AggregatorWrapper struct {
	pipeline.PluginContext
	Config   *LogstoreConfig
	Interval time.Duration

	outEventsTotal      pipeline.CounterMetric
	outEventGroupsTotal pipeline.CounterMetric
	outSizeBytes        pipeline.CounterMetric
}

func (wrapper *AggregatorWrapper) InitMetricRecord(pluginMeta *pipeline.PluginMeta) {
	labels := helper.GetCommonLabels(wrapper.Config.Context, pluginMeta)
	wrapper.MetricRecord = wrapper.Config.Context.RegisterMetricRecord(labels)

	wrapper.outEventsTotal = helper.NewCounterMetricAndRegister(wrapper.MetricRecord, helper.MetricPluginOutEventsTotal)
	wrapper.outEventGroupsTotal = helper.NewCounterMetricAndRegister(wrapper.MetricRecord, helper.MetricPluginOutEventGroupsTotal)
	wrapper.outSizeBytes = helper.NewCounterMetricAndRegister(wrapper.MetricRecord, helper.MetricPluginOutSizeBytes)
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

	inEventsTotal      pipeline.CounterMetric
	inEventGroupsTotal pipeline.CounterMetric
	inSizeBytes        pipeline.CounterMetric
	totalDelayTimeMs   pipeline.CounterMetric
}

func (wrapper *FlusherWrapper) InitMetricRecord(pluginMeta *pipeline.PluginMeta) {
	labels := helper.GetCommonLabels(wrapper.Config.Context, pluginMeta)
	wrapper.MetricRecord = wrapper.Config.Context.RegisterMetricRecord(labels)

	wrapper.inEventsTotal = helper.NewCounterMetricAndRegister(wrapper.MetricRecord, helper.MetricPluginInEventsTotal)
	wrapper.inEventGroupsTotal = helper.NewCounterMetricAndRegister(wrapper.MetricRecord, helper.MetricPluginInEventGroupsTotal)
	wrapper.inSizeBytes = helper.NewCounterMetricAndRegister(wrapper.MetricRecord, helper.MetricPluginInSizeBytes)
	wrapper.totalDelayTimeMs = helper.NewCounterMetricAndRegister(wrapper.MetricRecord, helper.MetricPluginTotalDelayMs)
}

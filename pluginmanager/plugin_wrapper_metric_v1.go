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

package pluginmanager

import (
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"

	"time"
)

type MetricWrapperV1 struct {
	MetricWrapper
	LogsChan chan *pipeline.LogWithContext
	Input    pipeline.MetricInputV1
}

func (wrapper *MetricWrapperV1) Init(pluginMeta *pipeline.PluginMeta, inputInterval int) error {
	wrapper.InitMetricRecord(pluginMeta)

	interval, err := wrapper.Input.Init(wrapper.Config.Context)
	if err != nil {
		return err
	}
	if interval == 0 {
		interval = inputInterval
	}
	wrapper.Interval = time.Duration(interval) * time.Millisecond
	return nil
}

func (wrapper *MetricWrapperV1) AddData(tags map[string]string, fields map[string]string, t ...time.Time) {
	wrapper.AddDataWithContext(tags, fields, nil, t...)
}

func (wrapper *MetricWrapperV1) AddDataArray(tags map[string]string,
	columns []string,
	values []string,
	t ...time.Time) {
	wrapper.AddDataArrayWithContext(tags, columns, values, nil, t...)
}

func (wrapper *MetricWrapperV1) AddRawLog(log *protocol.Log) {
	wrapper.AddRawLogWithContext(log, nil)
}

func (wrapper *MetricWrapperV1) AddDataWithContext(tags map[string]string, fields map[string]string, ctx map[string]interface{}, t ...time.Time) {
	var logTime time.Time
	if len(t) == 0 {
		logTime = time.Now()
	} else {
		logTime = t[0]
	}
	slsLog, _ := helper.CreateLog(logTime, len(t) != 0, wrapper.Tags, tags, fields)
	wrapper.outEventsTotal.Add(1)
	wrapper.outEventGroupsTotal.Add(1)
	wrapper.outSizeBytes.Add(int64(slsLog.Size()))
	wrapper.LogsChan <- &pipeline.LogWithContext{Log: slsLog, Context: ctx}
}

func (wrapper *MetricWrapperV1) AddDataArrayWithContext(tags map[string]string,
	columns []string,
	values []string,
	ctx map[string]interface{},
	t ...time.Time) {
	var logTime time.Time
	if len(t) == 0 {
		logTime = time.Now()
	} else {
		logTime = t[0]
	}
	slsLog, _ := helper.CreateLogByArray(logTime, len(t) != 0, wrapper.Tags, tags, columns, values)
	wrapper.outEventsTotal.Add(1)
	wrapper.outEventGroupsTotal.Add(1)
	wrapper.outSizeBytes.Add(int64(slsLog.Size()))
	wrapper.LogsChan <- &pipeline.LogWithContext{Log: slsLog, Context: ctx}
}

func (wrapper *MetricWrapperV1) AddRawLogWithContext(log *protocol.Log, ctx map[string]interface{}) {
	wrapper.outEventsTotal.Add(1)
	wrapper.outEventGroupsTotal.Add(1)
	wrapper.outSizeBytes.Add(int64(log.Size()))
	wrapper.LogsChan <- &pipeline.LogWithContext{Log: log, Context: ctx}
}

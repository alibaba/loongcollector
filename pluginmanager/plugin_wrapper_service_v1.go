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
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"

	"time"
)

type ServiceWrapperV1 struct {
	ServiceWrapper
	LogsChan chan *pipeline.LogWithContext
	Input    pipeline.ServiceInputV1
}

func (p *ServiceWrapperV1) Init(pluginMeta *pipeline.PluginMeta) error {
	p.InitMetricRecord(pluginMeta)

	_, err := p.Input.Init(p.Config.Context)
	return err
}

func (p *ServiceWrapperV1) Run(cc *pipeline.AsyncControl) {
	logger.Info(p.Config.Context.GetRuntimeContext(), "start run service", p.Input)

	go func() {
		defer panicRecover(p.Input.Description())
		err := p.Input.Start(p)
		if err != nil {
			logger.Error(p.Config.Context.GetRuntimeContext(), "PLUGIN_ALARM", "start service error, err", err)
		}
		logger.Info(p.Config.Context.GetRuntimeContext(), "service done", p.Input.Description())
	}()

}

func (p *ServiceWrapperV1) Stop() error {
	err := p.Input.Stop()
	if err != nil {
		logger.Error(p.Config.Context.GetRuntimeContext(), "PLUGIN_ALARM", "stop service error, err", err)
	}
	return err
}

func (p *ServiceWrapperV1) AddData(tags map[string]string, fields map[string]string, t ...time.Time) {
	p.AddDataWithContext(tags, fields, nil, t...)
}

func (p *ServiceWrapperV1) AddDataArray(tags map[string]string,
	columns []string,
	values []string,
	t ...time.Time) {
	p.AddDataArrayWithContext(tags, columns, values, nil, t...)
}

func (p *ServiceWrapperV1) AddRawLog(log *protocol.Log) {
	p.AddRawLogWithContext(log, nil)
}

func (p *ServiceWrapperV1) AddDataWithContext(tags map[string]string, fields map[string]string, ctx map[string]interface{}, t ...time.Time) {
	var logTime time.Time
	if len(t) == 0 {
		logTime = time.Now()
	} else {
		logTime = t[0]
	}
	slsLog, _ := helper.CreateLog(logTime, len(t) != 0, p.Tags, tags, fields)
	p.inputRecordsTotal.Add(1)
	p.inputRecordsSizeBytes.Add(int64(slsLog.Size()))
	p.LogsChan <- &pipeline.LogWithContext{Log: slsLog, Context: ctx}
}

func (p *ServiceWrapperV1) AddDataArrayWithContext(tags map[string]string,
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
	slsLog, _ := helper.CreateLogByArray(logTime, len(t) != 0, p.Tags, tags, columns, values)
	p.inputRecordsTotal.Add(1)
	p.inputRecordsSizeBytes.Add(int64(slsLog.Size()))
	p.LogsChan <- &pipeline.LogWithContext{Log: slsLog, Context: ctx}
}

func (p *ServiceWrapperV1) AddRawLogWithContext(log *protocol.Log, ctx map[string]interface{}) {
	p.inputRecordsTotal.Add(1)
	p.inputRecordsSizeBytes.Add(int64(log.Size()))
	p.LogsChan <- &pipeline.LogWithContext{Log: log, Context: ctx}
}

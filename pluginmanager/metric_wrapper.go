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
	"github.com/alibaba/ilogtail/pkg/util"

	"time"
)

type MetricWrapper struct {
	Input    pipeline.MetricInputV1
	Config   *LogstoreConfig
	Tags     map[string]string
	Interval time.Duration

	LogsChan      chan *pipeline.LogWithContext
	LatencyMetric pipeline.LatencyMetric
}

func (p *MetricWrapper) Run(control *pipeline.AsyncControl) {
	logger.Info(p.Config.Context.GetRuntimeContext(), "start run metric ", p.Input.Description())
	defer panicRecover(p.Input.Description())
	for {
		exitFlag := util.RandomSleep(p.Interval, 0.1, control.CancelToken())
		startTime := time.Now()
		err := p.Input.Collect(p)
		p.LatencyMetric.Observe(float64(time.Since(startTime)))
		if err != nil {
			logger.Error(p.Config.Context.GetRuntimeContext(), "INPUT_COLLECT_ALARM", "error", err)
		}
		if exitFlag {
			return
		}
	}
}

func (p *MetricWrapper) AddData(tags map[string]string, fields map[string]string, t ...time.Time) {
	p.AddDataWithContext(tags, fields, nil, t...)
}

func (p *MetricWrapper) AddDataArray(tags map[string]string,
	columns []string,
	values []string,
	t ...time.Time) {
	p.AddDataArrayWithContext(tags, columns, values, nil, t...)
}

func (p *MetricWrapper) AddRawLog(log *protocol.Log) {
	p.AddRawLogWithContext(log, nil)
}

func (p *MetricWrapper) AddDataWithContext(tags map[string]string, fields map[string]string, ctx map[string]interface{}, t ...time.Time) {
	var logTime time.Time
	if len(t) == 0 {
		logTime = time.Now()
	} else {
		logTime = t[0]
	}
	slsLog, _ := helper.CreateLog(logTime, len(t) != 0, p.Tags, tags, fields)
	p.LogsChan <- &pipeline.LogWithContext{Log: slsLog, Context: ctx}
}

func (p *MetricWrapper) AddDataArrayWithContext(tags map[string]string,
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
	p.LogsChan <- &pipeline.LogWithContext{Log: slsLog, Context: ctx}
}

func (p *MetricWrapper) AddRawLogWithContext(log *protocol.Log, ctx map[string]interface{}) {
	p.LogsChan <- &pipeline.LogWithContext{Log: log, Context: ctx}
}

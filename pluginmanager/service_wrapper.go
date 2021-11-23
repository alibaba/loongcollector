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
	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"

	"time"
)

type ServiceWrapper struct {
	Input    ilogtail.ServiceInput
	Config   *LogstoreConfig
	Tags     map[string]string
	Interval time.Duration

	LogsChan chan *protocol.Log
}

func (p *ServiceWrapper) Run() {
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

func (p *ServiceWrapper) Stop() error {
	err := p.Input.Stop()
	if err != nil {
		logger.Error(p.Config.Context.GetRuntimeContext(), "PLUGIN_ALARM", "stop service error, err", err)
	}
	return err
}

func (p *ServiceWrapper) AddData(tags map[string]string, fields map[string]string, t ...time.Time) {
	var logTime time.Time
	if len(t) == 0 {
		logTime = time.Now()
	} else {
		logTime = t[0]
	}
	slsLog, _ := util.CreateLog(logTime, p.Tags, tags, fields)
	p.LogsChan <- slsLog
}

func (p *ServiceWrapper) AddDataArray(tags map[string]string,
	columns []string,
	values []string,
	t ...time.Time) {
	var logTime time.Time
	if len(t) == 0 {
		logTime = time.Now()
	} else {
		logTime = t[0]
	}
	slsLog, _ := util.CreateLogByArray(logTime, p.Tags, tags, columns, values)
	p.LogsChan <- slsLog
}

func (p *ServiceWrapper) AddRawLog(log *protocol.Log) {
	p.LogsChan <- log
}

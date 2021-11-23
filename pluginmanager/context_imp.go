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
	"context"
	"encoding/json"
	"sync"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

type ContextImp struct {
	StringMetrics  map[string]ilogtail.StringMetric
	CounterMetrics map[string]ilogtail.CounterMetric
	LatencyMetrics map[string]ilogtail.LatencyMetric

	common      *pkg.LogtailContextMeta
	pluginNames string
	ctx         context.Context
}

var contextMutex sync.Mutex

func (p *ContextImp) GetRuntimeContext() context.Context {
	return p.ctx
}

func (p *ContextImp) GetConfigName() string {
	return p.common.GetConfigName()
}
func (p *ContextImp) GetProject() string {
	return p.common.GetProject()
}
func (p *ContextImp) GetLogstore() string {
	return p.common.GetLogStore()
}

func (p *ContextImp) AddPlugin(name string) {
	if len(p.pluginNames) != 0 {
		p.pluginNames += "," + name
	} else {
		p.pluginNames = name
	}
}

func (p *ContextImp) InitContext(project, logstore, configName string) {
	// bind metadata information.
	p.ctx, p.common = pkg.NewLogtailContextMeta(project, logstore, configName)
}

func (p *ContextImp) RegisterCounterMetric(metric ilogtail.CounterMetric) {
	contextMutex.Lock()
	defer contextMutex.Unlock()
	if p.CounterMetrics == nil {
		p.CounterMetrics = make(map[string]ilogtail.CounterMetric)
	}
	p.CounterMetrics[metric.Name()] = metric
}

func (p *ContextImp) RegisterStringMetric(metric ilogtail.StringMetric) {
	contextMutex.Lock()
	defer contextMutex.Unlock()
	if p.StringMetrics == nil {
		p.StringMetrics = make(map[string]ilogtail.StringMetric)
	}
	p.StringMetrics[metric.Name()] = metric
}

func (p *ContextImp) RegisterLatencyMetric(metric ilogtail.LatencyMetric) {
	contextMutex.Lock()
	defer contextMutex.Unlock()
	if p.LatencyMetrics == nil {
		p.LatencyMetrics = make(map[string]ilogtail.LatencyMetric)
	}
	p.LatencyMetrics[metric.Name()] = metric
}

func (p *ContextImp) MetricSerializeToPB(log *protocol.Log) {
	if log == nil {
		return
	}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "project", Value: p.GetProject()})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "config_name", Value: p.GetConfigName()})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "plugins", Value: p.pluginNames})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "category", Value: p.GetLogstore()})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "source_ip", Value: util.GetIPAddress()})
	contextMutex.Lock()
	defer contextMutex.Unlock()
	if p.CounterMetrics != nil {
		for _, value := range p.CounterMetrics {
			value.Serialize(log)
			value.Clear(0)
		}
	}
	if p.StringMetrics != nil {
		for _, value := range p.StringMetrics {
			value.Serialize(log)
			value.Set("")
		}
	}
	if p.LatencyMetrics != nil {
		for _, value := range p.LatencyMetrics {
			value.Serialize(log)
			value.Clear()
		}
	}
}

func (p *ContextImp) SaveCheckPoint(key string, value []byte) error {
	logger.Debug(p.ctx, "save checkpoint, key", key, "value", string(value))
	return CheckPointManager.SaveCheckpoint(p.GetConfigName(), key, value)
}

func (p *ContextImp) GetCheckPoint(key string) (value []byte, exist bool) {
	value, err := CheckPointManager.GetCheckpoint(p.GetConfigName(), key)
	logger.Debug(p.ctx, "get checkpoint, key", key, "value", string(value), "error", err)
	return value, value != nil
}

func (p *ContextImp) SaveCheckPointObject(key string, obj interface{}) error {
	val, err := json.Marshal(obj)
	if err != nil {
		return err
	}
	return p.SaveCheckPoint(key, val)
}

func (p *ContextImp) GetCheckPointObject(key string, obj interface{}) (exist bool) {
	val, ok := p.GetCheckPoint(key)
	if !ok {
		return false
	}
	err := json.Unmarshal(val, obj)
	if err != nil {
		logger.Error(p.ctx, "CHECKPOINT_INVALID_ALARM", "invalid checkpoint, key", key, "val", util.CutString(string(val), 1024), "error", err)
		return false
	}
	return true
}

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

package helper

import (
	"context"
	"encoding/json"
	"sync"

	"github.com/alibaba/ilogtail/pkg"
	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

type LocalContext struct {
	StringMetrics  map[string]pipeline.StringMetric
	CounterMetrics map[string]pipeline.CounterMetric
	LatencyMetrics map[string]pipeline.LatencyMetric
	AllCheckPoint  map[string][]byte

	ctx         context.Context
	pluginNames string
	common      *pkg.LogtailContextMeta
}

var contextMutex sync.Mutex

func (p *LocalContext) GetConfigName() string {
	return p.common.GetConfigName()
}
func (p *LocalContext) GetProject() string {
	return p.common.GetProject()
}
func (p *LocalContext) GetLogstore() string {
	return p.common.GetLogStore()
}

func (p *LocalContext) GetPipelineScopeConfig() *config.GlobalConfig {
	return &config.GlobalConfig{}
}

func (p *LocalContext) AddPlugin(name string) {
	if len(p.pluginNames) != 0 {
		p.pluginNames += "," + name
	} else {
		p.pluginNames = name
	}
}

func (p *LocalContext) InitContext(project, logstore, configName string) {
	// bind metadata information.
	p.ctx, p.common = pkg.NewLogtailContextMetaWithoutAlarm(project, logstore, configName)
	p.AllCheckPoint = make(map[string][]byte)
}

func (p *LocalContext) GetRuntimeContext() context.Context {
	return p.ctx
}

func (p *LocalContext) GetExtension(name string, cfg any) (pipeline.Extension, error) {
	return nil, nil
}

func (p *LocalContext) RegisterCounterMetric(metric pipeline.CounterMetric) {
	contextMutex.Lock()
	defer contextMutex.Unlock()
	if p.CounterMetrics == nil {
		p.CounterMetrics = make(map[string]pipeline.CounterMetric)
	}
	p.CounterMetrics[metric.Name()] = metric
}

func (p *LocalContext) RegisterStringMetric(metric pipeline.StringMetric) {
	contextMutex.Lock()
	defer contextMutex.Unlock()
	if p.StringMetrics == nil {
		p.StringMetrics = make(map[string]pipeline.StringMetric)
	}
	p.StringMetrics[metric.Name()] = metric
}

func (p *LocalContext) RegisterLatencyMetric(metric pipeline.LatencyMetric) {
	contextMutex.Lock()
	defer contextMutex.Unlock()
	if p.LatencyMetrics == nil {
		p.LatencyMetrics = make(map[string]pipeline.LatencyMetric)
	}
	p.LatencyMetrics[metric.Name()] = metric
}

func (p *LocalContext) MetricSerializeToPB(log *protocol.Log) {
	if log == nil {
		return
	}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "project", Value: p.GetProject()})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "config_name", Value: p.GetConfigName()})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "plugins", Value: p.pluginNames})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "category", Value: p.GetProject()})
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

func (p *LocalContext) SaveCheckPoint(key string, value []byte) error {
	logger.Debug(p.ctx, "save checkpoint, key", key, "value", string(value))
	p.AllCheckPoint[key] = value
	return nil
}

func (p *LocalContext) GetCheckPoint(key string) (value []byte, exist bool) {
	value, exist = p.AllCheckPoint[key]
	logger.Debug(p.ctx, "get checkpoint, key", key, "value", string(value))
	return value, exist
}

func (p *LocalContext) SaveCheckPointObject(key string, obj interface{}) error {
	val, err := json.Marshal(obj)
	if err != nil {
		logger.Debug(p.ctx, "CHECKPOINT_INVALID_ALARM", "save checkpoint error, invalid checkpoint, key", key, "val", util.CutString(string(val), 1024), "error", err)
		return err
	}
	return p.SaveCheckPoint(key, val)
}

func (p *LocalContext) GetCheckPointObject(key string, obj interface{}) (exist bool) {
	val, ok := p.GetCheckPoint(key)
	if !ok {
		return false
	}
	err := json.Unmarshal(val, obj)
	if err != nil {
		logger.Debug(p.ctx, "CHECKPOINT_INVALID_ALARM", "get checkpoint error, invalid checkpoint, key", key, "val", util.CutString(string(val), 1024), "error", err)
		return false
	}
	return true
}

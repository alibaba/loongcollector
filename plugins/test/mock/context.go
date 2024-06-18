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

package mock

import (
	"context"
	"encoding/json"
	"fmt"
	"sync"

	"github.com/alibaba/ilogtail/pkg"
	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

func NewEmptyContext(project, logstore, configName string) *EmptyContext {
	ctx, c := pkg.NewLogtailContextMetaWithoutAlarm(project, logstore, configName)
	return &EmptyContext{
		ctx:        ctx,
		common:     c,
		checkpoint: make(map[string][]byte),
	}
}

type EmptyContext struct {
	MetricsRecords []*pipeline.MetricsRecord

	common      *pkg.LogtailContextMeta
	ctx         context.Context
	checkpoint  map[string][]byte
	pluginNames string
}

var contextMutex sync.RWMutex

func (p *EmptyContext) GetConfigName() string {
	return p.common.GetConfigName()
}
func (p *EmptyContext) GetProject() string {
	return p.common.GetProject()
}
func (p *EmptyContext) GetLogstore() string {
	return p.common.GetLogStore()
}

func (p *EmptyContext) GetPipelineScopeConfig() *config.GlobalConfig {
	return &config.GlobalConfig{}
}

func (p *EmptyContext) AddPlugin(name string) {
	if len(p.pluginNames) != 0 {
		p.pluginNames += "," + name
	} else {
		p.pluginNames = name
	}
}

func (p *EmptyContext) InitContext(project, logstore, configName string) {
	p.ctx, p.common = pkg.NewLogtailContextMetaWithoutAlarm(project, logstore, configName)
}

func (p *EmptyContext) RegisterMetricRecord(labels []pipeline.LabelPair) *pipeline.MetricsRecord {
	contextMutex.Lock()
	defer contextMutex.Unlock()

	metricsRecord := &pipeline.MetricsRecord{Context: p}
	metricsRecord.Labels = append(metricsRecord.Labels, pipeline.Label{Key: "project", Value: p.GetProject()})
	metricsRecord.Labels = append(metricsRecord.Labels, pipeline.Label{Key: "config_name", Value: p.GetConfigName()})
	metricsRecord.Labels = append(metricsRecord.Labels, pipeline.Label{Key: "plugins", Value: p.pluginNames})
	metricsRecord.Labels = append(metricsRecord.Labels, pipeline.Label{Key: "category", Value: p.GetProject()})
	metricsRecord.Labels = append(metricsRecord.Labels, pipeline.Label{Key: "source_ip", Value: util.GetIPAddress()})
	for _, label := range labels {
		metricsRecord.Labels = append(metricsRecord.Labels, pipeline.Label{Key: label.Key, Value: label.Value})
	}

	p.MetricsRecords = append(p.MetricsRecords, metricsRecord)
	return metricsRecord
}

func (p *EmptyContext) GetMetricRecord() *pipeline.MetricsRecord {
	contextMutex.RLock()
	if len(p.MetricsRecords) > 0 {
		defer contextMutex.RUnlock()
		return p.MetricsRecords[len(p.MetricsRecords)-1]
	}
	contextMutex.RUnlock()
	return p.RegisterMetricRecord(nil)
}

func (p *EmptyContext) MetricSerializeToPB(logGroup *protocol.LogGroup) {
	if logGroup == nil {
		return
	}

	contextMutex.Lock()
	defer contextMutex.Unlock()
	for _, metricsRecord := range p.MetricsRecords {
		metricsRecord.Serialize(logGroup)
	}
}

// ExportMetricRecords is used for exporting metrics records.
// Each metric is a map[string]string
func (p *EmptyContext) ExportMetricRecords() []map[string]string {
	contextMutex.RLock()
	defer contextMutex.RUnlock()

	records := make([]map[string]string, 0)
	for _, metricsRecord := range p.MetricsRecords {
		records = append(records, metricsRecord.ExportMetricRecords()...)
	}
	return records
}

func (p *EmptyContext) SaveCheckPoint(key string, value []byte) error {
	logger.Debug(p.ctx, "save checkpoint, key", key, "value", string(value))
	p.checkpoint[key] = value
	return nil
}

func (p *EmptyContext) GetCheckPoint(key string) (value []byte, exist bool) {
	value, ok := p.checkpoint[key]
	logger.Debug(p.ctx, "get checkpoint, key", key, "value", string(value), "ok", ok)
	return value, ok
}

func (p *EmptyContext) SaveCheckPointObject(key string, obj interface{}) error {
	val, err := json.Marshal(obj)
	if err != nil {
		return err
	}
	return p.SaveCheckPoint(key, val)
}

func (p *EmptyContext) GetCheckPointObject(key string, obj interface{}) (exist bool) {
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

func (p *EmptyContext) GetRuntimeContext() context.Context {
	return p.ctx
}

func (p *EmptyContext) GetExtension(name string, cfg any) (pipeline.Extension, error) {
	creator, ok := pipeline.Extensions[name]
	if !ok {
		return nil, fmt.Errorf("extension %s not found", name)
	}
	extension := creator()
	config, err := json.Marshal(cfg)
	if err != nil {
		return nil, err
	}
	err = json.Unmarshal(config, extension)
	if err != nil {
		return nil, err
	}
	err = extension.Init(p)
	if err != nil {
		return nil, err
	}
	return extension, nil
}

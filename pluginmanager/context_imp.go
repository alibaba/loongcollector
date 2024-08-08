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
	"fmt"
	"sync"

	"github.com/alibaba/ilogtail/pkg"
	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

type ContextImp struct {
	MetricsRecords []*pipeline.MetricsRecord

	common      *pkg.LogtailContextMeta
	pluginNames string
	ctx         context.Context
	logstoreC   *LogstoreConfig
}

var contextMutex sync.RWMutex

func (p *ContextImp) GetRuntimeContext() context.Context {
	return p.ctx
}

func (p *ContextImp) GetExtension(name string, cfg any) (pipeline.Extension, error) {
	if p.logstoreC == nil || p.logstoreC.PluginRunner == nil {
		return nil, fmt.Errorf("pipeline not initialized")
	}
	// try to find in extensions that explicitly defined in pipeline
	exists, ok := p.logstoreC.PluginRunner.GetExtension(name)
	if ok {
		return exists, nil
	}

	// if it's a naming extension, we won't do further create
	if getPluginType(name) != getPluginTypeWithID(name) {
		return nil, fmt.Errorf("not found extension: %s", name)
	}

	// create if not found
	typeWithID := genEmbeddedPluginName(getPluginType(name))
	err := loadExtension(typeWithID, p.logstoreC, cfg)
	if err != nil {
		return nil, err
	}

	// get the new created extension
	exists, ok = p.logstoreC.PluginRunner.GetExtension(typeWithID)
	if !ok {
		return nil, fmt.Errorf("failed to load extension: %s", typeWithID)
	}
	return exists, nil
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

func (p *ContextImp) GetPipelineScopeConfig() *config.GlobalConfig {
	return p.logstoreC.GlobalConfig
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

func (p *ContextImp) RegisterMetricRecord(labels []pipeline.LabelPair) *pipeline.MetricsRecord {
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

func (p *ContextImp) GetMetricRecord() *pipeline.MetricsRecord {
	contextMutex.RLock()
	if len(p.MetricsRecords) > 0 {
		defer contextMutex.RUnlock()
		return p.MetricsRecords[len(p.MetricsRecords)-1]
	}
	contextMutex.RUnlock()
	return p.RegisterMetricRecord(nil)
}

func (p *ContextImp) MetricSerializeToPB(logGroup *protocol.LogGroup) {
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
func (p *ContextImp) ExportMetricRecords() []map[string]string {
	contextMutex.RLock()
	defer contextMutex.RUnlock()

	records := make([]map[string]string, 0)
	for _, metricsRecord := range p.MetricsRecords {
		records = append(records, metricsRecord.ExportMetricRecords())
	}
	return records
}

func (p *ContextImp) SaveCheckPoint(key string, value []byte) error {
	logger.Debug(p.ctx, "save checkpoint, key", key, "value", string(value))
	return CheckPointManager.SaveCheckpoint(p.GetConfigName(), key, value)
}

func (p *ContextImp) GetCheckPoint(key string) (value []byte, exist bool) {
	configName := p.GetConfigName()
	l := len(configName)
	if l > 2 && configName[l-2:] == "/1" {
		configName = configName[:l-2]
	}
	value, err := CheckPointManager.GetCheckpoint(configName, key)
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

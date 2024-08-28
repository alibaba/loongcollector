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

package packjson

import (
	"encoding/json"
	"fmt"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type ProcessorPackjson struct {
	SourceKeys        []string // 需要打包的 key
	DestKey           string   // 目标 key
	KeepSource        bool     // 是否保留源字段
	AlarmIfIncomplete bool     // 是否在不存在任何源字段时告警
	keyDictionary     map[string]bool
	context           pipeline.Context
}

const pluginType = "processor_packjson"

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorPackjson) Init(context pipeline.Context) error {
	if len(p.SourceKeys) == 0 {
		return fmt.Errorf("must specify SourceKeys for plugin %v", pluginType)
	}
	if p.DestKey == "" {
		return fmt.Errorf("must specify DestKey for plugin %v", pluginType)
	}
	p.keyDictionary = make(map[string]bool)
	for _, sourceKey := range p.SourceKeys {
		p.keyDictionary[sourceKey] = false
	}
	p.context = context
	return nil
}

func (*ProcessorPackjson) Description() string {
	return "packjson processor for logtail"
}

func (p *ProcessorPackjson) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		p.processLog(log)
	}
	return logArray
}

func (p *ProcessorPackjson) processLog(log *protocol.Log) {
	packMap := make(map[string]string)
	for idx := len(log.Contents) - 1; idx >= 0; idx-- {
		if _, exists := p.keyDictionary[log.Contents[idx].Key]; exists {
			packMap[log.Contents[idx].Key] = log.Contents[idx].Value
			if !p.KeepSource {
				log.Contents = append(log.Contents[:idx], log.Contents[idx+1:]...)
			}
		}
	}
	if p.AlarmIfIncomplete && len(p.SourceKeys) != len(packMap) {
		var emptyKeys []string
		for _, key := range p.SourceKeys {
			if _, exists := packMap[key]; !exists {
				emptyKeys = append(emptyKeys, key)
			}
		}
		logger.Warningf(p.context.GetRuntimeContext(), "PACK_JSON_ALARM", "SourceKeys not found %v", emptyKeys)
	}
	newValue, err := json.Marshal(packMap)
	if err != nil {
		logger.Errorf(p.context.GetRuntimeContext(), "PACK_JSON_ALARM", "package json error %v packMap: %v", err, packMap)
		return
	}
	newContent := &protocol.Log_Content{
		Key:   p.DestKey,
		Value: string(newValue),
	}
	log.Contents = append(log.Contents, newContent)
}

func init() {
	pipeline.Processors[pluginType] = func() pipeline.Processor {
		return &ProcessorPackjson{
			SourceKeys:        nil,
			DestKey:           "",
			KeepSource:        true,
			AlarmIfIncomplete: true,
		}
	}
}

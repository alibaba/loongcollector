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

package rename

import (
	"fmt"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type ProcessorRename struct {
	NoKeyError          bool
	SourceKeys          []string
	DestKeys            []string
	keyDictionary       map[string]int
	noKeyErrorBoolArray []bool
	context             ilogtail.Context
}

const pluginName = "processor_rename"

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorRename) Init(context ilogtail.Context) error {
	p.context = context
	if len(p.SourceKeys) == 0 {
		return fmt.Errorf("must specify SourceKeys for plugin %v", pluginName)
	}
	if len(p.DestKeys) == 0 {
		return fmt.Errorf("must specify DestKeys for plugin %v", pluginName)
	}
	if len(p.SourceKeys) != len(p.DestKeys) {
		return fmt.Errorf("The length of SourceKeys does not match the length of DestKeys for plugin %v", pluginName)
	}
	p.keyDictionary = make(map[string]int)
	for i, source := range p.SourceKeys {
		p.keyDictionary[source] = i
	}
	if p.NoKeyError {
		p.noKeyErrorBoolArray = make([]bool, len(p.SourceKeys))
	}
	return nil
}

func (*ProcessorRename) Description() string {
	return "rename processor for logtail"
}

func (p *ProcessorRename) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		p.processLog(log)
	}
	return logArray
}

func (p *ProcessorRename) checkNoKeyError() []string {
	var errorArray []string
	for idx, isError := range p.noKeyErrorBoolArray {
		if !isError {
			errorArray = append(errorArray, p.SourceKeys[idx])
		}
	}
	return errorArray
}

func (p *ProcessorRename) processLog(log *protocol.Log) {
	if p.NoKeyError {
		for idx := range p.noKeyErrorBoolArray {
			p.noKeyErrorBoolArray[idx] = false
		}
	}
	for _, content := range log.Contents {
		oldKey := content.Key
		if idx, exists := p.keyDictionary[oldKey]; exists {
			content.Key = p.DestKeys[idx]
			if p.NoKeyError {
				p.noKeyErrorBoolArray[idx] = true
			}
		}
	}
	if p.NoKeyError {
		if errorArray := p.checkNoKeyError(); errorArray != nil {
			logger.Warningf(p.context.GetRuntimeContext(), "RENAME_FIND_ALARM", "cannot find key %v", errorArray)
		}
	}
}

func (p *ProcessorRename) Process(in *models.PipelineGroupEvents, context ilogtail.PipelineContext) {
	if p.NoKeyError {
		for idx := range p.noKeyErrorBoolArray {
			p.noKeyErrorBoolArray[idx] = false
		}
	}
	for _, event := range in.Events {
		tags := event.GetTags()
		for idx, key := range p.SourceKeys {
			if tags.Contains(key) {
				tags.Add(p.DestKeys[idx], tags.Get(key))
				tags.Delete(key)
				if p.NoKeyError {
					p.noKeyErrorBoolArray[idx] = true
				}
			}
		}
	}
	context.Collector().Collect(in.Group, in.Events...)
	if p.NoKeyError {
		if errorArray := p.checkNoKeyError(); errorArray != nil {
			logger.Warningf(p.context.GetRuntimeContext(), "RENAME_FIND_ALARM", "cannot find key %v", errorArray)
		}
	}
}

func init() {
	ilogtail.Processors[pluginName] = func() ilogtail.Processor {
		return &ProcessorRename{
			NoKeyError: false,
			SourceKeys: nil,
			DestKeys:   nil,
		}
	}
}

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

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type ProcessorRename struct {
	NoKeyError         bool
	SourceKeys         []string
	DestKeys           []string
	keyDictionary      map[string]string // use map to eliminate duplicate keys and invalid renames
	existKeyDictionary map[string]bool   // not necessary in v2 pipeline
	noKeyErrorArray    []string
	context            pipeline.Context
}

const pluginType = "processor_rename"

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorRename) Init(context pipeline.Context) error {
	p.context = context
	if len(p.SourceKeys) == 0 {
		return fmt.Errorf("must specify SourceKeys for plugin %v", pluginType)
	}
	if len(p.DestKeys) == 0 {
		return fmt.Errorf("must specify DestKeys for plugin %v", pluginType)
	}
	if len(p.SourceKeys) != len(p.DestKeys) {
		return fmt.Errorf("The length of SourceKeys does not match the length of DestKeys for plugin %v", pluginType)
	}
	p.keyDictionary = make(map[string]string)
	for i, source := range p.SourceKeys {
		if source != p.DestKeys[i] {
			p.keyDictionary[source] = p.DestKeys[i]
		}
	}
	if p.NoKeyError {
		p.existKeyDictionary = make(map[string]bool)
		for i, source := range p.SourceKeys {
			if source != p.DestKeys[i] {
				p.existKeyDictionary[source] = false
			}
		}
		p.noKeyErrorArray = make([]string, len(p.SourceKeys))
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

func (p *ProcessorRename) processLog(log *protocol.Log) {
	if p.NoKeyError {
		p.noKeyErrorArray = p.noKeyErrorArray[:0]
		for k := range p.existKeyDictionary {
			p.existKeyDictionary[k] = false
		}
	}
	for _, content := range log.Contents {
		oldKey := content.Key
		if newKey, exists := p.keyDictionary[oldKey]; exists {
			content.Key = newKey
			if p.NoKeyError {
				p.existKeyDictionary[oldKey] = true
			}
		}
	}
	if p.NoKeyError {
		for k, v := range p.existKeyDictionary {
			if !v {
				p.noKeyErrorArray = append(p.noKeyErrorArray, k)
			}
		}
		if len(p.noKeyErrorArray) != 0 {
			logger.Warningf(p.context.GetRuntimeContext(), "RENAME_FIND_ALARM", "cannot find key %v", p.noKeyErrorArray)
		}
	}
}

func (p *ProcessorRename) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	if p.NoKeyError {
		p.noKeyErrorArray = p.noKeyErrorArray[:0]
	}
	for _, event := range in.Events {
		if event.GetType() == models.EventTypeLogging {
			logEvent := event.(*models.Log)
			p.processLogEvent(logEvent)
		} else {
			p.processOtherEvent(event)
		}
	}
	context.Collector().Collect(in.Group, in.Events...)
	if p.NoKeyError && len(p.noKeyErrorArray) != 0 {
		logger.Warningf(p.context.GetRuntimeContext(), "RENAME_FIND_ALARM", "cannot find key %v", p.noKeyErrorArray)
	}
}

func (p *ProcessorRename) processLogEvent(logEvent *models.Log) {
	contents := logEvent.GetIndices()
	tags := logEvent.GetTags()
	for oldKey, newKey := range p.keyDictionary {
		switch {
		case contents.Contains(oldKey):
			contents.Add(newKey, contents.Get(oldKey))
			contents.Delete(oldKey)
		case tags.Contains(oldKey):
			tags.Add(newKey, tags.Get(oldKey))
			tags.Delete(oldKey)
		case p.NoKeyError:
			p.noKeyErrorArray = append(p.noKeyErrorArray, oldKey)
		}
	}
}

func (p *ProcessorRename) processOtherEvent(event models.PipelineEvent) {
	tags := event.GetTags()
	for oldKey, newKey := range p.keyDictionary {
		if tags.Contains(oldKey) {
			tags.Add(newKey, tags.Get(oldKey))
			tags.Delete(oldKey)
		} else if p.NoKeyError {
			p.noKeyErrorArray = append(p.noKeyErrorArray, oldKey)
		}
	}
}

func init() {
	pipeline.Processors[pluginType] = func() pipeline.Processor {
		return &ProcessorRename{
			NoKeyError: false,
			SourceKeys: nil,
			DestKeys:   nil,
		}
	}
}

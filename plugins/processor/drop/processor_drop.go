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

package drop

import (
	"fmt"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type ProcessorDrop struct {
	DropKeys []string

	keyDictionary map[string]bool
	context       pipeline.Context
}

const pluginType = "processor_drop"

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorDrop) Init(context pipeline.Context) error {
	if len(p.DropKeys) == 0 {
		return fmt.Errorf("must specify DropKeys for plugin %v", pluginType)
	}

	p.context = context
	p.keyDictionary = make(map[string]bool)
	for _, dropKey := range p.DropKeys {
		p.keyDictionary[dropKey] = true
	}
	return nil
}

func (*ProcessorDrop) Description() string {
	return "drop processor for logtail"
}

func (p *ProcessorDrop) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		p.processLog(log)
	}
	return logArray
}

func (p *ProcessorDrop) processLog(log *protocol.Log) {
	for idx := len(log.Contents) - 1; idx >= 0; idx-- {
		if _, exists := p.keyDictionary[log.Contents[idx].Key]; exists {
			log.Contents = append(log.Contents[:idx], log.Contents[idx+1:]...)
		}
	}
}

func init() {
	pipeline.Processors[pluginType] = func() pipeline.Processor {
		return &ProcessorDrop{}
	}
}

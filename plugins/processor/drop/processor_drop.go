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

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
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

// Process implements the Processor interface: it drops the configured keys
// from each Log event and passes Metric/Span events through unchanged.
func (p *ProcessorDrop) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	pipeline.ProcessLogEventsOnly(in, context, p.processLogEvent)
}

func (p *ProcessorDrop) processLogEvent(log *models.Log) {
	contents := log.GetIndices()
	for dropKey := range p.keyDictionary {
		if contents.Contains(dropKey) {
			contents.Delete(dropKey)
		}
	}
}

func init() {
	pipeline.Processors[pluginType] = func() pipeline.Processor {
		return &ProcessorDrop{}
	}
}

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

package droplastkey

import (
	"fmt"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

// ProcessorDropLastKey is used to drop log content when process done
type ProcessorDropLastKey struct {
	Include []string
	DropKey string

	includeMap map[string]struct{}

	filterMetric pipeline.CounterMetric
	context      pipeline.Context
}

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorDropLastKey) Init(context pipeline.Context) error {
	p.context = context

	metricsRecord := p.context.GetMetricRecord()
	p.filterMetric = helper.NewCounterMetricAndRegister(metricsRecord, "drop_key_count")

	if len(p.DropKey) == 0 {
		return fmt.Errorf("Invalid config, DropKey is empty")
	}

	if len(p.Include) > 0 {
		p.includeMap = make(map[string]struct{})
		for _, key := range p.Include {
			p.includeMap[key] = struct{}{}
		}
	} else {
		return fmt.Errorf("Invalid config, Include is empty")
	}

	return nil
}

func (*ProcessorDropLastKey) Description() string {
	return "processor_drop_last_key is used to drop log content when process done"
}

func (p *ProcessorDropLastKey) process(log *protocol.Log) {
	dropFlag := false
	for _, content := range log.Contents {
		if _, ok := p.includeMap[content.Key]; ok {
			dropFlag = true
			break
		}
	}
	if !dropFlag {
		return
	}

	for index, content := range log.Contents {
		if content.Key == p.DropKey {
			log.Contents = append(log.Contents[:index], log.Contents[index+1:]...)
			p.filterMetric.Add(1)
			break
		}
	}
}

func (p *ProcessorDropLastKey) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		p.process(log)
	}
	return logArray
}

func init() {
	pipeline.Processors["processor_drop_last_key"] = func() pipeline.Processor {
		return &ProcessorDropLastKey{}
	}
}

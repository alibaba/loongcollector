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

package addenvs

import (
	"fmt"
    "os"

    "github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

// ProcessorAddEnvs struct implement the Processor interface.
// The plugin would append the field to the input data.
type ProcessorAddEnvs struct {
	Envs        []string // the appending envs
	IgnoreIfExist bool              // Whether to ignore when the same key exists
	context       ilogtail.Context
}

const pluginName = "processor_add_envs"

// Init method would be triggered before working for init some system resources,
// like socket, mutex. In this plugin, it verifies Fields must not be empty.
func (p *ProcessorAddEnvs) Init(context ilogtail.Context) error {
	if len(p.Envs) == 0 {
		return fmt.Errorf("must specify Fields for plugin %v", pluginName)
	}
	p.context = context
	return nil
}

func (*ProcessorAddEnvs) Description() string {
	return "add fields processor for ilogtail"
}

// ProcessLogs append Fields to each log.
func (p *ProcessorAddEnvs) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		p.processLog(log)
	}
	return logArray
}

func (p *ProcessorAddEnvs) processLog(log *protocol.Log) {
	if p.IgnoreIfExist && len(p.Envs) > 1 {
		dict := make(map[string]bool)
		for idx := range log.Contents {
			dict[log.Contents[idx].Key] = true
		}
		for _, k := range p.Envs {
			if _, exists := dict[k]; exists {
				continue
			}
			newContent := &protocol.Log_Content{
				Key:   k,
				Value: os.Getenv(k),
			}
			log.Contents = append(log.Contents, newContent)
		}
	} else {
		for _, k := range p.Envs {
			if p.IgnoreIfExist && p.isExist(log, k) {
				continue
			}
			newContent := &protocol.Log_Content{
				Key:   k,
				Value: os.Getenv(k),
			}
			log.Contents = append(log.Contents, newContent)
		}
	}
}

func (p *ProcessorAddEnvs) isExist(log *protocol.Log, key string) bool {
	for idx := range log.Contents {
		if log.Contents[idx].Key == key {
			return true
		}
	}
	return false
}

// Register the plugin to the Processors array.
func init() {
	ilogtail.Processors[pluginName] = func() ilogtail.Processor {
		return &ProcessorAddEnvs{
			Envs:        nil,
			IgnoreIfExist: false,
		}
	}
}

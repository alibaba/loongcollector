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

package encoding

import (
	"encoding/base64"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type ProcessorBase64Encoding struct {
	SourceKey  string
	NewKey     string
	NoKeyError bool

	context pipeline.Context
}

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorBase64Encoding) Init(context pipeline.Context) error {
	p.context = context
	return nil
}

func (*ProcessorBase64Encoding) Description() string {
	return "base64 encoding processor for logtail"
}

func (p *ProcessorBase64Encoding) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		found := false
		for _, content := range log.Contents {
			if content.Key == p.SourceKey {
				newVal := base64.StdEncoding.EncodeToString([]byte(content.Value))
				if len(p.NewKey) > 0 {
					newContent := &protocol.Log_Content{
						Key:   p.NewKey,
						Value: newVal,
					}
					log.Contents = append(log.Contents, newContent)
				} else {
					content.Value = newVal
				}
				found = true
				break
			}
		}
		if !found && p.NoKeyError {
			logger.Warning(p.context.GetRuntimeContext(), "BASE64_E_FIND_ALARM", "cannot find key", p.SourceKey)
		}
	}
	return logArray
}

func init() {
	pipeline.Processors["processor_base64_encoding"] = func() pipeline.Processor {
		return &ProcessorBase64Encoding{}
	}
}

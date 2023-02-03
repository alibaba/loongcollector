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

package md5

import (
	"crypto/md5" //nolint:gosec
	"fmt"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type ProcessorMD5 struct {
	SourceKey  string
	MD5Key     string
	NoKeyError bool

	context pipeline.Context
}

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorMD5) Init(context pipeline.Context) error {
	p.context = context
	return nil
}

func (*ProcessorMD5) Description() string {
	return "md5 processor for logtail"
}

func (p *ProcessorMD5) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		found := false
		for _, content := range log.Contents {
			if content.Key == p.SourceKey {
				newVal := fmt.Sprintf("%x", md5.Sum([]byte(content.Value))) //nolint:gosec
				newContent := &protocol.Log_Content{
					Key:   p.MD5Key,
					Value: newVal,
				}
				log.Contents = append(log.Contents, newContent)
				found = true
				break
			}
		}
		if !found && p.NoKeyError {
			logger.Warning(p.context.GetRuntimeContext(), "MD5_FIND_ALARM", "cannot find key", p.SourceKey)
		}
	}
	return logArray
}

func init() {
	pipeline.Processors["processor_md5"] = func() pipeline.Processor {
		return &ProcessorMD5{}
	}
}

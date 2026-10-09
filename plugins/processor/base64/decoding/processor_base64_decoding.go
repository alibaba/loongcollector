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

package decoding

import (
	"encoding/base64"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

type ProcessorBase64Decoding struct {
	SourceKey   string
	NewKey      string
	NoKeyError  bool
	DecodeError bool

	context pipeline.Context
}

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorBase64Decoding) Init(context pipeline.Context) error {
	p.context = context
	return nil
}

func (*ProcessorBase64Decoding) Description() string {
	return "base64 decoding processor for logtail"
}

func (p *ProcessorBase64Decoding) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		found := false
		for _, content := range log.Contents {
			if content.Key == p.SourceKey {
				newVal, err := base64.StdEncoding.DecodeString(content.Value)
				if err == nil {
					newContent := &protocol.Log_Content{
						Key:   p.NewKey,
						Value: string(newVal),
					}
					log.Contents = append(log.Contents, newContent)
				} else if p.DecodeError {
					logger.Warning(p.context.GetRuntimeContext(), selfmonitor.Base64DAlarm, "decode base64 error", err)
				}
				found = true
				break
			}
		}
		if !found && p.NoKeyError {
			logger.Warning(p.context.GetRuntimeContext(), selfmonitor.Base64DFindAlarm, "cannot find key", p.SourceKey)
		}
	}
	return logArray
}

// Process implements the v2 ProcessorV2 interface: it base64-decodes the
// SourceKey value of each Log event into NewKey; Metric/Span events pass
// through unchanged.
func (p *ProcessorBase64Decoding) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	pipeline.ProcessLogEventsOnly(in, context, p.processLogEvent)
}

func (p *ProcessorBase64Decoding) processLogEvent(log *models.Log) {
	contents := log.GetIndices()
	if !contents.Contains(p.SourceKey) {
		if p.NoKeyError {
			logger.Warning(p.context.GetRuntimeContext(), selfmonitor.Base64DFindAlarm, "cannot find key", p.SourceKey)
		}
		return
	}
	newVal, err := base64.StdEncoding.DecodeString(pipeline.GetStringValue(contents.Get(p.SourceKey)))
	if err != nil {
		if p.DecodeError {
			logger.Warning(p.context.GetRuntimeContext(), selfmonitor.Base64DAlarm, "decode base64 error", err)
		}
		return
	}
	contents.Add(p.NewKey, string(newVal))
}

func init() {
	pipeline.Processors["processor_base64_decoding"] = func() pipeline.Processor {
		return &ProcessorBase64Decoding{}
	}
}

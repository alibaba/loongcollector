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

package logstring

import (
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

type ProcessorSplit struct {
	SplitKey              string
	SplitSep              string
	PreserveOthers        bool
	NoKeyError            bool
	EnableLogPositionMeta bool

	context pipeline.Context
}

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorSplit) Init(context pipeline.Context) error {
	p.context = context
	if len(p.SplitSep) == 0 {
		p.SplitSep = "\u0000"
		logger.Infof(p.context.GetRuntimeContext(), "no separator is specified, use \\0")
	}
	return nil
}

func (*ProcessorSplit) Description() string {
	return "raw log split for logtail"
}

func (p *ProcessorSplit) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	for _, event := range in.Events {
		log, ok := event.(*models.Log)
		if !ok {
			// Metric/Span and other non-Log events are not splittable, so pass
			// them through unchanged rather than silently dropping them.
			context.Collector().Collect(in.Group, event)
			continue
		}
		contents := log.GetIndices()
		key := p.SplitKey
		if key == "" {
			if first, ordered := models.FirstLogContentKey(contents); ordered {
				key = first
			} else if contents.Contains(models.BodyKey) {
				key = models.BodyKey
			} else {
				for field := range contents.Iterator() {
					key = field
					break
				}
			}
		}

		if !contents.Contains(key) {
			if p.NoKeyError {
				logger.Warning(p.context.GetRuntimeContext(), selfmonitor.ProcessorSplitLogStringFindAlarm, "can't find split key", p.SplitKey)
			}
			if p.PreserveOthers {
				if log.Timestamp == 0 {
					log = log.Clone().(*models.Log)
					log.Timestamp = uint64(time.Now().Truncate(time.Second).UnixNano())
				}
				context.Collector().Collect(in.Group, log)
			}
			continue
		}
		payload := pipeline.GetStringValue(contents.Get(key))
		if payload == "" {
			continue
		}

		template := log.Clone().(*models.Log)
		if p.PreserveOthers {
			template.GetIndices().Delete(key)
		} else {
			template.SetIndices(models.NewOrderedLogContents(1))
			template.Tags = models.NewTags()
		}
		if template.Timestamp == 0 {
			template.Timestamp = uint64(time.Now().Truncate(time.Second).UnixNano())
		}
		hasOffset := log.GetOffset() != 0 || log.GetRawSize() != 0
		parts := strings.Split(payload, p.SplitSep)
		var offset uint64
		for i, part := range parts {
			if part == "" {
				continue
			}
			output := template
			if i < len(parts)-1 {
				output = template.Clone().(*models.Log)
			}
			output.GetIndices().Add(key, part)
			if p.EnableLogPositionMeta && hasOffset {
				output.SetOffset(log.GetOffset() + offset)
				offset += uint64(len(part) + len(p.SplitSep))
			}
			context.Collector().Collect(in.Group, output)
		}
	}
}

func init() {
	pipeline.Processors["processor_split_log_string"] = func() pipeline.Processor {
		return &ProcessorSplit{SplitSep: "\n", PreserveOthers: true}
	}
}

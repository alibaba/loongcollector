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

package logregex

import (
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"regexp"
	"time"

	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

type ProcessorSplitRegex struct {
	SplitKey              string
	SplitRegex            string
	PreserveOthers        bool
	NoKeyError            bool
	EnableLogPositionMeta bool

	context pipeline.Context
	regex   *regexp.Regexp
}

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorSplitRegex) Init(context pipeline.Context) error {
	p.context = context
	var err error
	if p.regex, err = regexp.Compile(p.SplitRegex); err != nil {
		return err
	}
	return nil
}

func (*ProcessorSplitRegex) Description() string {
	return "raw log regex split for logtail"
}

func fullMatch(reg *regexp.Regexp, str string) bool {
	rst := reg.FindStringSubmatchIndex(str)
	return len(rst) >= 2 && rst[0] == 0 && rst[1] == len(str)
}

func init() {
	pipeline.Processors["processor_split_log_regex"] = func() pipeline.Processor {
		return &ProcessorSplitRegex{SplitRegex: ".*", PreserveOthers: false}
	}
}

// splitSegment holds the text of a record and its byte offset in the input.
type splitSegment struct {
	value  string
	offset int64
}

// splitValue groups lines into log segments using SplitRegex to identify record starts.
func (p *ProcessorSplitRegex) splitValue(valueStr string) []splitSegment {
	segments := make([]splitSegment, 0)
	lastLineIndex := 0
	lastCheckIndex := 0

	for i := 0; i < len(valueStr); i++ {
		if valueStr[i] == '\n' {
			// Case 1: current line matches, combine lines before as log content.
			if fullMatch(p.regex, valueStr[lastCheckIndex:i]) && (lastLineIndex != 0 || lastCheckIndex != 0) {
				segments = append(segments, splitSegment{value: valueStr[lastLineIndex : lastCheckIndex-1], offset: int64(lastLineIndex)})
				lastLineIndex = lastCheckIndex
			}
			lastCheckIndex = i + 1
		}
	}

	// Case 2: the last line does not end with \n, check if it matches, if so,
	//   combine lines before as log content.
	if lastCheckIndex != 0 && lastCheckIndex < len(valueStr) {
		if fullMatch(p.regex, valueStr[lastCheckIndex:]) {
			segments = append(segments, splitSegment{value: valueStr[lastLineIndex : lastCheckIndex-1], offset: int64(lastLineIndex)})
			lastLineIndex = lastCheckIndex
		}
	}

	// Case 3: still has content, combine remainder lines as log content.
	if lastLineIndex < len(valueStr) {
		segments = append(segments, splitSegment{value: valueStr[lastLineIndex:], offset: int64(lastLineIndex)})
	}
	return segments
}

// Process emits one log per multiline segment and passes other event types through.
func (p *ProcessorSplitRegex) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	if in == nil {
		return
	}
	outEvents := make([]models.PipelineEvent, 0, len(in.Events))
	for _, event := range in.Events {
		log, ok := event.(*models.Log)
		if !ok {
			// Metric/Span (and any other kind) are not splittable; pass through.
			outEvents = append(outEvents, event)
			continue
		}

		contents := log.GetIndices()

		splitKey := p.SplitKey
		if splitKey == "" {
			if first, ordered := models.FirstLogContentKey(contents); ordered {
				splitKey = first
			} else {
				for key := range contents.Iterator() {
					splitKey = key
					break
				}
			}
		}

		if !contents.Contains(splitKey) {
			// Keep logs without the split key only when PreserveOthers is set.
			if p.NoKeyError {
				logger.Warning(p.context.GetRuntimeContext(), selfmonitor.LogRegexFindAlarm, "can't find split key", p.SplitKey)
			}
			if p.PreserveOthers {
				if log.Timestamp == 0 {
					log = log.Clone().(*models.Log)
					log.Timestamp = uint64(time.Now().Truncate(time.Second).UnixNano())
				}
				outEvents = append(outEvents, log)
			}
			continue
		}

		segments := p.splitValue(pipeline.GetStringValue(contents.Get(splitKey)))
		if len(segments) == 0 {
			continue
		}
		template := log.Clone().(*models.Log)
		if p.PreserveOthers {
			// Split payloads are appended after the preserved fields.
			template.GetIndices().Delete(splitKey)
		} else {
			template.SetIndices(models.NewOrderedLogContents(1))
		}
		if template.Timestamp == 0 {
			template.Timestamp = uint64(time.Now().Truncate(time.Second).UnixNano())
		}
		hasOffset := log.GetOffset() != 0 || log.GetRawSize() != 0
		for i, seg := range segments {
			newLog := template
			if i < len(segments)-1 {
				newLog = template.Clone().(*models.Log)
			}
			newLog.GetIndices().Add(splitKey, seg.value)
			if p.EnableLogPositionMeta && hasOffset {
				newLog.SetOffset(log.GetOffset() + uint64(seg.offset))
			}
			outEvents = append(outEvents, newLog)
		}
	}
	if len(outEvents) > 0 {
		context.Collector().Collect(in.Group, outEvents...)
	}
}

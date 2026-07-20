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
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"

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

func (p *ProcessorSplitRegex) SplitLog(logArray []*protocol.Log, rawLog *protocol.Log, cont *protocol.Log_Content) []*protocol.Log {
	valueStr := cont.GetValue()
	lastLineIndex := 0
	lastCheckIndex := 0

	for i := 0; i < len(valueStr); i++ {
		if valueStr[i] == '\n' {
			// Case 1: current line matches, combine lines before as log content.
			if fullMatch(p.regex, valueStr[lastCheckIndex:i]) && (lastLineIndex != 0 || lastCheckIndex != 0) {
				copyLog := protocol.CloneLog(rawLog)
				copyLog.Contents = append(copyLog.Contents, &protocol.Log_Content{
					Key: cont.GetKey(), Value: valueStr[lastLineIndex : lastCheckIndex-1]})
				helper.ReviseFileOffset(copyLog, int64(lastLineIndex), p.EnableLogPositionMeta)
				logArray = append(logArray, copyLog)
				lastLineIndex = lastCheckIndex
			}
			lastCheckIndex = i + 1
		}
	}

	// Case 2: the last line does not end with \n, check if it matches, if so,
	//   combine lines before as log content.
	// Special case: only one line without \n, should skip and be handled by case 3.
	if lastCheckIndex != 0 && lastCheckIndex < len(valueStr) {
		if fullMatch(p.regex, valueStr[lastCheckIndex:]) {
			copyLog := protocol.CloneLog(rawLog)
			copyLog.Contents = append(copyLog.Contents, &protocol.Log_Content{
				Key: cont.GetKey(), Value: valueStr[lastLineIndex : lastCheckIndex-1]})
			helper.ReviseFileOffset(copyLog, int64(lastLineIndex), p.EnableLogPositionMeta)
			logArray = append(logArray, copyLog)
			lastLineIndex = lastCheckIndex
		}
	}

	// Case 3: still has content, combine remainder lines as log content.
	if lastLineIndex < len(valueStr) {
		rawLog.Contents = append(rawLog.Contents, &protocol.Log_Content{
			Key: cont.GetKey(), Value: valueStr[lastLineIndex:]})
		helper.ReviseFileOffset(rawLog, int64(lastLineIndex), p.EnableLogPositionMeta)
		logArray = append(logArray, rawLog)
	}
	return logArray
}

func (p *ProcessorSplitRegex) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	destArray := make([]*protocol.Log, 0, len(logArray))

	for _, log := range logArray {
		newLog := &protocol.Log{}
		var destCont *protocol.Log_Content
		for _, cont := range log.Contents {
			if destCont == nil && (len(p.SplitKey) == 0 || cont.Key == p.SplitKey) {
				destCont = cont
			} else if p.PreserveOthers {
				newLog.Contents = append(newLog.Contents, cont)
			}
		}
		if log.Time != uint32(0) {
			if log.TimeNs != nil {
				protocol.SetLogTimeWithNano(newLog, log.Time, *log.TimeNs)
			} else {
				protocol.SetLogTime(newLog, log.Time)
			}
		} else {
			nowTime := time.Now()
			protocol.SetLogTime(newLog, uint32(nowTime.Unix()))
		}
		if destCont != nil {
			destArray = p.SplitLog(destArray, newLog, destCont)
		} else {
			if p.NoKeyError {
				logger.Warning(p.context.GetRuntimeContext(), selfmonitor.LogRegexFindAlarm, "can't find split key", p.SplitKey)
			}
			if p.PreserveOthers {
				destArray = append(destArray, newLog)
			}
		}
	}

	return destArray
}

func init() {
	pipeline.Processors["processor_split_log_regex"] = func() pipeline.Processor {
		return &ProcessorSplitRegex{SplitRegex: ".*", PreserveOthers: false}
	}
}

// splitSegment holds one split result: the segment value together with the
// byte offset of its first character within the original SplitKey value. The
// offset mirrors v1 SplitLog's lastLineIndex and is used to revise the file
// offset of the produced log.
type splitSegment struct {
	value  string
	offset int64
}

// splitValue reproduces the exact line-grouping algorithm of the v1 SplitLog,
// operating purely on the raw string instead of protocol.Log. Given the
// SplitKey value it returns the ordered list of segments, where each segment
// groups the lines that belong to a single output log. Keeping this identical
// to SplitLog guarantees v1 and v2 split the same input into the same number of
// logs with the same content values.
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

// Process implements the v2 ProcessorV2 interface. It performs the same
// line-start-regex multiline split as v1 ProcessLogs, but on models.Log events:
// one input Log can yield several output Logs. Non-Log events (Metric/Span) are
// passed through unchanged so they are never dropped. Because the number of Log
// events changes (1->N), the output slice is built manually rather than via
// ProcessLogEventsOnly, and all events are collected in original group order.
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

		// Resolve the split key. When SplitKey is empty, v1 uses the first
		// content field; v2 contents are an (unordered) map, so this is a
		// best-effort pick via Iterator and the chosen key is not guaranteed to
		// match v1's slice order when the log has multiple content keys.
		splitKey := p.SplitKey
		if len(splitKey) == 0 {
			for k := range contents.Iterator() {
				splitKey = k
				break
			}
		}

		if len(splitKey) == 0 || !contents.Contains(splitKey) {
			// Split key not found: mirror v1 -- warn (NoKeyError) and keep the
			// whole log only when PreserveOthers is set, otherwise drop it.
			if p.NoKeyError {
				logger.Warning(p.context.GetRuntimeContext(), selfmonitor.LogRegexFindAlarm, "can't find split key", p.SplitKey)
			}
			if p.PreserveOthers {
				outEvents = append(outEvents, log)
			}
			continue
		}

		valueStr, _ := contents.Get(splitKey).(string)
		for _, seg := range p.splitValue(valueStr) {
			newLog := log.Clone().(*models.Log)
			// Clone shares the source Contents map, so build a fresh content set
			// for each output log to keep them independent. When PreserveOthers
			// is set, carry the other keys over just like v1.
			newContents := models.NewLogContents()
			if p.PreserveOthers {
				for k, v := range contents.Iterator() {
					if k != splitKey {
						newContents.Add(k, v)
					}
				}
			}
			newContents.Add(splitKey, seg.value)
			newLog.SetIndices(newContents)
			// v1 revises the __file_offset__ tag of a protocol.Log via
			// helper.ReviseFileOffset; models.Log has no such tag, so the offset
			// is replicated through the Offset field instead (same approach as
			// split_log_string v2).
			newLog.SetOffset(log.GetOffset() + uint64(seg.offset))
			outEvents = append(outEvents, newLog)
		}
	}
	if len(outEvents) > 0 {
		context.Collector().Collect(in.Group, outEvents...)
	}
}

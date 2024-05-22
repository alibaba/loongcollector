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
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"

	"regexp"
	"time"
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
				logger.Warning(p.context.GetRuntimeContext(), "LOG_REGEX_FIND_ALARM", "can't find split key", p.SplitKey)
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

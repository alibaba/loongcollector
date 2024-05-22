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

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
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

func (p *ProcessorSplit) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
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
			strArray := strings.Split(destCont.Value, p.SplitSep)
			if len(strArray) == 0 {
				return destArray
			}
			var offset int64
			for i := 0; i < len(strArray)-1; i++ {
				if len(strArray[i]) == 0 {
					continue
				}
				copyLog := protocol.CloneLog(newLog)
				copyLog.Contents = append(copyLog.Contents, &protocol.Log_Content{Key: destCont.Key, Value: strArray[i]})
				helper.ReviseFileOffset(copyLog, offset, p.EnableLogPositionMeta)
				offset += int64(len(strArray[i]) + len(p.SplitSep))
				destArray = append(destArray, copyLog)
			}
			newLogCont := strArray[len(strArray)-1]
			if (len(newLogCont)) > 0 {
				newLog.Contents = append(newLog.Contents, &protocol.Log_Content{Key: destCont.Key, Value: newLogCont})
				helper.ReviseFileOffset(newLog, offset, p.EnableLogPositionMeta)
				destArray = append(destArray, newLog)
			}
		} else {
			if p.NoKeyError {
				logger.Warning(p.context.GetRuntimeContext(), "PROCESSOR_SPLIT_LOG_STRING_FIND_ALARM", "can't find split key", p.SplitKey)
			}
			if p.PreserveOthers {
				destArray = append(destArray, newLog)
			}
		}
	}

	return destArray
}

func (p *ProcessorSplit) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	for _, event := range in.Events {
		if log, ok := event.(*models.Log); ok {
			tmpLog := &models.Log{
				Name:      log.Name,
				Level:     log.Level,
				Timestamp: log.Timestamp,
				SpanID:    log.SpanID,
				TraceID:   log.TraceID,
				Contents:  log.Contents,
			}
			tmpLog.SetBody(log.GetBody())
			tmpLog.Tags = models.NewTags()
			body := util.ZeroCopyBytesToString(log.GetBody())
			for k, v := range log.GetTags().Iterator() {
				if len(body) == 0 && k == p.SplitKey {
					body = v
				} else if p.PreserveOthers {
					tmpLog.Tags.Add(k, v)
				}
			}
			if tmpLog.Timestamp == uint64(0) {
				tmpLog.Timestamp = uint64(time.Now().UnixNano())
			}
			if len(body) > 0 {
				strArray := strings.Split(body, p.SplitSep)
				if len(strArray) == 0 {
					continue
				}
				var offset int64
				for i := 0; i < len(strArray); i++ {
					if len(strArray[i]) == 0 {
						continue
					}
					var newLog *models.Log
					if i < len(strArray)-1 {
						newLog = tmpLog.Clone().(*models.Log)
					} else {
						newLog = tmpLog
					}
					newLog.SetBody(util.ZeroCopyStringToBytes(strArray[i]))
					newLog.Offset += uint64(offset)
					offset += int64(len(strArray[i]) + len(p.SplitSep))
					context.Collector().Collect(in.Group, newLog)
				}
			} else {
				if p.NoKeyError {
					logger.Warning(p.context.GetRuntimeContext(), "PROCESSOR_SPLIT_LOG_STRING_FIND_ALARM", "can't find split key", p.SplitKey)
				}
				if p.PreserveOthers {
					context.Collector().Collect(in.Group, tmpLog)
				}
			}
		}
	}
}

func init() {
	pipeline.Processors["processor_split_log_string"] = func() pipeline.Processor {
		return &ProcessorSplit{SplitSep: "\n", PreserveOthers: true}
	}
}

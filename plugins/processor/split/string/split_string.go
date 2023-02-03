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

package string

import (
	"fmt"
	"strconv"
	"strings"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

// ProcessorSplitString is a processor plugin to split field (SourceKey) with multi-bytes
// separator (SplitSep) and reinsert extracted values into log with SplitKeys.
// If PreserveOthers is set, it will insert the remainder bytes (after spliting) into log:
// if ExpandOthers is not set, the key will be set to _split_preserve_, otherwise,
// ExpandKeyPrefix will be used to generate key, such as expand_1, expand_2.
type ProcessorSplitString struct {
	SplitSep        string
	SplitKeys       []string
	SourceKey       string
	PreserveOthers  bool
	ExpandOthers    bool
	ExpandKeyPrefix string
	NoKeyError      bool
	NoMatchError    bool
	KeepSource      bool

	context pipeline.Context
}

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorSplitString) Init(context pipeline.Context) error {
	if len(p.SplitSep) == 0 {
		return fmt.Errorf("no split separator")
	}
	p.context = context
	return nil
}

func (*ProcessorSplitString) Description() string {
	return "string splitor for logtail"
}

func (p *ProcessorSplitString) SplitValue(log *protocol.Log, value string) {
	if len(p.SplitKeys) == 0 {
		if p.PreserveOthers {
			if p.ExpandOthers {
				splitValues := strings.Split(value, p.SplitSep)
				for i, subVal := range splitValues {
					log.Contents = append(log.Contents, &protocol.Log_Content{Key: p.ExpandKeyPrefix + strconv.Itoa(i+1), Value: subVal})
				}
			} else {
				log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_split_preserve_", Value: value})
			}
		}
		return
	}
	splitCount := len(p.SplitKeys) + 1
	if p.ExpandOthers {
		splitCount = -1
	}
	splitValues := strings.SplitN(value, p.SplitSep, splitCount)

	if len(splitValues) < len(p.SplitKeys) {
		if p.NoMatchError {
			logger.Warning(p.context.GetRuntimeContext(), "SPLIT_LOG_ALARM", "split keys not match, split len", len(splitValues), "log", util.CutString(value, 1024))
		}
		for i, val := range splitValues {
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: p.SplitKeys[i], Value: val})
		}
		return
	}
	for i, key := range p.SplitKeys {
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: key, Value: splitValues[i]})
	}
	if len(splitValues) > len(p.SplitKeys) && p.PreserveOthers {
		if p.ExpandOthers {
			for i := len(p.SplitKeys); i < len(splitValues); i++ {
				log.Contents = append(log.Contents, &protocol.Log_Content{Key: p.ExpandKeyPrefix + strconv.Itoa(i+1-len(p.SplitKeys)), Value: splitValues[i]})
			}
		} else {
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_split_preserve_", Value: splitValues[len(p.SplitKeys)]})
		}
	}
}

func (p *ProcessorSplitString) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		findKey := false
		for i, cont := range log.Contents {
			if len(p.SourceKey) == 0 || p.SourceKey == cont.Key {
				findKey = true
				if !p.KeepSource {
					log.Contents = append(log.Contents[:i], log.Contents[i+1:]...)
				}
				// process
				p.SplitValue(log, cont.Value)
				break
			}
		}
		if !findKey && p.NoKeyError {
			logger.Warning(p.context.GetRuntimeContext(), "SPLIT_FIND_ALARM", "cannot find key", p.SourceKey)
		}
	}

	return logArray
}

func init() {
	pipeline.Processors["processor_split_string"] = func() pipeline.Processor {
		return &ProcessorSplitString{SplitSep: "\n", PreserveOthers: true}
	}
}

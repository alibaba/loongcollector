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
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
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

func (p *ProcessorSplitString) splitValue(value string, add func(key, value string)) {
	if len(p.SplitKeys) == 0 {
		if p.PreserveOthers {
			if p.ExpandOthers {
				splitValues := strings.Split(value, p.SplitSep)
				for i, subVal := range splitValues {
					add(p.ExpandKeyPrefix+strconv.Itoa(i+1), subVal)
				}
			} else {
				add("_split_preserve_", value)
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
			logger.Warning(p.context.GetRuntimeContext(), selfmonitor.SplitLogAlarm, "split keys not match, split len", len(splitValues), "log", util.CutString(value, 1024))
		}
		for i, val := range splitValues {
			add(p.SplitKeys[i], val)
		}
		return
	}
	for i, key := range p.SplitKeys {
		add(key, splitValues[i])
	}
	if len(splitValues) > len(p.SplitKeys) && p.PreserveOthers {
		if p.ExpandOthers {
			for i := len(p.SplitKeys); i < len(splitValues); i++ {
				add(p.ExpandKeyPrefix+strconv.Itoa(i+1-len(p.SplitKeys)), splitValues[i])
			}
		} else {
			add("_split_preserve_", splitValues[len(p.SplitKeys)])
		}
	}
}

func init() {
	pipeline.Processors["processor_split_string"] = func() pipeline.Processor {
		return &ProcessorSplitString{SplitSep: "\n", PreserveOthers: true}
	}
}

// Process implements the Processor interface: it splits the SourceKey value
// of each Log event into SplitKeys on the same log. Metric/Span events pass
// through unchanged.
func (p *ProcessorSplitString) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	pipeline.ProcessLogEventsOnly(in, context, p.processLogEvent)
}

func (p *ProcessorSplitString) processLogEvent(log *models.Log) {
	contents := log.GetIndices()
	sourceKey := p.SourceKey
	if sourceKey == "" {
		firstKey, ordered := models.FirstLogContentKey(contents)
		if ordered {
			sourceKey = firstKey
		} else {
			for key := range contents.Iterator() {
				sourceKey = key
				break
			}
		}
		if contents.Len() == 0 {
			if p.NoKeyError {
				logger.Warning(p.context.GetRuntimeContext(), selfmonitor.SplitFindAlarm, "cannot find key", p.SourceKey)
			}
			return
		}
	} else if !contents.Contains(sourceKey) {
		if p.NoKeyError {
			logger.Warning(p.context.GetRuntimeContext(), selfmonitor.SplitFindAlarm, "cannot find key", p.SourceKey)
		}
		return
	}

	value := pipeline.GetStringValue(contents.Get(sourceKey))
	// Remove the source first so a split field with the same key is kept.
	if !p.KeepSource {
		contents.Delete(sourceKey)
	}
	p.splitValue(value, func(key, value string) {
		contents.Add(key, value)
	})
}

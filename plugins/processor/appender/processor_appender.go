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

package appender

import (
	"fmt"
	"regexp"
	"strings"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

type ProcessorAppender struct {
	Key        string
	Value      string
	SortLabels bool

	realValue string
	context   ilogtail.Context
}

const pluginName = "processor_appender"

var replaceReg = regexp.MustCompile(`{{[^}]+}}`)

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorAppender) Init(context ilogtail.Context) error {
	if len(p.Key) == 0 || len(p.Value) == 0 {
		return fmt.Errorf("must specify Key and Value for plugin %v", pluginName)
	}
	p.context = context
	p.realValue = replaceReg.ReplaceAllStringFunc(p.Value, func(s string) string {
		return util.ParseVariableValue(s[2 : len(s)-2])
	})
	return nil
}

func (*ProcessorAppender) Description() string {
	return "processor to append some value for specific key"
}

func (p *ProcessorAppender) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		p.processLog(log)
	}
	return logArray
}

func (p *ProcessorAppender) processLog(log *protocol.Log) {
	c := p.find(log, p.Key)
	if c == nil {
		c = &protocol.Log_Content{
			Key: p.Key,
		}
		log.Contents = append(log.Contents, c)
	}
	p.processField(c)
}

func (p *ProcessorAppender) processField(c *protocol.Log_Content) {
	c.Value += p.realValue
	if p.SortLabels {
		labels := strings.Split(c.Value, "|")
		var keyValue helper.KeyValues
		for _, labelStr := range labels {
			kv := strings.SplitN(labelStr, "#$#", 2)
			if len(kv) == 2 {
				keyValue.Append(kv[0], kv[1])
			}
		}
		if keyValue.Len() > 0 {
			keyValue.Sort()
			c.Value = keyValue.String()
		}
	}
}

func (p *ProcessorAppender) find(log *protocol.Log, key string) *protocol.Log_Content {
	for idx := range log.Contents {
		if log.Contents[idx].Key == key {
			return log.Contents[idx]
		}
	}
	return nil
}

func init() {
	ilogtail.Processors[pluginName] = func() ilogtail.Processor {
		return &ProcessorAppender{}
	}
}

// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
package ratelimit

import (
	"fmt"
	"sort"
	"strings"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type ProcessorRateLimit struct {
	Fields []string `comment:"Optional. Fields of value to be limited, for each unique result from combining these field values."`
	Limit  string   `comment:"Optional. Limit rate in the format of (number)/(time unit). Supported time unit: 's' (per second), 'm' (per minute), and 'h' (per hour)."`

	Algorithm       algorithm
	limitMetric     pipeline.CounterMetric
	processedMetric pipeline.CounterMetric
	context         pipeline.Context
}

const pluginName = "processor_rate_limit"

func (p *ProcessorRateLimit) Init(context pipeline.Context) error {
	p.context = context

	limit := rate{}
	err := limit.Unpack(p.Limit)
	if err != nil {
		return err
	}
	p.Algorithm = newTokenBucket(limit)
	p.limitMetric = helper.NewCounterMetric(fmt.Sprintf("%v_limited", pluginName))
	p.context.RegisterCounterMetric(p.limitMetric)
	p.processedMetric = helper.NewCounterMetric(fmt.Sprintf("%v_processed", pluginName))
	p.context.RegisterCounterMetric(p.processedMetric)
	return nil
}

func (*ProcessorRateLimit) Description() string {
	return "rate limit processor for logtail"
}

// V1
func (p *ProcessorRateLimit) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	totalLen := len(logArray)
	nextIdx := 0
	for idx := 0; idx < totalLen; idx++ {
		key := p.makeKey(logArray[idx])
		if p.Algorithm.IsAllowed(key) {
			if idx != nextIdx {
				logArray[nextIdx] = logArray[idx]
			}
			nextIdx++
		} else {
			p.limitMetric.Add(1)
		}
		p.processedMetric.Add(1)
	}
	logArray = logArray[:nextIdx]
	return logArray
}

func (p *ProcessorRateLimit) makeKey(log *protocol.Log) string {
	if len(p.Fields) == 0 {
		return ""
	}

	sort.Strings(p.Fields)
	values := make([]string, len(p.Fields))
	for _, field := range p.Fields {
		exist := false
		for _, logContent := range log.Contents {
			if field == logContent.GetKey() {
				values = append(values, fmt.Sprintf("%v", logContent.GetValue()))
				exist = true
				break
			}
		}
		if !exist {
			values = append(values, "")
		}
	}

	return strings.Join(values, "_")
}

func init() {
	pipeline.Processors[pluginName] = func() pipeline.Processor {
		return &ProcessorRateLimit{}
	}
}

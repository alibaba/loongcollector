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

package logstorerouter

import (
	"fmt"
	"regexp"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/aggregator/baseagg"
)

const (
	MaxLogCount     = 1024
	MaxLogGroupSize = 3 * 1024 * 1024
)

// SubAgg uses reg to decide if a log belongs to it or not.
// Logs are added to agg and will be send to logstore.
type SubAgg struct {
	logstore string
	reg      *regexp.Regexp
	agg      *baseagg.AggregatorBase
}

type AggregatorRouter struct {
	MaxLogGroupCount int
	MaxLogCount      int
	PackFlag         bool
	Topic            string
	DropDisMatch     bool
	SourceKey        string
	NoMatchError     bool
	RouterRegex      []string
	RouterLogstore   []string

	subAggs    []*SubAgg
	defaultAgg *baseagg.AggregatorBase
	Lock       *sync.Mutex
	context    pipeline.Context
	queue      pipeline.LogGroupQueue
}

func (p *AggregatorRouter) Init(context pipeline.Context, que pipeline.LogGroupQueue) (int, error) {
	p.context = context
	p.queue = que
	if len(p.RouterRegex) != len(p.RouterLogstore) {
		return 0, fmt.Errorf("router regex count %d, logstore count %d", len(p.RouterRegex), len(p.RouterLogstore))
	}
	for i, regStr := range p.RouterRegex {
		regExp, err := regexp.Compile(regStr)
		if err != nil {
			return 0, fmt.Errorf("compile regex error, regex %s, error %s", regStr, err)
		}
		subAgg := &SubAgg{
			logstore: p.RouterLogstore[i],
			reg:      regExp,
			agg:      baseagg.NewAggregatorBase(),
		}
		if _, err := subAgg.agg.Init(context, que); err != nil {
			continue
		}
		subAgg.agg.InitInner(p.PackFlag, p.context.GetConfigName()+p.RouterLogstore[i]+util.GetIPAddress()+time.Now().String(), p.Lock, p.RouterLogstore[i], p.Topic, p.MaxLogCount, p.MaxLogGroupCount)
		p.subAggs = append(p.subAggs, subAgg)
	}
	if !p.DropDisMatch {
		p.defaultAgg = baseagg.NewAggregatorBase()
		if _, err := p.defaultAgg.Init(context, que); err != nil {
			return 0, err
		}
		p.defaultAgg.InitInner(p.PackFlag, p.context.GetConfigName()+util.GetIPAddress()+time.Now().String(), p.Lock, "", p.Topic, p.MaxLogCount, p.MaxLogGroupCount)
	}
	return 0, nil
}

func (*AggregatorRouter) Description() string {
	return "aggregator router for logtail"
}

func (p *AggregatorRouter) route(log *protocol.Log, value string) error {
	for _, subAgg := range p.subAggs {
		if indexArray := subAgg.reg.FindStringSubmatchIndex(value); len(indexArray) >= 2 && indexArray[0] == 0 && indexArray[1] == len(value) {
			return subAgg.agg.Add(log, nil)
		}
	}
	// no match
	if !p.DropDisMatch {
		return p.defaultAgg.Add(log, nil)
	}
	if p.NoMatchError {
		logger.Warning(p.context.GetRuntimeContext(), "NO_MATCH_ROUTER_ALARM", "no match router", "drop this log")
	}
	return nil
}

// Add adds @log to aggregator.
// If @log has a key equal to aggregator.SourceKey (or SourceKey is empty), it passes @log to
// matched sub aggregator by calling route function.
// If @log don't have specified key but aggregator.DropDisMatch is not set, it passed @log to
// default aggregator, otherwise, it returns error when aggregator.NoMatchError is set.
// Add returns any error encountered, nil means success.
func (p *AggregatorRouter) Add(log *protocol.Log, ctx map[string]interface{}) error {
	// logger.Debug("agg add", *log)

	// find log key
	for _, cont := range log.Contents {
		if len(p.SourceKey) == 0 || cont.Key == p.SourceKey {
			return p.route(log, cont.Value)
		}
	}
	// find no key
	if !p.DropDisMatch {
		return p.defaultAgg.Add(log, ctx)
	}
	if p.NoMatchError {
		logger.Warning(p.context.GetRuntimeContext(), "NO_MATCH_ROUTER_ALARM", "no match router", "drop this log")
	}
	return nil
}

func (p *AggregatorRouter) Flush() []*protocol.LogGroup {

	logGroups := []*protocol.LogGroup{}
	if !p.DropDisMatch {
		logGroups = append(logGroups, p.defaultAgg.Flush()...)
	}
	for _, subAgg := range p.subAggs {
		logGroups = append(logGroups, subAgg.agg.Flush()...)
	}
	return logGroups
}

func (p *AggregatorRouter) Reset() {
	if !p.DropDisMatch {
		p.defaultAgg.Reset()
	}
	for _, subAgg := range p.subAggs {
		subAgg.agg.Reset()
	}
}

func NewAggregatorRouter() *AggregatorRouter {
	return &AggregatorRouter{
		MaxLogGroupCount: 4,
		MaxLogCount:      MaxLogCount,
		PackFlag:         true,
		Lock:             &sync.Mutex{},
	}
}
func init() {
	pipeline.Aggregators["aggregator_logstore_router"] = func() pipeline.Aggregator {
		return NewAggregatorRouter()
	}
}

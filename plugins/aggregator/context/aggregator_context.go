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

package context

import (
	"context"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

const (
	MaxLogCount         = 1024
	MaxLogGroupSize     = 3 * 1024 * 1024
	SmallPackIDTimeout  = time.Duration(24*30) * time.Hour
	BigPackIDTimeout    = time.Duration(24) * time.Hour
	PackIDMapLenThresh  = 100000
	MaxLogGroupPoolSize = 10 * 1024 * 1024
)

type LogPackSeqInfo struct {
	seq            int64
	lastUpdateTime time.Time
}

type AggregatorContext struct {
	MaxLogGroupCount                 int    // the maximum log group count per source to trigger flush operation
	MaxLogCount                      int    // the maximum log in a log group
	Topic                            string // the output topic
	ContextPreservationToleranceSize int    // the maximum number of log source per config where logGroupPoolMap will not be cleared periodically
	PackFlag                         bool   // whether to add __pack_id__ as a tag

	lock             *sync.Mutex
	logGroupPoolMap  map[string][]*LogGroupWithSize
	logGroupPoolSize int
	packIDMap        map[string]*LogPackSeqInfo
	defaultPack      string
	context          pipeline.Context
	queue            pipeline.LogGroupQueue

	packIDMapCleanInterval time.Duration
	packIDTimeout          time.Duration
	lastCleanPackIDMapTime time.Time
}

type LogGroupWithSize struct {
	LogGroup     *protocol.LogGroup
	LogGroupSize int
}

// Init method would be trigger before working.
// 1. context store the metadata of this Logstore config
// 2. que is a transfer channel for flushing LogGroup when reaches the maximum in the cache.
func (p *AggregatorContext) Init(context pipeline.Context, que pipeline.LogGroupQueue) (int, error) {
	p.defaultPack = util.NewPackIDPrefix(context.GetConfigName())
	p.context = context
	p.queue = que
	p.lastCleanPackIDMapTime = time.Now()
	return 0, nil
}

func (*AggregatorContext) Description() string {
	return "context aggregator for logtail"
}

// addToQueueWithRetry add logGroup to queue with retry
func addToQueueWithRetry(context context.Context, queue pipeline.LogGroupQueue, logGroup *protocol.LogGroup) {
	for tryCount := 1; true; tryCount++ {
		err := queue.Add(logGroup)
		if err == nil {
			return
		}
		// wait until shutdown is active
		if tryCount%100 == 0 {
			logger.Warning(context, "AGGREGATOR_ADD_ALARM", "error", err)
		}
		time.Sleep(time.Millisecond * 10)
	}
}

// Add adds @log with @ctx to aggregator.
func (p *AggregatorContext) Add(log *protocol.Log, ctx map[string]interface{}) error {
	// when logGroupPoolMap is full flush all logGroup to queue
	if p.logGroupPoolSize > MaxLogGroupPoolSize {
		logGroups := p.Flush()
		for _, logGroup := range logGroups {
			if len(logGroup.Logs) == 0 {
				continue
			}
			addToQueueWithRetry(p.context.GetRuntimeContext(), p.queue, logGroup)
		}
	}
	p.lock.Lock()
	defer p.lock.Unlock()

	source := p.defaultPack
	if _, ok := ctx["source"]; ok {
		source = ctx["source"].(string)
		if !strings.HasSuffix(source, "-") {
			source += "-"
		}
	}

	topic := p.Topic
	if _, ok := ctx["topic"]; ok {
		topic = ctx["topic"].(string)
	}

	logGroupList := p.logGroupPoolMap[source]
	if logGroupList == nil {
		logGroupList = make([]*LogGroupWithSize, 0, p.MaxLogGroupCount)
	}
	if len(logGroupList) == 0 {
		if _, ok := ctx["tags"]; ok {
			newLogGroup := p.newLogGroupWithSize(source, topic)
			fillTags(ctx["tags"].([]*protocol.LogTag), newLogGroup)
			logGroupList = append(logGroupList, newLogGroup)
		} else {
			logGroupList = append(logGroupList, p.newLogGroupWithSize(source, topic))
		}
	}
	nowLogGroup := logGroupList[len(logGroupList)-1]

	logSize := p.evaluateLogSize(log)
	// When current log group is full (log count or no more capacity for current log),
	// allocate a new log group.
	if len(nowLogGroup.LogGroup.Logs) >= p.MaxLogCount || nowLogGroup.LogGroupSize+logSize > MaxLogGroupSize {
		// The number of log group exceeds limit, make a quick flush.
		if len(logGroupList) == p.MaxLogGroupCount {
			// Quick flush to avoid becoming bottleneck when large logs come.
			if err := p.queue.Add(logGroupList[0].LogGroup); err == nil {
				// add success, remove head log group
				p.logGroupPoolSize -= logGroupList[0].LogGroupSize
				if p.logGroupPoolSize < 0 {
					p.logGroupPoolSize = 0
				}
				logGroupList = logGroupList[1:]
			} else {
				return err
			}
		}
		// New log group, reset size.
		if _, ok := ctx["tags"]; ok {
			newLogGroup := p.newLogGroupWithSize(source, topic)
			fillTags(ctx["tags"].([]*protocol.LogTag), newLogGroup)
			logGroupList = append(logGroupList, newLogGroup)
		} else {
			logGroupList = append(logGroupList, p.newLogGroupWithSize(source, topic))
		}
		nowLogGroup = logGroupList[len(logGroupList)-1]
	}

	// add log size
	p.logGroupPoolSize += logSize
	nowLogGroup.LogGroupSize += logSize
	nowLogGroup.LogGroup.Logs = append(nowLogGroup.LogGroup.Logs, log)
	p.logGroupPoolMap[source] = logGroupList
	return nil
}

func fillTags(logTags []*protocol.LogTag, logGroupWithSize *LogGroupWithSize) {
	logGroupWithSize.LogGroup.LogTags = append(logGroupWithSize.LogGroup.LogTags, logTags...)
}

// Flush ...
func (p *AggregatorContext) Flush() []*protocol.LogGroup {
	p.lock.Lock()
	defer p.lock.Unlock()
	var ret []*protocol.LogGroup
	for pack, logGroupList := range p.logGroupPoolMap {
		if len(logGroupList) == 0 {
			continue
		}
		for _, logGroup := range logGroupList {
			if len(logGroup.LogGroup.Logs) == 0 {
				continue
			}
			ret = append(ret, logGroup.LogGroup)
		}
		delete(p.logGroupPoolMap, pack)
	}
	p.logGroupPoolSize = 0

	curTime := time.Now()
	if len(p.packIDMap) > p.ContextPreservationToleranceSize && time.Since(p.lastCleanPackIDMapTime) > p.packIDMapCleanInterval {
		if len(p.packIDMap) > PackIDMapLenThresh {
			p.packIDTimeout = BigPackIDTimeout
		} else {
			p.packIDTimeout = SmallPackIDTimeout
		}
		for k, v := range p.packIDMap {
			if curTime.Sub(v.lastUpdateTime) > p.packIDTimeout {
				delete(p.packIDMap, k)
			}
		}
		p.lastCleanPackIDMapTime = curTime
	}

	return ret
}

// Reset ...
func (p *AggregatorContext) Reset() {
	p.lock.Lock()
	defer p.lock.Unlock()
	p.logGroupPoolSize = 0
	p.logGroupPoolMap = make(map[string][]*LogGroupWithSize)
}

func (p *AggregatorContext) newLogGroupWithSize(pack string, topic string) *LogGroupWithSize {
	logGroupWithSize := LogGroupWithSize{
		LogGroup: &protocol.LogGroup{
			Logs:  make([]*protocol.Log, 0, p.MaxLogCount),
			Topic: topic,
		},
		LogGroupSize: 0,
	}
	info := p.packIDMap[pack]
	if info == nil {
		info = &LogPackSeqInfo{
			seq: 1,
		}
	}
	if p.PackFlag {
		logGroupWithSize.LogGroup.LogTags = append(logGroupWithSize.LogGroup.LogTags, util.NewLogTagForPackID(pack, &info.seq))
	}
	info.lastUpdateTime = time.Now()
	p.packIDMap[pack] = info

	return &logGroupWithSize
}

func (*AggregatorContext) evaluateLogSize(log *protocol.Log) int {
	var logSize = 6
	for _, logC := range log.Contents {
		logSize += 5 + len(logC.Key) + len(logC.Value)
	}
	return logSize
}

// NewAggregatorContext create a default aggregator with default value.
func NewAggregatorContext() *AggregatorContext {
	return &AggregatorContext{
		MaxLogGroupCount:                 2,
		MaxLogCount:                      MaxLogCount,
		ContextPreservationToleranceSize: 10,
		PackFlag:                         true,
		logGroupPoolMap:                  make(map[string][]*LogGroupWithSize),
		packIDMap:                        make(map[string]*LogPackSeqInfo),
		packIDMapCleanInterval:           time.Duration(600) * time.Second,
		lock:                             &sync.Mutex{},
		logGroupPoolSize:                 0,
	}
}

// Register the plugin to the Aggregators array.
func init() {
	pipeline.Aggregators["aggregator_context"] = func() pipeline.Aggregator {
		return NewAggregatorContext()
	}
}

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

package baseagg

import (
	"sync"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

const (
	MaxLogCount     = 1024
	MaxLogGroupSize = 3 * 1024 * 1024
)

// Other aggregators can use AggregatorBase as base aggregator.
//
// For inner usage, note about following information.
// There is a quick flush design in AggregatorBase, which is implemented
// in Add method (search p.queue.Add in current file). Therefore, not all
// LogGroups are returned through Flush method.
// If you want to do some operations (such as adding tags) on LogGroups returned
// by AggregatorBase in your own aggregator, you should do some extra works,
// just see the sample code in doc.go.
type AggregatorBase struct {
	MaxLogGroupCount int    // the maximum log group count to trigger flush operation
	MaxLogCount      int    // the maximum log in a log group
	PackFlag         bool   // whether to add config name as a tag
	Topic            string // the output topic

	pack            string
	defaultLogGroup []*protocol.LogGroup
	packID          int64
	logstore        string
	Lock            *sync.Mutex
	context         pipeline.Context
	queue           pipeline.LogGroupQueue
	nowLoggroupSize int
}

// Init method would be trigger before working.
// 1. context store the metadata of this Logstore config
// 2. que is a transfer channel for flushing LogGroup when reaches the maximum in the cache.
func (p *AggregatorBase) Init(context pipeline.Context, que pipeline.LogGroupQueue) (int, error) {
	p.context = context
	p.queue = que
	if p.PackFlag {
		p.pack = util.NewPackIDPrefix(context.GetConfigName())
	}
	return 0, nil
}

func (*AggregatorBase) Description() string {
	return "base aggregator for logtail"
}

func (*AggregatorBase) evaluateLogSize(log *protocol.Log) int {
	var logSize = 6
	for _, logC := range log.Contents {
		logSize += 5 + len(logC.Key) + len(logC.Value)
	}
	return logSize
}

// Add adds @log to aggregator.
// It uses defaultLogGroup to store log groups which contain logs as following:
// defaultLogGroup => [LG1: log1->log2->log3] -> [LG2: log1->log2->log3] -> ..
// The last log group is set as nowLogGroup, @log will be appended to nowLogGroup
// if the size and log count of the log group don't exceed limits (MaxLogCount and
// MAX_LOG_GROUP_SIZE).
// When nowLogGroup exceeds limits, Add creates a new log group and switch nowLogGroup
// to it, then append @log to it.
// When the count of log group reaches MaxLogGroupCount, the first log group will
// be popped from defaultLogGroup list and add to queue (after adding pack_id tag).
// Add returns any error encountered, nil means success.
//
// @return error. **For inner usage, must handle this error!!!!**
func (p *AggregatorBase) Add(log *protocol.Log, ctx map[string]interface{}) error {
	p.Lock.Lock()
	defer p.Lock.Unlock()
	if len(p.defaultLogGroup) == 0 {
		p.nowLoggroupSize = 0
		p.defaultLogGroup = append(p.defaultLogGroup, &protocol.LogGroup{
			Logs: make([]*protocol.Log, 0, p.MaxLogCount),
		})
	}
	nowLogGroup := p.defaultLogGroup[len(p.defaultLogGroup)-1]
	logSize := p.evaluateLogSize(log)

	// When current log group is full (log count or no more capacity for current log),
	// allocate a new log group.
	if len(nowLogGroup.Logs) >= p.MaxLogCount || p.nowLoggroupSize+logSize > MaxLogGroupSize {
		// The number of log group exceeds limit, make a quick flush.
		if len(p.defaultLogGroup) == p.MaxLogGroupCount {
			// try to send
			p.addPackID(p.defaultLogGroup[0])
			p.defaultLogGroup[0].Category = p.logstore
			if len(p.Topic) > 0 {
				p.defaultLogGroup[0].Topic = p.Topic
			}

			// Quick flush to avoid becoming bottleneck when large logs come.
			if err := p.queue.Add(p.defaultLogGroup[0]); err == nil {
				// add success, remove head log group
				p.defaultLogGroup = p.defaultLogGroup[1:]
			} else {
				return err
			}
		}
		// New log group, reset size.
		p.nowLoggroupSize = 0
		p.defaultLogGroup = append(p.defaultLogGroup, &protocol.LogGroup{
			Logs: make([]*protocol.Log, 0, p.MaxLogCount),
		})
		nowLogGroup = p.defaultLogGroup[len(p.defaultLogGroup)-1]
	}

	// add log size
	p.nowLoggroupSize += logSize
	nowLogGroup.Logs = append(nowLogGroup.Logs, log)
	return nil
}

// addPackID adds __pack_id__ into logGroup.LogTags if it is not existing at last.
func (p *AggregatorBase) addPackID(logGroup *protocol.LogGroup) {
	if !p.PackFlag {
		return
	}
	if len(logGroup.LogTags) == 0 || logGroup.LogTags[len(logGroup.LogTags)-1].GetKey() != util.PackIDTagKey {
		logGroup.LogTags = append(logGroup.LogTags, util.NewLogTagForPackID(p.pack, &p.packID))
	}
}

// Flush ...
func (p *AggregatorBase) Flush() []*protocol.LogGroup {
	p.Lock.Lock()
	if len(p.defaultLogGroup) == 0 {
		p.Lock.Unlock()
		return nil
	}
	p.nowLoggroupSize = 0
	logGroupList := p.defaultLogGroup
	p.defaultLogGroup = make([]*protocol.LogGroup, 0, p.MaxLogGroupCount)
	p.Lock.Unlock()

	for i, logGroup := range logGroupList {
		// check if last is empty
		if len(logGroup.Logs) == 0 {
			logGroupList = logGroupList[0:i]
			break
		}
		p.addPackID(logGroup)
		logGroup.Category = p.logstore
		if len(p.Topic) > 0 {
			logGroup.Topic = p.Topic
		}
	}
	return logGroupList
}

// Reset ...
func (p *AggregatorBase) Reset() {
	p.Lock.Lock()
	defer p.Lock.Unlock()
	p.defaultLogGroup = make([]*protocol.LogGroup, 0)
}

// InitInner initializes instance for other aggregators.
func (p *AggregatorBase) InitInner(packFlag bool, packString string, lock *sync.Mutex, logstore string, topic string, maxLogCount int, maxLoggroupCount int) {
	p.PackFlag = packFlag
	p.MaxLogCount = maxLogCount
	p.MaxLogGroupCount = maxLoggroupCount
	p.Lock = lock
	p.Topic = topic
	p.logstore = logstore
	if p.PackFlag {
		p.pack = util.NewPackIDPrefix(packString)
	}
}

func (p *AggregatorBase) Record(event *models.PipelineGroupEvents, ctx pipeline.PipelineContext) error {
	ctx.Collector().CollectList(event)
	return nil
}

// GetResult the current aggregates to the accumulator.
func (p *AggregatorBase) GetResult(ctx pipeline.PipelineContext) error {
	return nil
}

// NewAggregatorBase create a default aggregator with default value.
func NewAggregatorBase() *AggregatorBase {
	return &AggregatorBase{
		defaultLogGroup:  make([]*protocol.LogGroup, 0),
		MaxLogGroupCount: 4,
		MaxLogCount:      MaxLogCount,
		PackFlag:         true,
		Lock:             &sync.Mutex{},
	}
}

// Register the plugin to the Aggregators array.
func init() {
	pipeline.Aggregators["aggregator_base"] = func() pipeline.Aggregator {
		return NewAggregatorBase()
	}
}

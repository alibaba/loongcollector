// Copyright 2022 iLogtail Authors
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

package contentvaluegroup

import (
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/aggregator/baseagg"
)

const groupKeyConnector = "_"

type groupAggregator struct {
	groupKVs map[string]string
	agg      *baseagg.AggregatorBase
}

func (g *groupAggregator) Add(log *protocol.Log, ctx map[string]interface{}) error {
	return g.agg.Add(log, ctx)
}

func (g *groupAggregator) Flush() []*protocol.LogGroup {
	logGroups := g.agg.Flush()
	for _, logGroup := range logGroups {
		addGroupTags(logGroup, g.groupKVs)
	}
	return logGroups
}

type groupQueue struct {
	groupKVs map[string]string
	queue    pipeline.LogGroupQueue
}

// Add ...
func (q *groupQueue) Add(logGroup *protocol.LogGroup) error {
	addGroupTags(logGroup, q.groupKVs)
	return q.queue.Add(logGroup)
}

// AddWithWait ...
func (q *groupQueue) AddWithWait(logGroup *protocol.LogGroup, duration time.Duration) error {
	addGroupTags(logGroup, q.groupKVs)
	return q.queue.AddWithWait(logGroup, duration)
}

type AggregatorContentValueGroup struct {
	GroupKeys        []string // The Keys to group by
	Topic            string   // Topic to attach, if set
	ErrIfKeyNotFound bool     // If logging when group-key not found
	EnablePackID     bool     // If attach PackID

	groupAggs sync.Map
	lock      *sync.Mutex
	context   pipeline.Context
	queue     pipeline.LogGroupQueue
}

// Description ...
func (*AggregatorContentValueGroup) Description() string {
	return "aggregator that group logs by a set of keys"
}

func (g *AggregatorContentValueGroup) Init(context pipeline.Context, que pipeline.LogGroupQueue) (int, error) {
	g.context = context
	g.queue = que

	if len(g.GroupKeys) == 0 {
		return 0, fmt.Errorf("must specify GroupKeys")
	}

	return 0, nil
}

func (g *AggregatorContentValueGroup) Add(log *protocol.Log, ctx map[string]interface{}) error {
	agg, err := g.getOrCreateGroupAggs(log)
	if err != nil {
		return err
	}
	return agg.Add(log, ctx)
}

func (g *AggregatorContentValueGroup) Flush() []*protocol.LogGroup {
	var logGroups []*protocol.LogGroup
	g.groupAggs.Range(func(key, value any) bool {
		agg := value.(*groupAggregator)
		logGroups = append(logGroups, agg.Flush()...)
		return true
	})
	return logGroups
}

func (g *AggregatorContentValueGroup) Reset() {
	g.groupAggs.Range(func(key, value any) bool {
		agg := value.(*groupAggregator)
		agg.agg.Reset()
		return true
	})
}

func (g *AggregatorContentValueGroup) getOrCreateGroupAggs(log *protocol.Log) (*groupAggregator, error) {
	groupKey := g.buildGroupKey(log)
	groupAgg, ok := g.groupAggs.Load(groupKey)
	if ok {
		return groupAgg.(*groupAggregator), nil
	}

	groupKVs := g.getGroupKVs(log)
	groupQueue := &groupQueue{groupKVs: groupKVs, queue: g.queue}

	agg := baseagg.NewAggregatorBase()
	if _, err := agg.Init(g.context, groupQueue); err != nil {
		logger.Error(g.context.GetRuntimeContext(), "AGG_GROUP_ALARM", "aggregator group fail to create agg for group", groupKVs)
		return nil, err
	}
	agg.InitInner(
		g.EnablePackID,
		util.NewPackIDPrefix(g.context.GetConfigName()+groupKey),
		g.lock,
		"",
		g.Topic,
		1024,
		4)

	groupAgg = &groupAggregator{groupKVs: groupKVs, agg: agg}
	groupAgg, _ = g.groupAggs.LoadOrStore(groupKey, groupAgg)
	return groupAgg.(*groupAggregator), nil
}

func (g *AggregatorContentValueGroup) findLogContent(log *protocol.Log, key string) (value string, ok bool) {
	for _, cont := range log.Contents {
		if cont.Key == key {
			value = cont.Value
			ok = true
			break
		}
	}
	return
}

func (g *AggregatorContentValueGroup) buildGroupKey(log *protocol.Log) string {
	var groupKey strings.Builder
	for idx, key := range g.GroupKeys {
		val, _ := g.findLogContent(log, key)
		if idx == 0 {
			groupKey.WriteString(val)
		} else {
			groupKey.WriteString(groupKeyConnector)
			groupKey.WriteString(val)
		}
	}
	return groupKey.String()
}

func (g *AggregatorContentValueGroup) getGroupKVs(log *protocol.Log) map[string]string {
	group := make(map[string]string, len(g.GroupKeys))
	for _, key := range g.GroupKeys {
		val, found := g.findLogContent(log, key)
		if !found && g.ErrIfKeyNotFound {
			logger.Warning(g.context.GetRuntimeContext(), "AGG_GROUP_ALARM", "aggregator group fail to find key in log content,key", key)
		}
		group[key] = val
	}
	return group
}

func addGroupTags(logGroup *protocol.LogGroup, tagKVs map[string]string) {
	for k, v := range tagKVs {
		for _, tag := range logGroup.LogTags {
			if tag.Key == k {
				// if any k exists already, just return
				return
			}
		}

		logGroup.LogTags = append(logGroup.LogTags, &protocol.LogTag{
			Key:   k,
			Value: v,
		})
	}
}

func init() {
	pipeline.Aggregators["aggregator_content_value_group"] = func() pipeline.Aggregator {
		return &AggregatorContentValueGroup{
			EnablePackID:     true,
			ErrIfKeyNotFound: false,
			lock:             &sync.Mutex{},
		}
	}
}

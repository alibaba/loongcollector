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

package aggregatorshardhash

import (
	"fmt"
	"math/big"
	"math/bits"
	"strconv"
	"strings"

	"github.com/cespare/xxhash/v2"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/aggregator/baseagg"
)

const (
	defaultShardCount = 8
	maxShardCount     = 512
	pluginType        = "aggregator_shardhash"
)

type AggregatorShardHash struct {
	SourceKeys       []string
	ShardCount       int
	Topic            string
	Connector        string
	ErrIfKeyNotFound bool
	EnablePackID     bool

	shardHashes []string
	context     pipeline.Context
	batches     baseagg.Batcher
}

func (s *AggregatorShardHash) Init(ctx pipeline.Context) (int, error) {
	if len(s.SourceKeys) == 0 {
		return 0, fmt.Errorf("plugin %s must specify SourceKeys", pluginType)
	}
	if s.ShardCount <= 0 || s.ShardCount > maxShardCount {
		return 0, fmt.Errorf("invalid ShardCount: %d, range [1, %d]", s.ShardCount, maxShardCount)
	}
	if s.ShardCount&(s.ShardCount-1) != 0 {
		return 0, fmt.Errorf("invalid ShardCount: %d, must be power of 2", s.ShardCount)
	}
	s.context = ctx
	s.shardHashes = make([]string, s.ShardCount)
	bucketBits := bits.Len(uint(s.ShardCount - 1))
	for i := range s.shardHashes {
		hash := big.NewInt(int64(i))
		hash.Lsh(hash, uint(128-bucketBits))
		hexHash := hash.Text(16)
		s.shardHashes[i] = hexHash + strings.Repeat("0", 32-len(hexHash))
	}
	return 0, s.batches.Init(ctx, baseagg.MaxLogCount, 4, s.EnablePackID)
}

func (*AggregatorShardHash) Description() string {
	return "aggregator that generates shard hashes for logs"
}

func (s *AggregatorShardHash) Record(in *models.PipelineGroupEvents, ctx pipeline.PipelineContext) error {
	if in == nil {
		return nil
	}
	shards := make(map[int][]models.PipelineEvent)
	var passThrough []models.PipelineEvent
	for _, event := range in.Events {
		shard, ok := s.shard(event)
		if !ok {
			passThrough = append(passThrough, event)
			continue
		}
		shards[shard] = append(shards[shard], event)
	}
	for shard, events := range shards {
		tags := map[string]string{util.ShardHashTagKey: s.shardHashes[shard]}
		s.batches.CollectPartition(ctx, strconv.Itoa(shard), baseagg.GroupForRoute(in.Group, "", s.Topic, tags), events)
	}
	ctx.Collector().Collect(in.Group, passThrough...)
	return nil
}

func (s *AggregatorShardHash) shard(event models.PipelineEvent) (int, bool) {
	var source strings.Builder
	isLog := event.GetType() == models.EventTypeLogging
	var hasField bool
	for i, key := range s.SourceKeys {
		if i > 0 {
			source.WriteString(s.Connector)
		}
		if value, ok := baseagg.FindEventField(event, key); ok {
			hasField = true
			source.WriteString(value)
		} else if isLog && s.ErrIfKeyNotFound {
			logger.Warning(s.context.GetRuntimeContext(), selfmonitor.AggShardhashNotFoundKey, key)
		}
	}
	if !isLog && !hasField {
		return 0, false
	}
	return int(xxhash.Sum64String(source.String()) % uint64(s.ShardCount)), true
}

func (s *AggregatorShardHash) GetResult(ctx pipeline.PipelineContext) error {
	s.batches.CollectPending(ctx)
	return nil
}

func (s *AggregatorShardHash) Reset() {
	s.batches.Reset()
}

func newAggregatorShardHash() *AggregatorShardHash {
	return &AggregatorShardHash{
		Connector:    "_",
		ShardCount:   defaultShardCount,
		EnablePackID: true,
	}
}

func init() {
	pipeline.Aggregators[pluginType] = func() pipeline.Aggregator {
		return newAggregatorShardHash()
	}
}

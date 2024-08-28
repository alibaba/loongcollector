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

package aggregatorshardhash

import (
	"fmt"
	"math/big"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/cespare/xxhash/v2"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/aggregator/baseagg"
)

const (
	defaultShardCount = 8
	maxShardCount     = 512
	pluginType        = "aggregator_shardhash"
)

// shardAggregator decides which agg (log group) the log belongs to.
type shardAggregator struct {
	md5 string
	agg *baseagg.AggregatorBase
}

// Use an inner queue so that we can append shard hash into log groups' tags.
type innerQueue struct {
	// Related aggregator.
	agg *AggregatorShardHash
}

// Add ...
func (q *innerQueue) Add(logGroup *protocol.LogGroup) error {
	q.agg.update(logGroup)
	return q.agg.queue.Add(logGroup)
}

// AddWithWait ...
func (q *innerQueue) AddWithWait(logGroup *protocol.LogGroup, duration time.Duration) error {
	q.agg.update(logGroup)
	return q.agg.queue.AddWithWait(logGroup, duration)
}

// AggregatorShardHash ...
// ShardCount is the power of 2, so that we can use bits to represent buckets.
// For example, 4 needs 2 bits to represent 3 bucket, so 00 to 11 are enough.
type AggregatorShardHash struct {
	SourceKeys       []string
	ShardCount       int
	Topic            string
	Connector        string
	ErrIfKeyNotFound bool
	EnablePackID     bool

	bucketBits int
	// BucketID -> Shard.
	shardAggs []*shardAggregator

	// For __pack_id__ when EnablePackID is true.
	packIDPrefix string
	packIDSeqNum int64

	lock       *sync.Mutex
	context    pipeline.Context
	queue      pipeline.LogGroupQueue
	innerQueue *innerQueue
}

func isPowerOfTwo(num int) bool {
	return num > 0 && ((num & (num - 1)) == 0)
}

func (s *AggregatorShardHash) Init(context pipeline.Context, que pipeline.LogGroupQueue) (int, error) {
	s.context = context
	s.queue = que

	if len(s.SourceKeys) == 0 {
		return 0, fmt.Errorf("plugin %v must specify SourceKeys", pluginType)
	}
	if s.ShardCount <= 0 || s.ShardCount > maxShardCount {
		return 0, fmt.Errorf("invalid ShardCount: %v, range [1, %v]",
			s.ShardCount, maxShardCount)
	}
	if !isPowerOfTwo(s.ShardCount) {
		return 0, fmt.Errorf("invalid ShardCount: %v, must be power of 2", s.ShardCount)
	}

	if s.EnablePackID {
		s.packIDPrefix = util.NewPackIDPrefix(context.GetConfigName())
		s.packIDSeqNum = 0
	}

	s.innerQueue = &innerQueue{agg: s}
	s.bucketBits = len(strconv.FormatInt(int64(s.ShardCount-1), 2))
	return s.initShardAggs()
}

func (s *AggregatorShardHash) initShardAggs() (int, error) {
	s.shardAggs = make([]*shardAggregator, s.ShardCount)
	for idx := 0; idx < s.ShardCount; idx++ {
		bucketBinaryID := fmt.Sprintf("%b", idx)
		if len(bucketBinaryID) < s.bucketBits {
			bucketBinaryID = strings.Repeat("0", s.bucketBits-len(bucketBinaryID)) + bucketBinaryID
		}

		binaryHash := bucketBinaryID + strings.Repeat("0", 128-s.bucketBits)
		bigInt := big.NewInt(0)
		bigInt.SetString(binaryHash, 2)
		hexHash := bigInt.Text(16)
		if len(hexHash) < 32 {
			hexHash += strings.Repeat("0", 32-len(hexHash))
		}
		shardAgg := &shardAggregator{
			md5: hexHash,
			agg: baseagg.NewAggregatorBase(),
		}
		if _, err := shardAgg.agg.Init(s.context, s.innerQueue); err != nil {
			return 0, fmt.Errorf("initialize %vth shard agg error: %v", idx, err)
		}
		shardAgg.agg.InitInner(false,
			"",
			s.lock,
			"",
			strconv.Itoa(idx), // Use bucketID to replace Topic so that we can trace it.
			1024,
			4)
		s.shardAggs[idx] = shardAgg
	}
	return 0, nil
}

// Description ...
func (*AggregatorShardHash) Description() string {
	return "aggregator that generate shard hash for logs"
}

func (s *AggregatorShardHash) selectShardAgg(sourceValue string) *shardAggregator {
	h := xxhash.Sum64([]byte(sourceValue))
	bucketID := uint(h) % uint(s.ShardCount)
	return s.shardAggs[bucketID]
}

// Add ...
func (s *AggregatorShardHash) Add(log *protocol.Log, ctx map[string]interface{}) error {
	var sourceValue string
	for idx, key := range s.SourceKeys {
		var val string
		found := false
		for _, cont := range log.Contents {
			if cont.Key == key {
				val = cont.Value
				found = true
				break
			}
		}
		if !found && s.ErrIfKeyNotFound {
			logger.Warning(s.context.GetRuntimeContext(), "AGG_SHARDHASH_NOT_FOUND_KEY", key)
		}

		isFirstKey := 0 == idx
		if isFirstKey {
			sourceValue = val
		} else {
			sourceValue += s.Connector + val
		}
	}

	return s.selectShardAgg(sourceValue).agg.Add(log, ctx)
}

// update resets topic and appends tags (shardHash, pack ID) if they are not existing.
// An updated logGroup might be called to update again when quick flush Add has failed, so
// we must check before appending tags.
func (s *AggregatorShardHash) update(logGroup *protocol.LogGroup) {
	for _, tag := range logGroup.LogTags {
		if tag.Key == util.ShardHashTagKey {
			return
		}
	}

	bucketID, _ := strconv.Atoi(logGroup.GetTopic())
	logGroup.LogTags = append(logGroup.LogTags, &protocol.LogTag{
		Key:   util.ShardHashTagKey,
		Value: s.shardAggs[bucketID].md5,
	})
	if s.EnablePackID {
		logGroup.LogTags = append(logGroup.LogTags, util.NewLogTagForPackID(s.packIDPrefix, &s.packIDSeqNum))
	}
	logGroup.Topic = s.Topic
}

func (s *AggregatorShardHash) Flush() []*protocol.LogGroup {
	var logGroups []*protocol.LogGroup
	for _, shardAgg := range s.shardAggs {
		logGroups = append(logGroups, shardAgg.agg.Flush()...)
	}
	for _, logGroup := range logGroups {
		s.update(logGroup)
	}
	return logGroups
}

func (s *AggregatorShardHash) Reset() {
	for _, shardAgg := range s.shardAggs {
		shardAgg.agg.Reset()
	}
}

func newAggregatorShardHash() *AggregatorShardHash {
	return &AggregatorShardHash{
		Connector:        "_",
		ShardCount:       defaultShardCount,
		ErrIfKeyNotFound: false,
		EnablePackID:     true,
		lock:             &sync.Mutex{},
	}
}

func init() {
	pipeline.Aggregators[pluginType] = func() pipeline.Aggregator {
		return newAggregatorShardHash()
	}
}

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
	"crypto/md5" //nolint:gosec
	"fmt"
	"hash/fnv"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/cespare/xxhash/v2"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
	pluginmanager "github.com/alibaba/ilogtail/pluginmanager"
)

func TestInitShardAggs(t *testing.T) {
	s := newAggregatorShardHash()
	s.ShardCount = 8
	s.SourceKeys = []string{"test"}
	ctx := &pluginmanager.ContextImp{}
	ctx.InitContext("project", "logstore", "config")
	_, err := s.Init(ctx, nil)
	require.NoError(t, err)

	require.Equal(t, s.bucketBits, 3)
	require.Equal(t, len(s.shardAggs), 8)
	bucketIDs := make([]int, s.ShardCount)
	for i := 0; i < s.ShardCount; i++ {
		bucketIDs[i] = i
	}
	hexHashes := []string{"0", "2", "4", "6", "8", "a", "c", "e"}
	for idx, id := range bucketIDs {
		agg := s.shardAggs[id]
		require.NotNil(t, agg, "%v is not existing", id)
		require.Equal(t, agg.md5, hexHashes[idx]+strings.Repeat("0", 31))
	}
}

func TestSelectShardAggFrom8Shard(t *testing.T) {
	testCases := []struct {
		Value    string
		BucketID int // xxhash(Value) % ShardCount
	}{
		{
			Value:    "hello",
			BucketID: 3,
		},
		{
			Value:    "hello ",
			BucketID: 2,
		},
		{
			Value:    "hello w",
			BucketID: 2,
		},
		{
			Value:    "hello wo",
			BucketID: 5,
		},
		{
			Value:    "hello wor",
			BucketID: 3,
		},
		{
			Value:    "hello worl",
			BucketID: 1,
		},
		{
			Value:    "hello world",
			BucketID: 0,
		},
	}

	s := newAggregatorShardHash()
	s.ShardCount = 8
	s.SourceKeys = []string{"test"}
	ctx := &pluginmanager.ContextImp{}
	ctx.InitContext("project", "logstore", "config")
	_, err := s.Init(ctx, nil)
	require.NoError(t, err)

	for tcIdx, tc := range testCases {
		require.Equal(t, s.selectShardAgg(tc.Value), s.shardAggs[tc.BucketID],
			"TestCase %v", tcIdx)
	}
}

func TestSelectShardAggFrom16Shard(t *testing.T) {
	testCases := []struct {
		Value    string
		BucketID int
	}{
		{
			Value:    "hello",
			BucketID: 3,
		},
		{
			Value:    "hello ",
			BucketID: 2,
		},
		{
			Value:    "hello w",
			BucketID: 10,
		},
		{
			Value:    "hello wo",
			BucketID: 5,
		},
		{
			Value:    "hello wor",
			BucketID: 3,
		},
		{
			Value:    "hello worl",
			BucketID: 9,
		},
		{
			Value:    "hello world",
			BucketID: 8,
		},
	}

	s := newAggregatorShardHash()
	s.ShardCount = 16
	s.SourceKeys = []string{"test"}
	ctx := &pluginmanager.ContextImp{}
	ctx.InitContext("project", "logstore", "config")
	_, err := s.Init(ctx, nil)
	require.NoError(t, err)

	for tcIdx, tc := range testCases {
		require.Equal(t, s.selectShardAgg(tc.Value), s.shardAggs[tc.BucketID],
			"TestCase %v", tcIdx)
	}
}

func TestSelectShardAggFrom512Shard(t *testing.T) {
	testCases := []struct {
		Value    string
		BucketID int
	}{
		{
			Value:    "hello",
			BucketID: 419,
		},
		{
			Value:    "hello ",
			BucketID: 466,
		},
		{
			Value:    "hello w",
			BucketID: 250,
		},
		{
			Value:    "hello wo",
			BucketID: 501,
		},
		{
			Value:    "hello wor",
			BucketID: 163,
		},
		{
			Value:    "hello worl",
			BucketID: 329,
		},
		{
			Value:    "hello world",
			BucketID: 360,
		},
	}

	s := newAggregatorShardHash()
	s.ShardCount = 512
	s.SourceKeys = []string{"test"}
	ctx := &pluginmanager.ContextImp{}
	ctx.InitContext("project", "logstore", "config")
	_, err := s.Init(ctx, nil)
	require.NoError(t, err)

	for tcIdx, tc := range testCases {
		require.Equal(t, s.selectShardAgg(tc.Value), s.shardAggs[tc.BucketID],
			"TestCase %v", tcIdx)
	}
}

type TestQueue struct {
	logGroups []*protocol.LogGroup
}

func (q *TestQueue) Add(logGroup *protocol.LogGroup) error {
	q.logGroups = append(q.logGroups, logGroup)
	return nil
}

func (q *TestQueue) AddWithWait(logGroup *protocol.LogGroup, duration time.Duration) error {
	q.logGroups = append(q.logGroups, logGroup)
	return nil
}

func TestInnerQueue(t *testing.T) {
	s := newAggregatorShardHash()
	s.ShardCount = 8
	s.SourceKeys = []string{"test"}
	s.Topic = "topic"
	ctx := &pluginmanager.ContextImp{}
	ctx.InitContext("project", "logstore", "config")
	q := &TestQueue{}
	_, err := s.Init(ctx, q)
	require.NoError(t, err)

	testCases := []struct {
		Value    string
		BucketID int // xxhash(Value) % ShardCount
	}{
		{
			Value:    "hello",
			BucketID: 3,
		},
		{
			Value:    "hello ",
			BucketID: 2,
		},
		{
			Value:    "hello w",
			BucketID: 2,
		},
		{
			Value:    "hello wo",
			BucketID: 5,
		},
		{
			Value:    "hello wor",
			BucketID: 3,
		},
		{
			Value:    "hello worl",
			BucketID: 1,
		},
		{
			Value:    "hello world",
			BucketID: 0,
		},
	}

	for _, tc := range testCases {
		log := &protocol.Log{}
		log.Contents = append(log.Contents, &protocol.Log_Content{
			Key: "test", Value: tc.Value,
		})
		require.NoError(t, s.Add(log, nil))
	}

	logGroups := s.Flush()
	require.Equal(t, len(logGroups), 5, "%v", logGroups)
	for _, logGroup := range logGroups {
		require.Equal(t, logGroup.Topic, "topic", "%v", logGroup)
		require.NotEmpty(t, logGroup.LogTags, "%v", logGroup)
		var tag *protocol.LogTag
		for _, logTag := range logGroup.LogTags {
			if logTag.Key == util.ShardHashTagKey {
				tag = logTag
			}
		}
		require.NotNil(t, tag)
		require.Equal(t, tag.Key, util.ShardHashTagKey)
		require.Equal(t, tag.Value, s.selectShardAgg(logGroup.Logs[0].Contents[0].Value).md5)
	}
}

func TestMultipleSourceKeys(t *testing.T) {
	s := newAggregatorShardHash()
	s.ShardCount = 8
	s.SourceKeys = []string{"k1", "k2", "k3"}
	s.Connector = "+"
	ctx := &pluginmanager.ContextImp{}
	ctx.InitContext("project", "logstore", "config")
	q := &TestQueue{}
	_, err := s.Init(ctx, q)
	require.NoError(t, err)

	testCases := []struct {
		Values []string
		Key    string
	}{
		{Values: []string{"v1", "v2", "v3"}, Key: "v1+v2+v3"},
		{Values: []string{"v11", "v22", "v33"}, Key: "v11+v22+v33"},
		{Values: []string{"v111", "v222", "v333"}, Key: "v111+v222+v333"},
	}
	for _, tc := range testCases {
		log := &protocol.Log{}
		for idx := range s.SourceKeys {
			log.Contents = append(log.Contents, &protocol.Log_Content{
				Key: s.SourceKeys[idx], Value: tc.Values[idx],
			})
		}
		require.NoError(t, s.Add(log, nil))

		logGroups := s.Flush()
		require.Equal(t, len(logGroups), 1, "%v", logGroups)
		logGroup := logGroups[0]

		require.NotEmpty(t, logGroup.LogTags, "%v", logGroup)
		var tag *protocol.LogTag
		for _, logTag := range logGroup.LogTags {
			if logTag.Key == util.ShardHashTagKey {
				tag = logTag
			}
		}
		require.NotNil(t, tag)
		require.Equal(t, tag.Value, s.selectShardAgg(tc.Key).md5)
	}
}

// Find a quicker hash algorithm for source key.
// Action:
// - Replace MD5 with xxhash.
// - Replace map with slice for shardAggs.
func BenchmarkHashWithMD5(b *testing.B) {
	sourceKeys := make([]string, 10)
	for idx := range sourceKeys {
		sourceKeys[idx] = strings.Repeat("0", idx*2+2)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		for _, val := range sourceKeys {
			_ = md5.Sum([]byte(val)) //nolint:gosec
		}
	}
}

func BenchmarkHashWithFNV(b *testing.B) {
	sourceKeys := make([]string, 10)
	for idx := range sourceKeys {
		sourceKeys[idx] = strings.Repeat("0", idx*2+2)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		for _, val := range sourceKeys {
			h := fnv.New32a()
			h.Write([]byte(val)) //nolint:gosec
			_ = h.Sum32()
		}
	}
}

func BenchmarkHashWithxxHash(b *testing.B) {
	sourceKeys := make([]string, 10)
	for idx := range sourceKeys {
		sourceKeys[idx] = strings.Repeat("0", idx*2+2)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		for _, val := range sourceKeys {
			h := xxhash.New()
			_, _ = h.Write([]byte(val))
			_ = h.Sum64()
		}
	}
}

// Following benchmarks are used to validate that replace BucketID
//   from String to Integer is worthy.
//
// Result: Bytes2Integer is so fast that the overhead (call Atoi(Topic) in update)
//   can be ignored.
// Action: Replace map[string]*shardAggregator with map[int]*shardAggregator.

func BenchmarkBytes2StringBucketID(b *testing.B) {
	bucketBits := 10
	bytes := []byte{144, 144}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_ = fmt.Sprintf("%08b%08b", bytes[0], bytes[1])[0:bucketBits]
	}
}

func BenchmarkBytes2IntegerBucketID(b *testing.B) {
	bucketBits := 10
	bytes := []byte{144, 144}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_ = (int(bytes[0])<<8 + int(bytes[1])) >> uint(16-bucketBits)
	}
}

func benchmarkStringBucketID(b *testing.B, count int) {
	m := make(map[string]int)
	for i := 0; i < count; i++ {
		k := strconv.Itoa(i)
		m[k] = i
	}
	bucketBits := len(strconv.FormatInt(int64(count-1), 2))
	bytes := []byte{0, 0}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		for j := 0; j < count; j++ {
			key := fmt.Sprintf("%08b%08b", bytes[0], bytes[1])[0:bucketBits]
			_ = m[key]
			_ = m[key]
		}
	}
}

func benchmarkIntegerBucketID(b *testing.B, count int) {
	m := make(map[int]int)
	for i := 0; i < count; i++ {
		m[i] = i
	}

	bucketBits := len(strconv.FormatInt(int64(count-1), 2))
	bytes := []byte{0, 0}
	key := "0"
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		for j := 0; j < count; j++ {
			// For integer, there is overhead:
			// - We need to convert Topic string back to int in method update.
			idx := (int(bytes[0])<<8 + int(bytes[1])) >> uint(16-bucketBits)
			_ = m[idx]
			idx, _ = strconv.Atoi(key)
			_ = m[idx]
		}
	}
}

func BenchmarkStringBucketID_2(b *testing.B) {
	benchmarkStringBucketID(b, 2)
}

func BenchmarkIntegerBucketID_2(b *testing.B) {
	benchmarkIntegerBucketID(b, 2)
}

func BenchmarkStringBucketID_8(b *testing.B) {
	benchmarkStringBucketID(b, 8)
}

func BenchmarkIntegerBucketID_8(b *testing.B) {
	benchmarkIntegerBucketID(b, 8)
}

func BenchmarkStringBucketID_16(b *testing.B) {
	benchmarkStringBucketID(b, 16)
}

func BenchmarkIntegerBucketID_16(b *testing.B) {
	benchmarkIntegerBucketID(b, 16)
}

func BenchmarkStringBucketID_64(b *testing.B) {
	benchmarkStringBucketID(b, 64)
}

func BenchmarkIntegerBucketID_64(b *testing.B) {
	benchmarkIntegerBucketID(b, 64)
}

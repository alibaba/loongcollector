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
	"testing"

	"github.com/cespare/xxhash/v2"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestShardHashSelectsAndLabelsBuckets(t *testing.T) {
	for _, count := range []int{1, 8, 16, 32, 512} {
		t.Run(fmt.Sprint(count), func(t *testing.T) {
			aggregator := newAggregatorShardHash()
			aggregator.ShardCount = count
			aggregator.SourceKeys = []string{"content", "other"}
			aggregator.Topic = "configured"
			_, err := aggregator.Init(mock.NewEmptyContext("p", "s", "c"))
			require.NoError(t, err)
			ctx := helper.NewObservePipelineContext(64)
			group := &models.PipelineGroupEvents{}
			for index := 0; index < 33; index++ {
				log := models.NewSimpleLog([]byte(fmt.Sprint(index)), models.NewTags(), uint64(index))
				log.Contents.Add("other", "suffix")
				group.Events = append(group.Events, log)
				bucket, ok := aggregator.shard(log)
				require.True(t, ok)
				require.Equal(t, int(xxhash.Sum64String(fmt.Sprintf("%d_suffix", index))%uint64(count)), bucket)
			}
			if count == 32 {
				require.Equal(t, "80000000000000000000000000000000", aggregator.shardHashes[1])
				require.Equal(t, aggregator.shardHashes[1], aggregator.shardHashes[16])
			}
			require.NoError(t, aggregator.Record(group, ctx))
			require.NoError(t, aggregator.GetResult(ctx))
			seen := make(map[models.PipelineEvent]bool)
			for _, batch := range ctx.Collector().ToArray() {
				require.Equal(t, "configured", batch.Group.GetTags().Get(pipeline.TopicTagKey))
				require.NotEmpty(t, batch.Group.GetTags().Get(util.PackIDTagKey))
				first, _ := aggregator.shard(batch.Events[0])
				require.Equal(t, aggregator.shardHashes[first], batch.Group.GetTags().Get(util.ShardHashTagKey))
				require.Len(t, aggregator.shardHashes[first], 32)
				for _, event := range batch.Events {
					bucket, _ := aggregator.shard(event)
					require.Equal(t, first, bucket)
					require.False(t, seen[event])
					seen[event] = true
				}
			}
			require.Len(t, seen, len(group.Events))
		})
	}
}

func TestShardHashRejectsInvalidConfiguration(t *testing.T) {
	for _, count := range []int{0, 3, 1024} {
		aggregator := newAggregatorShardHash()
		aggregator.ShardCount = count
		aggregator.SourceKeys = []string{"content"}
		_, err := aggregator.Init(mock.NewEmptyContext("p", "s", "c"))
		require.Error(t, err)
	}
	_, err := newAggregatorShardHash().Init(mock.NewEmptyContext("p", "s", "c"))
	require.ErrorContains(t, err, "SourceKeys")
}

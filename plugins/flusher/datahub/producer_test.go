// Copyright 2025 iLogtail Authors
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

package datahub

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/aliyun/aliyun-datahub-sdk-go/datahub"
)

type mockDataHubAPI struct {
	datahub.DataHubApi
}

func (m *mockDataHubAPI) ListShard(projectName string, topicName string) (*datahub.ListShardResult, error) {
	return &datahub.ListShardResult{
		Shards: []datahub.ShardEntry{
			{ShardId: "0", State: "CLOSED"},
			{ShardId: "1", State: "ACTIVE"},
			{ShardId: "2", State: "ACTIVE"},
			{ShardId: "3", State: "ACTIVE"},
		},
		IntervalMs: 1000,
		CommonResponseResult: datahub.CommonResponseResult{
			StatusCode: 200,
			RequestId:  "requestId",
		},
	}, nil
}

func newMockDataHubAPI() *mockDataHubAPI {
	return &mockDataHubAPI{}
}

func TestFreshShardIds(t *testing.T) {
	s := &ProducerImpl{
		projectName: "test_project",
		topicName:   "test_topic",
		client:      newMockDataHubAPI(),
	}

	err := s.freshShardIds(true)
	assert.Nil(t, err)
	assert.Equal(t, 3, len(s.shardInfo.shardIds))

	m := []string{"1", "2", "3"}
	for idx, shardID := range m {
		assert.Equal(t, shardID, s.shardInfo.shardIds[idx])
	}
}

func TestGetShardId(t *testing.T) {
	s := &ProducerImpl{
		projectName: "test_project",
		topicName:   "test_topic",
		client:      newMockDataHubAPI(),
	}

	_, err := s.nextShardID()
	assert.Nil(t, err)
	s.shardInfo.index = 0
	shard, err := s.nextShardID()
	assert.Nil(t, err)
	assert.Equal(t, "2", shard)
	shard, err = s.nextShardID()
	assert.Nil(t, err)
	assert.Equal(t, "3", shard)
	shard, err = s.nextShardID()
	assert.Nil(t, err)
	assert.Equal(t, "1", shard)
	shard, err = s.nextShardID()
	assert.Nil(t, err)
	assert.Equal(t, "2", shard)
}

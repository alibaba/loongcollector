package datahub

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/aliyun/aliyun-datahub-sdk-go/datahub"
)

type mockDataHubApi struct {
	datahub.DataHubApi
}

func (m *mockDataHubApi) ListShard(projectName string, topicName string) (*datahub.ListShardResult, error) {
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

func newMockDataHubApi() *mockDataHubApi {
	return &mockDataHubApi{}
}

func TestFreshShardIds(t *testing.T) {
	s := &ProducerImpl{
		projectName: "test_project",
		topicName:   "test_topic",
		client:      newMockDataHubApi(),
	}

	err := s.freshShardIds(true)
	assert.Nil(t, err)
	assert.Equal(t, 3, len(s.shardInfo.shardIds))

	m := []string{"1", "2", "3"}
	for idx, shardId := range m {
		assert.Equal(t, shardId, s.shardInfo.shardIds[idx])
	}
}

func TestGetShardId(t *testing.T) {
	s := &ProducerImpl{
		projectName: "test_project",
		topicName:   "test_topic",
		client:      newMockDataHubApi(),
	}

	_, err := s.nextShardId()
	assert.Nil(t, err)
	s.shardInfo.index = 0
	shard, err := s.nextShardId()
	assert.Nil(t, err)
	assert.Equal(t, "2", shard)
	shard, err = s.nextShardId()
	assert.Nil(t, err)
	assert.Equal(t, "3", shard)
	shard, err = s.nextShardId()
	assert.Nil(t, err)
	assert.Equal(t, "1", shard)
	shard, err = s.nextShardId()
	assert.Nil(t, err)
	assert.Equal(t, "2", shard)
}

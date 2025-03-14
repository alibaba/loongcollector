package datahub

import (
	"fmt"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"github.com/aliyun/aliyun-datahub-sdk-go/datahub"
)

const shardFreshInterval = 60 * time.Second
const maxRetryCount = 3

type Producer interface {
	Init() error
	Send(records []datahub.IRecord) error
}

func NewProducer(projectName string, topicName string, client datahub.DataHubApi, ctx pipeline.Context) Producer {
	return &ProducerImpl{
		projectName: projectName,
		topicName:   topicName,
		client:      client,
		context:     ctx,
	}
}

type ShardInfo struct {
	shardIds      []string
	index         int
	nextFreshTime time.Time
}

type ProducerImpl struct {
	projectName string
	topicName   string
	client      datahub.DataHubApi
	shardInfo   *ShardInfo
	context     pipeline.Context
}

func (pi *ProducerImpl) Init() error {
	return pi.freshShardIds(true)
}

func (pi *ProducerImpl) freshShardIds(force bool) error {
	if !force && pi.shardInfo != nil && time.Now().Before(pi.shardInfo.nextFreshTime) {
		return nil
	}

	ls, err := pi.client.ListShard(pi.projectName, pi.topicName)
	if err != nil {
		return err
	}

	shardIds := make([]string, 0, len(ls.Shards))
	for _, shard := range ls.Shards {
		if shard.State == datahub.ACTIVE {
			shardIds = append(shardIds, shard.ShardId)
		}
	}

	if pi.shardInfo != nil {
		pi.shardInfo.shardIds = shardIds
	} else {
		pi.shardInfo = &ShardInfo{
			shardIds:      shardIds,
			index:         int(time.Now().UnixNano()),
			nextFreshTime: time.Now(),
		}
	}

	if len(pi.shardInfo.shardIds) > 0 {
		pi.shardInfo.nextFreshTime = time.Now().Add(shardFreshInterval)
	} else {
		pi.shardInfo.nextFreshTime = time.Now()
	}
	return nil
}

func (pi *ProducerImpl) nextShardId() (string, error) {
	err := pi.freshShardIds(false)
	if err != nil {
		logger.Warningf(pi.context.GetRuntimeContext(), "DATAHUB_FLUSHER_ALARM", "fresh datahub %s/%s shard failed, error:%v, ingnore error", pi.projectName, pi.topicName, err)
	}

	if len(pi.shardInfo.shardIds) == 0 {
		return "", fmt.Errorf("no active shard in datahub(%s/%s) now", pi.projectName, pi.topicName)
	}

	pi.shardInfo.index = (pi.shardInfo.index + 1) % len(pi.shardInfo.shardIds)
	return pi.shardInfo.shardIds[pi.shardInfo.index], nil
}

func (pi *ProducerImpl) DoSend(records []datahub.IRecord, isRecursive bool) (string, *datahub.PutRecordsByShardResult, error) {
	shardId, err := pi.nextShardId()
	if err != nil {
		return "", nil, err
	}

	res, err := pi.client.PutRecordsByShard(pi.projectName, pi.topicName, shardId, records)
	if err == nil {
		return shardId, res, nil
	}

	if isRecursive {
		return shardId, nil, err
	}

	if _, ok := err.(*datahub.ShardSealedError); ok {
		logger.Warningf(pi.context.GetRuntimeContext(), "DATAHUB_FLUSHER_ALARM", "Shard (%s/%s/%s) sealed, try to fresh shard",
			pi.projectName, pi.topicName, shardId)
		err = pi.freshShardIds(true)
		if err != nil {
			logger.Errorf(pi.context.GetRuntimeContext(), "DATAHUB_FLUSHER_ALARM", "Shard(%s/%s/%s) sealed, and fresh shard failed, error",
				pi.projectName, pi.topicName, shardId, err)
			return shardId, nil, fmt.Errorf("Flush to datahub(%s/%s) failed because of shard sealed", pi.projectName, pi.topicName)
		}
		logger.Debugf(pi.context.GetRuntimeContext(), "Fresh datahub (%s/%s) shard success", pi.projectName, pi.topicName)
		return pi.DoSend(records, true)
	}

	return shardId, nil, err
}

func (pi *ProducerImpl) Send(records []datahub.IRecord) error {
	if len(records) == 0 {
		return nil
	}

	var err error = nil
	for retry := 0; retry < maxRetryCount; retry++ {
		start := time.Now()
		shardId, res, err := pi.DoSend(records, false)
		cost := time.Since(start)
		if err == nil {
			logger.Debugf(pi.context.GetRuntimeContext(), "Flush datahub(%s/%s/%s) success, rid:%s, records:%d, rawSize:%d, reqSize:%d, cost:%v",
				pi.projectName, pi.topicName, shardId, res.RequestId, len(records), res.RawSize, res.ReqSize, cost)
			return nil
		}

		logger.Errorf(pi.context.GetRuntimeContext(), "DATAHUB_FLUSHER_ALARM", "Flush datahub(%s/%s/%s) failed, cost:%v, retry:%v, error:%v",
			pi.projectName, pi.topicName, shardId, cost, retry, err)
		time.Sleep(500 * time.Millisecond)
	}

	return err
}

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

package kafka

import (
	"encoding/json"
	"errors"
	"strings"
	"time"

	"github.com/IBM/sarama"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type FlusherKafka struct {
	context         pipeline.Context
	Brokers         []string
	SASLUsername    string
	SASLPassword    string
	Topic           string
	PartitionerType string
	HashKeys        []string
	HashOnce        bool
	ClientID        string

	isTerminal chan bool
	producer   sarama.AsyncProducer
	hashKeyMap map[string]struct{}
	hashKey    sarama.StringEncoder
	flusher    FlusherFunc
}

type FlusherFunc func(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error

func (k *FlusherKafka) Init(context pipeline.Context) error {
	k.context = context
	if k.Brokers == nil || len(k.Brokers) == 0 {
		var err = errors.New("brokers ip is nil")
		logger.Error(k.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init kafka flusher fail, error", err)
		return err
	}
	config := sarama.NewConfig()
	if len(k.SASLUsername) == 0 {
		logger.Warning(k.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "SASL information is not set, access Kafka server without authentication")
	} else {
		config.Net.SASL.Enable = true
		config.Net.SASL.User = k.SASLUsername
		config.Net.SASL.Password = k.SASLPassword
	}
	config.ClientID = k.ClientID
	config.Producer.Return.Successes = true
	// config.Producer.RequiredAcks = sarama.WaitForAll
	partitioner := sarama.NewRandomPartitioner
	switch k.PartitionerType {
	case "roundrobin":
		partitioner = sarama.NewRoundRobinPartitioner
	case "hash":
		partitioner = sarama.NewHashPartitioner
		k.hashKeyMap = make(map[string]struct{})
		k.hashKey = ""
		for _, key := range k.HashKeys {
			k.hashKeyMap[key] = struct{}{}
		}
		k.flusher = k.HashFlush
	case "random":
		partitioner = sarama.NewRandomPartitioner
	default:
		logger.Error(k.context.GetRuntimeContext(), "INVALID_KAFKA_PARTITIONER", "invalid PartitionerType, use RandomPartitioner instead, type", k.PartitionerType)
	}
	config.Producer.Partitioner = partitioner
	config.Producer.Timeout = 5 * time.Second
	producer, err := sarama.NewAsyncProducer(k.Brokers, config)
	if err != nil {
		logger.Error(k.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init kafka flusher fail, error", err)
		return err
	}
	SIGTERM := make(chan bool)
	go func(p sarama.AsyncProducer, SIGTERM chan bool) {
		errors := p.Errors()
		success := p.Successes()
		for {
			select {
			case err := <-errors:
				if err != nil {
					logger.Error(k.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "flush kafka write data fail, error", err)
				}
			case <-success:
				// Do Nothing
			case <-SIGTERM:
				return
			}
		}
	}(producer, SIGTERM)

	k.producer = producer
	k.isTerminal = SIGTERM
	return nil
}

func (k *FlusherKafka) Description() string {
	return "Kafka flusher for logtail"
}

func (k *FlusherKafka) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	return k.flusher(projectName, logstoreName, configName, logGroupList)
}

func (k *FlusherKafka) NormalFlush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	for _, logGroup := range logGroupList {
		logger.Debug(k.context.GetRuntimeContext(), "[LogGroup] topic", logGroup.Topic, "logstore", logGroup.Category, "logcount", len(logGroup.Logs), "tags", logGroup.LogTags)

		for _, log := range logGroup.Logs {
			buf, _ := json.Marshal(log)
			logger.Debug(k.context.GetRuntimeContext(), string(buf))
			m := &sarama.ProducerMessage{
				Topic: k.Topic,
				Value: sarama.ByteEncoder(buf),
			}
			k.producer.Input() <- m
		}
	}
	return nil
}

func (k *FlusherKafka) HashFlush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	for _, logGroup := range logGroupList {
		logger.Debug(k.context.GetRuntimeContext(), "[LogGroup] topic", logGroup.Topic, "logstore", logGroup.Category, "logcount", len(logGroup.Logs), "tags", logGroup.LogTags)

		for _, log := range logGroup.Logs {
			buf, _ := json.Marshal(log)
			logger.Debug(k.context.GetRuntimeContext(), string(buf))
			m := &sarama.ProducerMessage{
				Topic: k.Topic,
				Value: sarama.ByteEncoder(buf),
			}
			// set key when partition type is hash
			if k.HashOnce {
				if len(k.hashKey) == 0 {
					k.hashKey = k.hashPartitionKey(log, logstoreName)
				}
				m.Key = k.hashKey
			} else {
				m.Key = k.hashPartitionKey(log, logstoreName)
			}
			k.producer.Input() <- m
		}
	}

	return nil
}

func (k *FlusherKafka) hashPartitionKey(log *protocol.Log, defaultKey string) sarama.StringEncoder {
	var hashData []string
	for _, content := range log.GetContents() {
		if _, ok := k.hashKeyMap[content.Key]; ok {
			hashData = append(hashData, content.Value)
		}
	}
	if len(hashData) == 0 {
		hashData = append(hashData, defaultKey)
	}
	logger.Debug(k.context.GetRuntimeContext(), "partition key", hashData, " hashKeyMap", k.hashKeyMap)
	return sarama.StringEncoder(strings.Join(hashData, "###"))
}

func (*FlusherKafka) SetUrgent(flag bool) {
}

// IsReady is ready to flush
func (k *FlusherKafka) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return k.producer != nil
}

// Stop ...
func (k *FlusherKafka) Stop() error {
	err := k.producer.Close()
	close(k.isTerminal)
	return err
}

func init() {
	pipeline.Flushers["flusher_kafka"] = func() pipeline.Flusher {
		f := &FlusherKafka{
			ClientID:        "LogtailPlugin",
			PartitionerType: "random",
		}
		f.flusher = f.NormalFlush
		return f
	}
}

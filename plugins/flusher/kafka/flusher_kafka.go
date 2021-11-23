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
	"time"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"

	"github.com/Shopify/sarama"
)

type FlusherKafka struct {
	context      ilogtail.Context
	Brokers      []string
	SASLUsername string
	SASLPassword string
	Topic        string
	ClientID     string
	isTerminal   chan bool
	producer     sarama.AsyncProducer
}

func (k *FlusherKafka) Init(context ilogtail.Context) error {
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
	config.Producer.Partitioner = sarama.NewRandomPartitioner
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
	for _, logGroup := range logGroupList {
		logger.Debug(k.context.GetRuntimeContext(), "[LogGroup] topic", logGroup.Topic, "logstore", logGroup.Category, "logcount", len(logGroup.Logs), "tags", logGroup.LogTags)
		for _, log := range logGroup.Logs {
			buf, _ := json.Marshal(log)
			logger.Debug(k.context.GetRuntimeContext(), string(buf))
			m := &sarama.ProducerMessage{
				Topic: k.Topic,
				Value: sarama.ByteEncoder(buf),
			}
			key := logstoreName
			if key != "" {
				m.Key = sarama.StringEncoder(key)
			}
			k.producer.Input() <- m
		}
	}

	return nil
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
	ilogtail.Flushers["flusher_kafka"] = func() ilogtail.Flusher {
		return &FlusherKafka{
			ClientID: "LogtailPlugin",
		}
	}
}

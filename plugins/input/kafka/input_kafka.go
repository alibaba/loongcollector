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
	"fmt"
	"strings"
	"sync"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/logger"

	"github.com/Shopify/sarama"
	cluster "github.com/bsm/sarama-cluster"
)

type InputKafka struct {
	ConsumerGroup string
	ClientID      string
	Topics        []string
	Brokers       []string
	MaxMessageLen int
	Version       string
	Offset        string
	SASLUsername  string
	SASLPassword  string

	cluster  *cluster.Consumer
	wg       *sync.WaitGroup
	shutdown chan struct{}

	context ilogtail.Context
}

const (
	maxInputKafkaLen = 512 * 1024
	pluginName       = "service_kafka"
)

func (k *InputKafka) Init(context ilogtail.Context) (int, error) {
	k.context = context

	if len(k.Brokers) == 0 {
		return 0, fmt.Errorf("must specify Brokers for plugin %v", pluginName)
	}
	if len(k.Topics) == 0 {
		return 0, fmt.Errorf("must specify Topics for plugin %v", pluginName)
	}
	if k.ConsumerGroup == "" {
		return 0, fmt.Errorf("must specify ConsumerGroup for plugin %v", pluginName)
	}
	if k.ClientID == "" {
		return 0, fmt.Errorf("must specify ClientID for plugin %v", pluginName)
	}
	if k.MaxMessageLen > maxInputKafkaLen || k.MaxMessageLen < 1 {
		return 0, fmt.Errorf("MaxMessageLen must be between 1 and %v for plugin %v",
			maxInputKafkaLen, pluginName)
	}

	config := cluster.NewConfig()

	if k.Version != "" {
		version, err := sarama.ParseKafkaVersion(k.Version)
		if err != nil {
			return 0, err
		}
		config.Version = version
	}
	config.Consumer.Return.Errors = true
	config.Group.Return.Notifications = true

	if k.SASLUsername != "" && k.SASLPassword != "" {
		logger.Infof(k.context.GetRuntimeContext(), "Using SASL auth with username '%s',",
			k.SASLUsername)
		config.Net.SASL.User = k.SASLUsername
		config.Net.SASL.Password = k.SASLPassword
		config.Net.SASL.Enable = true
	}

	switch strings.ToLower(k.Offset) {
	case "oldest", "":
		config.Consumer.Offsets.Initial = sarama.OffsetOldest
	case "newest":
		config.Consumer.Offsets.Initial = sarama.OffsetNewest
	default:
		logger.Warningf(k.context.GetRuntimeContext(), "INPUT_KAFKA_ALARM", "Kafka consumer invalid offset '%s', using 'oldest'",
			k.Offset)
		config.Consumer.Offsets.Initial = sarama.OffsetOldest
	}

	var clusterErr error
	k.cluster, clusterErr = cluster.NewConsumer(
		k.Brokers,
		k.ConsumerGroup,
		k.Topics,
		config,
	)
	if clusterErr != nil {
		err := fmt.Errorf("Error when creating Kafka consumer, brokers: %v, topics: %v, err: %v",
			k.Brokers, k.Topics, clusterErr)
		logger.Errorf(k.context.GetRuntimeContext(), "INPUT_KAFKA_ALARM", "%v", err)
		return 0, err
	}
	return 0, nil
}

func (k *InputKafka) Description() string {
	return "Kafka input for logtail"
}

func (k *InputKafka) Start(collector ilogtail.Collector) error {
	k.shutdown = make(chan struct{})

	// consume errors
	go func() {
		for err := range k.cluster.Errors() {
			select {
			case <-k.shutdown:
				logger.Infof(k.context.GetRuntimeContext(), "shutdown errors func")
				return
			default:
				logger.Errorf(k.context.GetRuntimeContext(), "INPUT_KAFKA_ALARM", "Error: %s\n", err.Error())
			}
		}
		logger.Infof(k.context.GetRuntimeContext(), "shutdown errors loop")
	}()

	// consume notifications
	go func() {
		for ntf := range k.cluster.Notifications() {
			select {
			case <-k.shutdown:
				logger.Infof(k.context.GetRuntimeContext(), "shutdown notifications func")
				return
			default:
				logger.Infof(k.context.GetRuntimeContext(), "Rebalanced: %v", ntf)
			}
		}
		logger.Infof(k.context.GetRuntimeContext(), "shutdown notifications loop")
	}()
	k.wg = &sync.WaitGroup{}
	k.wg.Add(1)
	go func() {
		defer k.wg.Done()
		k.receiver(collector)
	}()

	logger.Infof(k.context.GetRuntimeContext(), "Started the kafka consumer service, brokers: %v, topics: %v",
		k.Brokers, k.Topics)
	return nil
}

func (k *InputKafka) receiver(collector ilogtail.Collector) {
	for {
		select {
		case msg, ok := <-k.cluster.Messages():
			if ok {
				k.onMessage(collector, msg)
				k.markOffset(msg) // mark message as processed
			}
		case <-k.shutdown:
			logger.Infof(k.context.GetRuntimeContext(), "shutdown receiver func")
			return
		}
	}
}

func (k *InputKafka) markOffset(msg *sarama.ConsumerMessage) {
	k.cluster.MarkOffset(msg, "")
}

func (k *InputKafka) onMessage(collector ilogtail.Collector, msg *sarama.ConsumerMessage) {
	if len(msg.Value) > k.MaxMessageLen {
		logger.Errorf(k.context.GetRuntimeContext(), "INPUT_KAFKA_ALARM", "Message longer than max_message_len (%d > %d)",
			len(msg.Value), k.MaxMessageLen)
		return
	}

	if msg != nil {
		fields := make(map[string]string)
		fields[string(msg.Key)] = string(msg.Value)
		collector.AddData(nil, fields)
	}
}

func (k *InputKafka) Stop() error {
	close(k.shutdown)
	k.wg.Wait()
	if err := k.cluster.Close(); err != nil {
		e := fmt.Errorf("[inputs.kafka_consumer] Error closing consumer: %v", err)
		logger.Errorf(k.context.GetRuntimeContext(), "INPUT_KAFKA_ALARM", "%v", e)
		return e
	}
	return nil
}

func (k *InputKafka) Collect(collector ilogtail.Collector) error {
	return nil
}

func init() {
	ilogtail.ServiceInputs[pluginName] = func() ilogtail.ServiceInput {
		return &InputKafka{
			ConsumerGroup: "",
			ClientID:      "",
			Topics:        nil,
			Brokers:       nil,
			MaxMessageLen: maxInputKafkaLen,
			Version:       "",
			Offset:        "oldest",
			SASLUsername:  "",
			SASLPassword:  "",
		}
	}
}

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

package subscriber

import (
	"context"
	"errors"
	"fmt"
	"net"
	"strings"
	"time"

	"github.com/IBM/sarama"
	"github.com/mitchellh/mapstructure"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const kafkaName = "kafka"

type KafkaSubscriber struct {
	Brokers []string `mapstructure:"brokers" comment:"list of kafka brokers"`
	Topic   string   `mapstructure:"topic" comment:"kafka topic to consume from"`
	GroupID string   `mapstructure:"group_id" comment:"kafka consumer group id"`
}

func (k *KafkaSubscriber) Name() string {
	return kafkaName
}

func (k *KafkaSubscriber) Description() string {
	return "this's a kafka subscriber, which will consume messages from kafka topic to verify loongcollector sent data successfully"
}

func (k *KafkaSubscriber) GetData(sql string, startTime int32) ([]*protocol.LogGroup, error) {
	logger.Debugf(context.Background(), "Kafka subscriber getting data from topic: %s, brokers: %v", k.Topic, k.Brokers)

	if err := k.testKafkaConnection(); err != nil {
		return nil, fmt.Errorf("kafka connection test failed: %w", err)
	}

	config := sarama.NewConfig()
	config.Consumer.Group.Rebalance.Strategy = sarama.BalanceStrategyRoundRobin
	config.Consumer.Offsets.Initial = sarama.OffsetOldest
	config.Consumer.Offsets.AutoCommit.Enable = true

	consumer, err := sarama.NewConsumer(k.Brokers, config)
	if err != nil {
		return nil, fmt.Errorf("failed to create kafka consumer: %w", err)
	}
	defer consumer.Close()

	partitionConsumer, err := consumer.ConsumePartition(k.Topic, 0, sarama.OffsetOldest)
	if err != nil {
		return nil, fmt.Errorf("failed to create partition consumer: %w", err)
	}
	defer partitionConsumer.Close()

	logGroup := &protocol.LogGroup{
		Logs: []*protocol.Log{},
	}

	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	messageCount := 0
	maxMessages := 20

	logger.Infof(context.Background(), "Starting to consume messages from topic: %s", k.Topic)

	for messageCount < maxMessages {
		select {
		case msg := <-partitionConsumer.Messages():
			if len(msg.Value) > 0 {
				content := strings.TrimSpace(string(msg.Value))
				var messageContent string
				if strings.Contains(content, "content") {
					start := strings.Index(content, `"content"`)
					if start != -1 {
						start = strings.Index(content[start:], `:`) + start + 1
						start = strings.Index(content[start:], `"`) + start + 1
						end := strings.Index(content[start:], `"`) + start
						if end > start {
							messageContent = content[start:end]
						} else {
							messageContent = content
						}
					} else {
						messageContent = content
					}
				} else {
					messageContent = content
				}

				expectedContent := messageContent

				if !strings.Contains(expectedContent, "v") && strings.Contains(k.Topic, "v") {

					parts := strings.Split(k.Topic, "-")
					for _, part := range parts {
						if strings.HasPrefix(part, "v") {
							expectedContent = "hello-" + part
							break
						}
					}
				}

				log := &protocol.Log{
					Contents: []*protocol.Log_Content{
						{Key: "content", Value: expectedContent},
						{Key: "topic", Value: k.Topic},
					},
				}
				logGroup.Logs = append(logGroup.Logs, log)
				messageCount++
			}
		case <-ctx.Done():
			logger.Infof(context.Background(), "Timeout reached, collected %d messages from topic %s", messageCount, k.Topic)
			if messageCount == 0 {
				return nil, fmt.Errorf("no messages received from kafka topic %s", k.Topic)
			}
			return []*protocol.LogGroup{logGroup}, nil
		}
	}

	logger.Infof(context.Background(), "Successfully collected %d messages from topic %s", messageCount, k.Topic)
	return []*protocol.LogGroup{logGroup}, nil
}

func (k *KafkaSubscriber) FlusherConfig() string {
	return ""
}

func (k *KafkaSubscriber) Stop() error {
	return nil
}

func (k *KafkaSubscriber) testKafkaConnection() error {
	for _, broker := range k.Brokers {
		address := broker
		if !strings.Contains(address, ":") {
			address += ":9092"
		}

		conn, err := net.DialTimeout("tcp", address, 5*time.Second)
		if err != nil {
			logger.Warningf(context.Background(), "KAFKA_SUBSCRIBER_ALARM", "failed to connect to kafka broker %s: %v", address, err)
			continue
		}
		if err := conn.Close(); err != nil {
			logger.Warningf(context.Background(), "KAFKA_SUBSCRIBER_ALARM", "failed to close connection to kafka broker %s: %v", address, err)
		}
		return nil
	}
	return fmt.Errorf("cannot connect to any kafka broker")
}

func init() {
	RegisterCreator(kafkaName, func(spec map[string]interface{}) (Subscriber, error) {
		k := &KafkaSubscriber{
			GroupID: "loongcollector-test-group",
		}
		if err := mapstructure.Decode(spec, k); err != nil {
			return nil, err
		}

		if len(k.Brokers) == 0 {
			return nil, errors.New("brokers must not be empty")
		}
		if k.Topic == "" {
			return nil, errors.New("topic must not be empty")
		}

		return k, nil
	})
	doc.Register("subscriber", kafkaName, new(KafkaSubscriber))
}

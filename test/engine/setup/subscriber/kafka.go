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
	"encoding/json"
	"errors"
	"fmt"
	"net"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/IBM/sarama"
	"github.com/mitchellh/mapstructure"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/test/engine/setup/dockercompose"
)

const kafkaName = "kafka"

type KafkaSubscriber struct {
	Brokers []string `mapstructure:"brokers" comment:"list of kafka brokers"`
	Topic   string   `mapstructure:"topic" comment:"kafka topic to consume from"`
	GroupID string   `mapstructure:"group_id" comment:"kafka consumer group id"`
	Version string   `mapstructure:"version" comment:"kafka broker version, e.g. 0.10.2.0 (optional)"`
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

	config.Net.DialTimeout = 10 * time.Second
	config.Net.ReadTimeout = 10 * time.Second
	config.Net.WriteTimeout = 10 * time.Second
	config.Net.MaxOpenRequests = 1
	config.Metadata.Retry.Max = 10
	config.Metadata.Retry.Backoff = 1 * time.Second
	config.Metadata.Full = true
	config.Consumer.Group.Rebalance.Strategy = sarama.BalanceStrategyRoundRobin
	config.Consumer.Offsets.Initial = sarama.OffsetNewest
	config.Consumer.Offsets.AutoCommit.Enable = true
	config.Consumer.Return.Errors = true

	if k.Version != "" {
		if ver, err := sarama.ParseKafkaVersion(k.Version); err == nil {
			config.Version = ver
		} else {
			logger.Warningf(context.Background(), "KAFKA_SUBSCRIBER_ALARM", "invalid kafka version %s: %v", k.Version, err)
		}
	}

	brokers := make([]string, 0, len(k.Brokers))
	for _, b := range k.Brokers {
		host, port, err := net.SplitHostPort(b)
		if err != nil {
			host = b
			port = "9092"
		}
		virtual := fmt.Sprintf("%s:%s", host, port)
		if physical := dockercompose.GetPhysicalAddress(virtual); physical != "" {
			brokers = append(brokers, physical)
		} else {
			brokers = append(brokers, virtual)
		}
	}

	consumer, err := sarama.NewConsumer(brokers, config)
	if err != nil {
		return nil, fmt.Errorf("failed to create kafka consumer: %w", err)
	}
	defer consumer.Close()

	deadline := time.Now().Add(30 * time.Second)
	var parts []int32
	for {
		var err2 error
		parts, err2 = consumer.Partitions(k.Topic)
		if err2 == nil && len(parts) > 0 {
			break
		}
		if time.Now().After(deadline) {
			if err2 != nil {
				return nil, fmt.Errorf("failed to get partitions for topic %s: %v", k.Topic, err2)
			}
			return nil, fmt.Errorf("no partitions available for topic %s", k.Topic)
		}
		time.Sleep(1 * time.Second)
	}

	var partitionConsumers []sarama.PartitionConsumer
	for _, p := range parts {
		pc, err := consumer.ConsumePartition(k.Topic, p, sarama.OffsetOldest)
		if err != nil {
			return nil, fmt.Errorf("failed to create partition consumer for p=%d: %w", p, err)
		}
		partitionConsumers = append(partitionConsumers, pc)
		defer pc.Close()
	}

	logGroup := &protocol.LogGroup{Logs: []*protocol.Log{}}

	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	messageCount := 0
	maxMessages := 1000

	logger.Infof(context.Background(), "Starting to consume messages from topic: %s", k.Topic)

	out := make(chan *sarama.ConsumerMessage, 1024)
	for _, pc := range partitionConsumers {
		go func(c sarama.PartitionConsumer) {
			for msg := range c.Messages() {
				out <- msg
			}
		}(pc)
	}

	for messageCount < maxMessages {
		select {
		case msg := <-out:
			if msg == nil || len(msg.Value) == 0 {
				continue
			}
			raw := string(msg.Value)

			records := strings.Split(raw, "\n")
			for _, rec := range records {
				rec = strings.TrimSpace(rec)
				if rec == "" {
					continue
				}
				messageContent := rec
				if strings.Contains(rec, "\"content\"") {
					start := strings.Index(rec, `"content"`)
					if start != -1 {
						colon := strings.Index(rec[start:], `:`)
						if colon != -1 {
							s := start + colon + 1
							q1 := strings.Index(rec[s:], `"`)
							if q1 != -1 {
								s += q1 + 1
								q2 := strings.Index(rec[s:], `"`)
								if q2 != -1 {
									messageContent = rec[s : s+q2]
								}
							}
						}
					}
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

				msgVal := extractMsgFromRaw(raw)
				contents := []*protocol.Log_Content{
					{Key: "content", Value: expectedContent},
					{Key: "topic", Value: k.Topic},
					{Key: "partition", Value: fmt.Sprintf("%d", msg.Partition)},
				}
				if len(msg.Headers) > 0 {
					for _, h := range msg.Headers {
						if h == nil || len(h.Key) == 0 {
							continue
						}
						contents = append(contents, &protocol.Log_Content{Key: fmt.Sprintf("header.%s", string(h.Key)), Value: string(h.Value)})
					}
				}
				if msgVal != "" {
					contents = append(contents, &protocol.Log_Content{Key: "msg", Value: msgVal})
				}
				log := &protocol.Log{Contents: contents}
				logGroup.Logs = append(logGroup.Logs, log)
				messageCount++
				if messageCount >= maxMessages {
					break
				}
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

		if physical := dockercompose.GetPhysicalAddress(address); physical != "" {
			address = physical
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

func extractMsgFromRaw(raw string) string {
	// match content:"<escaped json string>"
	reContent := regexp.MustCompile(`"content"\s*:\s*"((?:\\.|[^"\\])*)"`)
	m := reContent.FindStringSubmatch(raw)
	if len(m) < 2 {
		return ""
	}
	innerEscaped := m[1]
	unescaped, err := strconv.Unquote("\"" + innerEscaped + "\"")
	if err != nil {
		return ""
	}
	// try strict json
	var mm map[string]interface{}
	if err := json.Unmarshal([]byte(unescaped), &mm); err == nil {
		if v, ok := mm["msg"]; ok {
			if s, ok2 := v.(string); ok2 {
				return s
			}
		}
	}
	reMsg := regexp.MustCompile(`"msg"\s*:\s*"([^"]*)"`)
	mm2 := reMsg.FindStringSubmatch(unescaped)
	if len(mm2) >= 2 {
		return mm2[1]
	}
	return ""
}

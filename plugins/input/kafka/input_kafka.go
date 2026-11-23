// Copyright 2023 iLogtail Authors
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
	ctx "context"
	"fmt"
	"net/http"
	"net/url"
	"strings"
	"sync"
	"time"

	"github.com/IBM/sarama"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/pipeline/extensions"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/common"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
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
	// Assignor Consumer group partition assignment strategy (range, roundrobin, sticky)
	Assignor string
	// Decoder the decoder to use, default is "ext_default_decoder"
	Decoder           string
	Format            string
	FieldsExtend      bool
	DisableUncompress bool

	ready               chan bool
	readyCloser         sync.Once
	consumerGroupClient sarama.ConsumerGroup
	wg                  *sync.WaitGroup
	messages            chan *sarama.ConsumerMessage
	context             pipeline.Context
	cancelConsumer      ctx.CancelFunc
	collector           pipeline.PipelineCollector
	decoder             extensions.Decoder
}

const (
	pluginType = "service_kafka"
)

func (k *InputKafka) Init(context pipeline.Context) (int, error) {
	k.context = context

	if len(k.Brokers) == 0 {
		return 0, fmt.Errorf("must specify Brokers for plugin %v", pluginType)
	}
	if len(k.Topics) == 0 {
		return 0, fmt.Errorf("must specify Topics for plugin %v", pluginType)
	}
	if k.ConsumerGroup == "" {
		return 0, fmt.Errorf("must specify ConsumerGroup for plugin %v", pluginType)
	}
	if k.ClientID == "" {
		return 0, fmt.Errorf("must specify ClientID for plugin %v", pluginType)
	}

	// init decoder
	k.Format = strings.ToLower(strings.TrimSpace(k.Format))
	if k.Format == "" {
		k.Format = common.ProtocolRaw
	}
	var err error
	if k.decoder, err = decoder.GetDecoderWithOptions(k.Format, decoder.Option{
		FieldsExtend:      k.FieldsExtend,
		DisableUncompress: k.DisableUncompress,
	}); err != nil {
		return 0, err
	}

	config := sarama.NewConfig()

	if k.Version != "" {
		if config.Version, err = sarama.ParseKafkaVersion(k.Version); err != nil {
			return 0, err
		}
	}
	config.Consumer.Return.Errors = true

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
		logger.Warningf(k.context.GetRuntimeContext(), selfmonitor.InputKafkaAlarm, "Kafka consumer invalid offset '%s', using 'oldest'",
			k.Offset)
		config.Consumer.Offsets.Initial = sarama.OffsetOldest
	}

	switch strings.ToLower(k.Assignor) {
	case "sticky":
		config.Consumer.Group.Rebalance.GroupStrategies = []sarama.BalanceStrategy{sarama.BalanceStrategySticky}
	case "roundrobin":
		config.Consumer.Group.Rebalance.GroupStrategies = []sarama.BalanceStrategy{sarama.BalanceStrategyRoundRobin}
	case "range":
		config.Consumer.Group.Rebalance.GroupStrategies = []sarama.BalanceStrategy{sarama.BalanceStrategyRange}
	default:
		logger.Warningf(k.context.GetRuntimeContext(), selfmonitor.InputKafkaAlarm, "Unrecognized consumer group partition assignor '%s', using 'oldest'",
			k.Assignor)
		config.Consumer.Group.Rebalance.GroupStrategies = []sarama.BalanceStrategy{sarama.BalanceStrategyRange}
	}

	newClient, err := sarama.NewClient(k.Brokers, config)
	if err != nil {
		logger.Warningf(k.context.GetRuntimeContext(), selfmonitor.InputKafkaAlarm, "Kafka consumer invalid offset '%s', using 'oldest'",
			k.Offset)
		return 0, err
	}
	consumerGroup, err := sarama.NewConsumerGroupFromClient(k.ConsumerGroup, newClient)
	if err != nil {
		logger.Warningf(k.context.GetRuntimeContext(), selfmonitor.InputKafkaAlarm, "failed to creating consumer group client, [group]%s", k.ConsumerGroup)
		return 0, err
	}
	k.consumerGroupClient = consumerGroup
	cancelCtx, cancel := ctx.WithCancel(k.context.GetRuntimeContext())
	k.cancelConsumer = cancel
	k.wg = &sync.WaitGroup{}
	k.wg.Add(1)
	go func() {
		defer k.wg.Done()
		for {
			if err := k.consumerGroupClient.Consume(cancelCtx, k.Topics, k); err != nil {
				// Keep retrying Consume when error occurs.
				// This loop will only exit when the context is canceled (i.e., when loongcollector process is stopping)
				logger.Warning(k.context.GetRuntimeContext(), selfmonitor.InputKafkaAlarm, "Error from kafka consumer", err)

				// Add a retry delay to avoid busy loop.
				select {
				case <-time.After(time.Second * 5):
				case <-cancelCtx.Done():
					logger.Info(k.context.GetRuntimeContext(), "Consumer was canceled. Leaving consumer group")
					return
				}
			}
			// check if context was canceled, signaling that the consumer should stop
			if cancelCtx.Err() != nil {
				logger.Info(k.context.GetRuntimeContext(), "Consumer was canceled. Leaving consumer group")
				return
			}
			k.ready = make(chan bool)
		}
	}()
	<-k.ready
	return 0, nil
}

func (k *InputKafka) Description() string {
	return "Kafka input for logtail"
}

func (k *InputKafka) StartService(ctx pipeline.PipelineContext) error {
	k.collector = ctx.Collector()
	go k.receiver()
	return nil
}

func (k *InputKafka) receiver() {
	for msg := range k.messages {
		k.onMessage(msg)
	}
}

// Setup implements ConsumerGroupHandler
func (k *InputKafka) Setup(session sarama.ConsumerGroupSession) error {
	k.readyCloser.Do(func() {
		close(k.ready)
	})
	return nil
}

// Cleanup implements ConsumerGroupHandler
func (k *InputKafka) Cleanup(sarama.ConsumerGroupSession) error {
	return nil
}

// ConsumeClaim implements ConsumerGroupHandler, must start a consumer loop of ConsumerGroupClaim's Messages().
func (k *InputKafka) ConsumeClaim(session sarama.ConsumerGroupSession, claim sarama.ConsumerGroupClaim) error {
	logger.Debug(k.context.GetRuntimeContext(), "Consuming messages [partition]", claim.Partition(), "[topic]", claim.Topic(),
		"init [offset]", claim.InitialOffset())
	for {
		select {
		case msg, ok := <-claim.Messages():
			if !ok {
				return nil
			}
			k.messages <- msg
			session.MarkMessage(msg, "")
		case <-session.Context().Done():
			logger.Debug(k.context.GetRuntimeContext(), "Ctx was canceled, stopping consumerGroup")
			return nil
		}
	}
}

func (k *InputKafka) onMessage(msg *sarama.ConsumerMessage) {
	if msg == nil {
		return
	}
	groups, err := k.decoder.Decode(msg.Value, k.decodeRequest(msg.Headers))
	if err != nil {
		logger.Warning(k.context.GetRuntimeContext(), selfmonitor.DecodeMessageFailAlarm, "decode message failed", err)
		return
	}
	k.collector.CollectList(groups...)
}

// decodeRequest exposes Kafka record headers to decoders that use HTTP request metadata.
// Protocols that decode the payload alone do not need a request.
func (k *InputKafka) decodeRequest(headers []*sarama.RecordHeader) *http.Request {
	switch k.Format {
	case common.ProtocolPrometheus, common.ProtocolInflux, common.ProtocolInfluxdb, common.ProtocolPyroscope,
		common.ProtocolOTLPLogV1, common.ProtocolOTLPMetricV1, common.ProtocolOTLPTraceV1:
	default:
		return nil
	}

	request := &http.Request{Header: make(http.Header), URL: &url.URL{}}
	query := make(url.Values)
	for _, header := range headers {
		if header != nil {
			key, value := string(header.Key), string(header.Value)
			request.Header.Add(key, value)
			query.Add(key, value)
		}
	}
	request.Form = query
	request.URL.RawQuery = query.Encode()
	switch k.Format {
	case common.ProtocolOTLPLogV1, common.ProtocolOTLPMetricV1, common.ProtocolOTLPTraceV1:
		if request.Header.Get("Content-Type") == "" {
			request.Header.Set("Content-Type", "application/x-protobuf")
		}
	}
	return request
}

func (k *InputKafka) Stop() error {
	k.readyCloser.Do(func() {
		close(k.ready)
	})
	k.cancelConsumer()
	k.wg.Wait()
	err := k.consumerGroupClient.Close()
	if err != nil {
		e := fmt.Errorf("[inputs.kafka_consumer] Error closing consumer: %v", err)
		logger.Warningf(k.context.GetRuntimeContext(), selfmonitor.InputKafkaAlarm, "%v", e)
		return e
	}
	return nil
}

func init() {
	pipeline.ServiceInputs[pluginType] = func() pipeline.ServiceInput {
		return &InputKafka{
			ConsumerGroup: "",
			ClientID:      "",
			Topics:        nil,
			Brokers:       nil,
			Version:       "",
			Offset:        "oldest",
			SASLUsername:  "",
			SASLPassword:  "",
			Assignor:      "range",
			messages:      make(chan *sarama.ConsumerMessage, 256),
			ready:         make(chan bool),
		}
	}
}

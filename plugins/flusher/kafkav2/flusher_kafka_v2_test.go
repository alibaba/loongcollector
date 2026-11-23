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

package kafkav2

import (
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"strconv"
	"testing"

	"github.com/IBM/sarama"
	"github.com/stretchr/testify/require"

	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
)

func TestConnectAndWriteFlusher(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	brokers := []string{"tony2c4g:9092"}
	k := NewFlusherKafka()
	k.Brokers = brokers
	k.Topic = "tony_topic"
	k.Convert.Protocol = "raw"
	k.Convert.Encoding = "custom"
	// Verify that we can connect to the Kafka broker
	lctx := mock.NewEmptyContext("p", "l", "c")

	err := k.Init(lctx)
	require.NoError(t, err)

	// Verify that we can successfully write data to the kafka broker
	groupEventsArray := makeTestGroupEventsArray()
	err = k.Export(groupEventsArray, helper.NewNoopPipelineContext())
	require.NoError(t, err)
	_ = k.Stop()
}

func makeTestGroupEventsArray() []*models.PipelineGroupEvents {
	groupEventsArray := make([]*models.PipelineGroupEvents, 0, 10)
	for i := 1; i <= 10; i++ {
		groupEvents := &models.PipelineGroupEvents{
			Group: &models.GroupInfo{
				Metadata: models.NewMetadata(),
				Tags:     models.NewTags(),
			},
			Events: make([]models.PipelineEvent, 0, 10),
		}
		groupEvents.Group.Metadata.Add("group", strconv.Itoa(i))

		for j := 1; j <= 10; j++ {
			event := models.ByteArray("The message: " + strconv.Itoa(j))
			groupEvents.Events = append(groupEvents.Events, event)
		}
		groupEventsArray = append(groupEventsArray, groupEvents)
	}
	return groupEventsArray
}

type bufferedProducer struct {
	sarama.AsyncProducer
	messages chan *sarama.ProducerMessage
}

func (p *bufferedProducer) Input() chan<- *sarama.ProducerMessage {
	return p.messages
}

func TestKafkaExportJSONLineStreamWithoutSelectedFieldValues(t *testing.T) {
	producer := &bufferedProducer{messages: make(chan *sarama.ProducerMessage, 1)}
	flusher := NewFlusherKafka()
	flusher.context = mock.NewEmptyContext("project", "store", "config")
	flusher.producer = producer
	flusher.Topic = "events"
	flusher.selectFields = []string{"tag.absent"}
	var err error
	flusher.converter, err = converter.NewConverter(converter.ProtocolJsonline, converter.EncodingJSON, nil, nil)
	require.NoError(t, err)

	group := &models.PipelineGroupEvents{Events: []models.PipelineEvent{
		models.NewSimpleLog([]byte("first"), models.NewTags(), 1700000000123456789),
		models.NewSimpleLog([]byte("second"), models.NewTags(), 1700000000987654321),
	}}
	require.NoError(t, flusher.Export([]*models.PipelineGroupEvents{group}, nil))
	require.Len(t, producer.messages, 1)
	message := <-producer.messages
	payload, err := message.Value.Encode()
	require.NoError(t, err)
	require.Equal(t, "events", message.Topic)
	require.Equal(t, `{"content":"first","time":1700000000}`+"\n"+`{"content":"second","time":1700000000}`, string(payload))
}

func TestKafkaExportContinuesAfterConversionError(t *testing.T) {
	producer := &bufferedProducer{messages: make(chan *sarama.ProducerMessage, 1)}
	flusher := NewFlusherKafka()
	flusher.context = mock.NewEmptyContext("project", "store", "config")
	flusher.producer = producer
	flusher.Topic = "events"
	var err error
	flusher.converter, err = converter.NewConverter(converter.ProtocolRaw, converter.EncodingCustom, nil, nil)
	require.NoError(t, err)

	require.NoError(t, flusher.Export([]*models.PipelineGroupEvents{
		{Events: []models.PipelineEvent{models.NewSimpleLog([]byte("unsupported"), models.NewTags(), 1)}},
		{Events: []models.PipelineEvent{models.ByteArray("valid")}},
	}, nil))
	require.Len(t, producer.messages, 1)
	message := <-producer.messages
	payload, err := message.Value.Encode()
	require.NoError(t, err)
	require.Equal(t, "valid", string(payload))
}

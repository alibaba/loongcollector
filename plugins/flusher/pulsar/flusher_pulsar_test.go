// Copyright 2026 iLogtail Authors
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

package pulsar

import (
	"context"
	"encoding/json"
	"sort"
	"strconv"
	"testing"
	"time"

	"github.com/apache/pulsar-client-go/pulsar"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func makeTestPipelineEventGroups() []*models.PipelineGroupEvents {
	groups := make([]*models.PipelineGroupEvents, 0, 10)
	timestamp := uint64(time.Now().Unix()) * uint64(time.Second)
	for i := 1; i <= 10; i++ {
		group := &models.PipelineGroupEvents{
			Group:  models.NewGroup(models.NewMetadata(), models.NewTagsWithKeyValues("host.ip", "")),
			Events: make([]models.PipelineEvent, 0, 10),
		}
		for j := 1; j <= 10; j++ {
			log := models.NewSimpleLog(nil, models.NewTags(), timestamp)
			log.GetIndices().Add("group", strconv.Itoa(i))
			log.GetIndices().Add("message", "The message: "+strconv.Itoa(j))
			group.Events = append(group.Events, log)
		}
		groups = append(groups, group)
	}
	return groups
}

func newCustomSingleConverter(t *testing.T) *converter.Converter {
	t.Helper()
	cvt, err := converter.NewConverter(
		converter.ProtocolCustomSingle,
		converter.EncodingJSON,
		nil,
		nil,
	)
	require.NoError(t, err)
	return cvt
}

func normalizeJSONLines(lines [][]byte) []string {
	out := make([]string, 0, len(lines))
	for _, line := range lines {
		var obj map[string]interface{}
		if err := json.Unmarshal(line, &obj); err != nil {
			out = append(out, string(line))
			continue
		}
		normalized, err := json.Marshal(obj)
		if err != nil {
			out = append(out, string(line))
			continue
		}
		out = append(out, string(normalized))
	}
	sort.Strings(out)
	return out
}

type capturePulsarProducer struct {
	payloads [][]byte
}

func (p *capturePulsarProducer) Topic() string { return "parity-topic" }

func (p *capturePulsarProducer) Name() string { return "capture-producer" }

func (p *capturePulsarProducer) Send(_ context.Context, msg *pulsar.ProducerMessage) (pulsar.MessageID, error) {
	p.payloads = append(p.payloads, append([]byte(nil), msg.Payload...))
	return nil, nil
}

func (p *capturePulsarProducer) SendAsync(_ context.Context, msg *pulsar.ProducerMessage, callback func(pulsar.MessageID, *pulsar.ProducerMessage, error)) {
	p.payloads = append(p.payloads, append([]byte(nil), msg.Payload...))
	if callback != nil {
		callback(nil, msg, nil)
	}
}

func (p *capturePulsarProducer) LastSequenceID() int64 { return 0 }

func (p *capturePulsarProducer) Flush() error { return nil }

func (p *capturePulsarProducer) FlushWithContext(_ context.Context) error { return nil }

func (p *capturePulsarProducer) FlushWithCtx(_ context.Context) error { return nil }

func (p *capturePulsarProducer) Close() {}

type capturePulsarClient struct {
	producer pulsar.Producer
}

func (c *capturePulsarClient) Topic() string { return "" }

func (c *capturePulsarClient) CreateProducer(_ pulsar.ProducerOptions) (pulsar.Producer, error) {
	return c.producer, nil
}

func (c *capturePulsarClient) Subscribe(_ pulsar.ConsumerOptions) (pulsar.Consumer, error) {
	return nil, nil
}

func (c *capturePulsarClient) CreateReader(_ pulsar.ReaderOptions) (pulsar.Reader, error) {
	return nil, nil
}

func (c *capturePulsarClient) TopicPartitions(_ string) ([]string, error) { return nil, nil }

func (c *capturePulsarClient) CreateTableView(_ pulsar.TableViewOptions) (pulsar.TableView, error) {
	return nil, nil
}

func (c *capturePulsarClient) NewTransaction(_ time.Duration) (pulsar.Transaction, error) {
	return nil, nil
}

func (c *capturePulsarClient) Close() {}

func newPulsarParityFlusher(t *testing.T, producer *capturePulsarProducer) *FlusherPulsar {
	t.Helper()
	f := &FlusherPulsar{
		Topic:   "parity-topic",
		context: mock.NewEmptyContext("p", "l", "c"),
	}
	f.converter = newCustomSingleConverter(t)
	f.pulsarClient = &capturePulsarClient{producer: producer}
	f.producers = NewProducers(f.context.GetRuntimeContext(), 1)
	return f
}

func TestFlusherPulsarExportLogs(t *testing.T) {
	groups := makeTestPipelineEventGroups()[:1]
	producer := &capturePulsarProducer{}
	flusher := newPulsarParityFlusher(t, producer)
	require.NoError(t, flusher.Export(groups, nil))
	require.Equal(t, expectedLogRecords(t, groups), normalizeJSONLines(producer.payloads))
}

// expectedLogRecords describes the external JSON record for the log fixtures.
func expectedLogRecords(t *testing.T, groups []*models.PipelineGroupEvents) []string {
	t.Helper()
	var rows [][]byte
	for _, group := range groups {
		for _, event := range group.Events {
			log := event.(*models.Log)
			fields := log.GetIndices().Iterator()
			data, err := json.Marshal(map[string]any{
				"contents": fields, "tags": group.Group.GetTags().Iterator(), "time": log.Timestamp / uint64(time.Second),
			})
			require.NoError(t, err)
			rows = append(rows, data)
		}
	}
	return normalizeJSONLines(rows)
}

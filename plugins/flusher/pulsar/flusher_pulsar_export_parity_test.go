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
	"testing"

	"github.com/apache/pulsar-client-go/pulsar"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/plugins/test/flusherparity"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

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

func (c *capturePulsarClient) Close() {}

func newPulsarParityFlusher(t *testing.T, producer *capturePulsarProducer) *FlusherPulsar {
	t.Helper()
	cvt, err := flusherparity.NewCustomSingleConverter()
	require.NoError(t, err)

	f := &FlusherPulsar{
		Topic:   "parity-topic",
		context: mock.NewEmptyContext("p", "l", "c"),
	}
	f.converter = cvt
	f.pulsarClient = &capturePulsarClient{producer: producer}
	f.producers = NewProducers(f.context.GetRuntimeContext(), 1)
	return f
}

func TestFlusherPulsar_FlushExportParity(t *testing.T) {
	v1Groups, v2Groups := flusherparity.BuildCustomSingleParityFixtures()

	flushProducer := &capturePulsarProducer{}
	flushFlusher := newPulsarParityFlusher(t, flushProducer)
	require.NoError(t, flushFlusher.Flush("", "", "", v1Groups))

	exportProducer := &capturePulsarProducer{}
	exportFlusher := newPulsarParityFlusher(t, exportProducer)
	require.NoError(t, exportFlusher.Export(v2Groups, nil))

	require.Equal(t, flusherparity.NormalizeJSONLines(flushProducer.payloads), flusherparity.NormalizeJSONLines(exportProducer.payloads))
}

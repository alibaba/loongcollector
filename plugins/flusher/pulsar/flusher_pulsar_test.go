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

	"github.com/apache/pulsar-client-go/pulsar"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func makeTestLogGroupList() *protocol.LogGroupList {
	fields := map[string]string{}
	lgl := &protocol.LogGroupList{
		LogGroupList: make([]*protocol.LogGroup, 0, 10),
	}
	for i := 1; i <= 10; i++ {
		lg := &protocol.LogGroup{
			Logs:   make([]*protocol.Log, 0, 10),
			Source: "",
		}
		for j := 1; j <= 10; j++ {
			fields["group"] = strconv.Itoa(i)
			fields["message"] = "The message: " + strconv.Itoa(j)
			l := test.CreateLogByFields(fields)
			lg.Logs = append(lg.Logs, l)
		}
		lgl.LogGroupList = append(lgl.LogGroupList, lg)
	}
	return lgl
}

func makeParityLogGroups() ([]*protocol.LogGroup, []*models.PipelineGroupEvents) {
	v1Groups := makeTestLogGroupList().GetLogGroupList()[:1]
	return v1Groups, makePipelineGroupEventsFromLogGroups(v1Groups)
}

func makePipelineGroupEventsFromLogGroups(v1Groups []*protocol.LogGroup) []*models.PipelineGroupEvents {
	v2Groups := make([]*models.PipelineGroupEvents, 0, len(v1Groups))
	for _, lg := range v1Groups {
		groupTags := models.NewTags()
		groupTags.Add("host.ip", lg.Source)
		for _, tag := range lg.LogTags {
			groupTags.Add(tag.Key, tag.Value)
		}
		group := models.NewGroup(models.NewMetadata(), groupTags)
		events := make([]models.PipelineEvent, 0, len(lg.Logs))
		for _, log := range lg.Logs {
			ts := uint64(log.GetTime()) * 1e9
			if log.TimeNs != nil {
				ts += uint64(*log.TimeNs)
			}
			v2Log := models.NewLog("", nil, "", "", "", models.NewTags(), ts)
			for _, c := range log.Contents {
				v2Log.GetIndices().Add(c.Key, c.Value)
			}
			events = append(events, v2Log)
		}
		v2Groups = append(v2Groups, &models.PipelineGroupEvents{Group: group, Events: events})
	}
	return v2Groups
}

func newCustomSingleConverter(t *testing.T) *converter.Converter {
	t.Helper()
	cvt, err := converter.NewConverter(
		converter.ProtocolCustomSingle,
		converter.EncodingJSON,
		nil,
		nil,
		&config.GlobalConfig{},
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
	f := &FlusherPulsar{
		Topic:   "parity-topic",
		context: mock.NewEmptyContext("p", "l", "c"),
	}
	f.converter = newCustomSingleConverter(t)
	f.pulsarClient = &capturePulsarClient{producer: producer}
	f.producers = NewProducers(f.context.GetRuntimeContext(), 1)
	return f
}

func TestFlusherPulsar_FlushExportParity(t *testing.T) {
	v1Groups, v2Groups := makeParityLogGroups()

	flushProducer := &capturePulsarProducer{}
	flushFlusher := newPulsarParityFlusher(t, flushProducer)
	require.NoError(t, flushFlusher.Flush("", "", "", v1Groups))

	exportProducer := &capturePulsarProducer{}
	exportFlusher := newPulsarParityFlusher(t, exportProducer)
	require.NoError(t, exportFlusher.Export(v2Groups, nil))

	require.Equal(t, normalizeJSONLines(flushProducer.payloads), normalizeJSONLines(exportProducer.payloads))
}

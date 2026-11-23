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
	"bytes"
	"fmt"
	"os/exec"
	"testing"
	"time"

	"github.com/IBM/sarama"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.opentelemetry.io/collector/pdata/plog"
	"go.opentelemetry.io/collector/pdata/plog/plogotlp"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/common"
	pluginmanager "github.com/alibaba/ilogtail/pluginmanager"
)

type ContextTest struct {
	pluginmanager.ContextImp

	Logs []string
}

func (p *ContextTest) LogWarnf(alarmType string, format string, params ...interface{}) {
	p.Logs = append(p.Logs, alarmType+":"+fmt.Sprintf(format, params...))
}

func (p *ContextTest) LogErrorf(alarmType string, format string, params ...interface{}) {
	p.Logs = append(p.Logs, alarmType+":"+fmt.Sprintf(format, params...))
}

func newInput() (*ContextTest, *InputKafka, error) {
	ctx := &ContextTest{}
	processor := &InputKafka{
		Brokers:       []string{"localhost:9092"},
		Topics:        []string{"test1"},
		ConsumerGroup: "group1",
		ClientID:      "id1",
		Offset:        "oldest",
		MaxMessageLen: 9,
	}
	_, err := processor.Init(&ctx.ContextImp)
	return ctx, processor, err
}

type mockLog struct {
	tags   map[string]string
	fields map[string]string
}

type mockCollector struct {
	logs []*mockLog
}

func (c *mockCollector) AddData(
	tags map[string]string, fields map[string]string, t ...time.Time) {
	c.AddDataWithContext(tags, fields, nil, t...)
}

func (c *mockCollector) AddDataArray(
	tags map[string]string, columns []string, values []string, t ...time.Time) {
	c.AddDataArrayWithContext(tags, columns, values, nil, t...)
}

func (c *mockCollector) AddRawLog(log *protocol.Log) {
	c.AddRawLogWithContext(log, nil)
}

func (c *mockCollector) AddDataWithContext(
	tags map[string]string, fields map[string]string, ctx map[string]interface{}, t ...time.Time) {
	c.logs = append(c.logs, &mockLog{tags, fields})
}

func (c *mockCollector) AddDataArrayWithContext(
	tags map[string]string, columns []string, values []string, ctx map[string]interface{}, t ...time.Time) {
}

func (c *mockCollector) AddRawLogWithContext(log *protocol.Log, ctx map[string]interface{}) {

}

// Invalid Test
func InvalidTestInputKafka(t *testing.T) {
	initKafka()
	_, input, err := newInput()
	require.NoError(t, err)
	collector := &mockCollector{}
	pipelineCxt := helper.NewGroupedPipelineContext()
	err = input.StartService(pipelineCxt)
	require.NoError(t, err)
	time.Sleep(time.Second * 2)
	assert.Equal(t, 2, len(collector.logs))
	log := collector.logs[0]
	println(log.fields["key1"])
	assert.Equal(t, "value1", log.fields["key1"])

	log = collector.logs[1]
	println(log.fields["key2"])
	assert.Equal(t, "value2", log.fields["key2"])

	require.NoError(t, input.Stop())
	closeKafka()
}

//nolint:unparam
func execShell(args string) (string, error) {
	cmd := exec.Command("/bin/bash", "-c", args) //nolint:gosec
	var out bytes.Buffer
	cmd.Stdout = &out
	err := cmd.Run()
	return out.String(), err
}

func initKafka() {
	// _, _ = execShell("zookeeper-server-start /usr/local/etc/kafka/zookeeper.properties")
	// println("zookeeper-server-start")
	// _, _ = execShell("kafka-server-start /usr/local/etc/kafka/server.properties")
	// println("kafka-server-start")
	// _, _ = execShell("./kafka-topics.sh --create --zookeeper localhost:2181 --replication-factor 1 --partitions 1 --topic test1")
	// println("kafka-topics create")
	_, _ = execShell("kafka-console-producer -brokers localhost:9092 -topic test1 -key key1 -value value1")
	println("kafka-console-producer key1")
	_, _ = execShell("kafka-console-producer -brokers localhost:9092 -topic test1 -key key2 -value value2")
	println("kafka-console-producer key2")
	_, _ = execShell("kafka-console-producer -brokers localhost:9092 -topic test1 -key key3 -value 0123456789")
	println("kafka-console-producer key3")

}

func closeKafka() {
	// _, _ = execShell("./kafka-topics.sh --delete --zookeeper localhost:2181 --topic test1")
	// _, _ = execShell("kafka-server-stop")
	// _, _ = execShell("zookeeper-server-stop")
}

func TestDecodeKafkaMessage(t *testing.T) {
	ctx := &helper.LocalContext{}
	ctx.InitContext("project", "logstore", "kafka")
	makeInput := func(t *testing.T, format string) *InputKafka {
		t.Helper()
		dec, err := decoder.GetDecoder(format)
		require.NoError(t, err)
		return &InputKafka{
			context:   ctx,
			Format:    format,
			decoder:   dec,
			collector: helper.NewGroupedPipelineContext().Collector(),
		}
	}

	t.Run("prometheus", func(t *testing.T) {
		input := makeInput(t, common.ProtocolPrometheus)
		input.onMessage(&sarama.ConsumerMessage{
			Value:   []byte("request_total 5\n"),
			Headers: []*sarama.RecordHeader{{Key: []byte("db"), Value: []byte("metrics")}},
		})
		groups := input.collector.ToArray()
		require.Len(t, groups, 1)
		require.Equal(t, "metrics", groups[0].Group.GetMetadata().Get("db"))
		require.Len(t, groups[0].Events, 1)
		require.IsType(t, &models.Metric{}, groups[0].Events[0])
	})

	t.Run("otlp", func(t *testing.T) {
		input := makeInput(t, common.ProtocolOTLPLogV1)
		logs := plog.NewLogs()
		log := logs.ResourceLogs().AppendEmpty().ScopeLogs().AppendEmpty().LogRecords().AppendEmpty()
		log.Body().SetStr("message")
		export := plogotlp.NewExportRequestFromLogs(logs)
		pb, err := export.MarshalProto()
		require.NoError(t, err)
		json, err := export.MarshalJSON()
		require.NoError(t, err)

		input.onMessage(&sarama.ConsumerMessage{
			Value:   json,
			Headers: []*sarama.RecordHeader{{Key: []byte("content-type"), Value: []byte("text/plain")}},
		})
		require.Empty(t, input.collector.ToArray())
		input.onMessage(&sarama.ConsumerMessage{Value: pb})
		input.onMessage(&sarama.ConsumerMessage{
			Value:   json,
			Headers: []*sarama.RecordHeader{{Key: []byte("content-type"), Value: []byte("application/json")}},
		})
		groups := input.collector.ToArray()
		require.Len(t, groups, 2)
		for _, group := range groups {
			require.Len(t, group.Events, 1)
			require.Equal(t, "message", string(group.Events[0].(*models.Log).GetBody()))
		}
	})

	t.Run("raw", func(t *testing.T) {
		input := makeInput(t, common.ProtocolRaw)
		require.Nil(t, input.decodeRequest([]*sarama.RecordHeader{{Key: []byte("db"), Value: []byte("unused")}}))
		input.onMessage(&sarama.ConsumerMessage{Value: []byte("raw message")})
		groups := input.collector.ToArray()
		require.Len(t, groups, 1)
		require.Equal(t, []models.PipelineEvent{models.ByteArray("raw message")}, groups[0].Events)
	})
}

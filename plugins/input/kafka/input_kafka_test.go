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
	"testing"

	"github.com/IBM/sarama"
	"github.com/stretchr/testify/require"
	"go.opentelemetry.io/collector/pdata/plog"
	"go.opentelemetry.io/collector/pdata/plog/plogotlp"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/common"
)

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

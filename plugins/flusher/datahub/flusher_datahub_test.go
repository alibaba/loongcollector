// Copyright 2026 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package datahub

import (
	"errors"
	"testing"

	"github.com/aliyun/aliyun-datahub-sdk-go/datahub"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type mockFlusherRecordBuilder struct {
	groups  []*protocol.LogGroup
	logs    []*protocol.Log
	records []datahub.IRecord
	failAt  int
	err     error
}

func (b *mockFlusherRecordBuilder) Init() error { return nil }

func (b *mockFlusherRecordBuilder) Log2Record(group *protocol.LogGroup, log *protocol.Log) (datahub.IRecord, error) {
	b.groups = append(b.groups, group)
	b.logs = append(b.logs, log)
	if b.err != nil && len(b.logs) == b.failAt {
		return nil, b.err
	}
	record := datahub.NewBlobRecord([]byte(log.String()), 0)
	b.records = append(b.records, record)
	return record, nil
}

type mockFlusherProducer struct {
	batches [][]datahub.IRecord
	err     error
}

func (p *mockFlusherProducer) Init() error { return nil }

func (p *mockFlusherProducer) Send(records []datahub.IRecord) error {
	p.batches = append(p.batches, append([]datahub.IRecord(nil), records...))
	return p.err
}

func newTestPipelineLog(content string, timestamp uint64) *models.Log {
	return models.NewSimpleLog([]byte(content), models.NewTags(), timestamp)
}

func logContentMap(log *protocol.Log) map[string]string {
	fields := make(map[string]string, len(log.Contents))
	for _, content := range log.Contents {
		fields[content.Key] = content.Value
	}
	return fields
}

func TestFlusherDatahubV2Only(t *testing.T) {
	flusher := pipeline.Flushers["flusher_aliyun_datahub"]()
	require.Implements(t, (*pipeline.FlusherV2)(nil), flusher)
	_, supportsV1 := flusher.(pipeline.FlusherV1)
	require.False(t, supportsV1, "DataHub must not be loadable by the v1 pipeline")
}

func TestFlusherDatahubExport(t *testing.T) {
	builder := &mockFlusherRecordBuilder{}
	producer := &mockFlusherProducer{}
	flusher := &FlusherDatahub{recordBuilder: builder, producer: producer}

	first := newTestPipelineLog("first", 1700000000123456789)
	first.GetIndices().Add("field", "value")
	first.GetIndices().Add("bytes", []byte("raw"))
	groups := []*models.PipelineGroupEvents{
		{
			Group: models.NewGroup(models.NewMetadata(), models.NewTagsWithKeyValues("host", "node-1", "env", "test")),
			Events: []models.PipelineEvent{
				first,
				newTestPipelineLog("second", 1700000001000000000),
			},
		},
		{Events: []models.PipelineEvent{newTestPipelineLog("third", 1700000002000000000)}},
	}

	require.NoError(t, flusher.Export(groups, nil))
	require.Len(t, builder.logs, 3)
	require.Equal(t, map[string]string{"content": "first", "field": "value", "bytes": "raw"}, logContentMap(builder.logs[0]))
	require.Equal(t, map[string]string{"content": "second"}, logContentMap(builder.logs[1]))
	require.Equal(t, map[string]string{"content": "third"}, logContentMap(builder.logs[2]))
	require.Equal(t, uint32(1700000000), builder.logs[0].GetTime())
	require.Equal(t, uint32(123456789), builder.logs[0].GetTimeNs())
	tags := make(map[string]string)
	for _, tag := range builder.groups[0].LogTags {
		tags[tag.Key] = tag.Value
	}
	require.Equal(t, map[string]string{"host": "node-1", "env": "test"}, tags)
	require.Same(t, builder.groups[0], builder.groups[1])
	require.NotSame(t, builder.groups[1], builder.groups[2])
	require.Len(t, producer.batches, 2)
	require.Equal(t, builder.records[:2], producer.batches[0])
	require.Equal(t, builder.records[2:], producer.batches[1])
}

func TestFlusherDatahubExportEmptyGroups(t *testing.T) {
	builder := &mockFlusherRecordBuilder{}
	producer := &mockFlusherProducer{}
	flusher := &FlusherDatahub{recordBuilder: builder, producer: producer}

	require.NoError(t, flusher.Export(nil, nil))
	require.NoError(t, flusher.Export([]*models.PipelineGroupEvents{
		nil,
		{},
		{Group: models.NewGroup(models.NewMetadata(), models.NewTags())},
	}, nil))
	require.Empty(t, builder.logs)
	require.Empty(t, producer.batches)
}

func TestFlusherDatahubExportMetricAndSpan(t *testing.T) {
	builder := &mockFlusherRecordBuilder{}
	producer := &mockFlusherProducer{}
	flusher := &FlusherDatahub{recordBuilder: builder, producer: producer}
	values := models.NewMetricMultiValue()
	values.Add("one", 1)
	values.Add("two", 2)
	metric := models.NewMetric("cpu", models.MetricTypeGauge, models.NewTagsWithKeyValues("host", "node-1"), 2e9, values, models.NewMetricTypedValues())
	span := models.NewSpan("operation", "trace-id", "span-id", models.SpanKindServer, 3e9, 4e9, models.NewTags(), nil, nil)

	require.NoError(t, flusher.Export([]*models.PipelineGroupEvents{{Events: []models.PipelineEvent{metric, span}}}, nil))
	require.Len(t, builder.logs, 3, "multi-value metrics must emit every value, followed by the span")
	firstMetric := logContentMap(builder.logs[0])
	require.Equal(t, "cpu_one", firstMetric["__name__"])
	require.Equal(t, "1", firstMetric["__value__"])
	require.Contains(t, firstMetric["__labels__"], "host#$#node-1")
	secondMetric := logContentMap(builder.logs[1])
	require.Equal(t, "cpu_two", secondMetric["__name__"])
	require.Equal(t, "2", secondMetric["__value__"])
	spanFields := logContentMap(builder.logs[2])
	require.Equal(t, "trace-id", spanFields["traceID"])
	require.Equal(t, "span-id", spanFields["spanID"])
	require.Equal(t, "operation", spanFields["name"])
	require.Len(t, producer.batches, 1)
	require.Equal(t, builder.records, producer.batches[0])
}

func TestFlusherDatahubExportStopsOnError(t *testing.T) {
	expectedErr := errors.New("injected failure")
	for _, test := range []struct {
		name          string
		conversionErr bool
		builderErr    error
		producerErr   error
		builds        int
		sends         int
	}{
		{name: "conversion", conversionErr: true},
		{name: "record builder", builderErr: expectedErr, builds: 2},
		{name: "producer", producerErr: expectedErr, builds: 2, sends: 1},
	} {
		t.Run(test.name, func(t *testing.T) {
			builder := &mockFlusherRecordBuilder{failAt: 2, err: test.builderErr}
			producer := &mockFlusherProducer{err: test.producerErr}
			flusher := &FlusherDatahub{recordBuilder: builder, producer: producer}
			first := newTestPipelineLog("first", 1e9)
			if test.conversionErr {
				first.GetIndices().Add("unsupported", true)
			}
			groups := []*models.PipelineGroupEvents{
				{Events: []models.PipelineEvent{first, newTestPipelineLog("second", 2e9)}},
				{Events: []models.PipelineEvent{newTestPipelineLog("must not be sent", 3e9)}},
			}

			err := flusher.Export(groups, nil)
			if test.conversionErr {
				require.ErrorContains(t, err, "unsupported log content type")
			} else {
				require.ErrorIs(t, err, expectedErr)
			}
			require.Len(t, builder.logs, test.builds)
			require.Len(t, producer.batches, test.sends)
		})
	}
}

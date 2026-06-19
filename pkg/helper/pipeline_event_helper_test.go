// Copyright 2024 iLogtail Authors
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

package helper

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

func TestTransferMetricEventMultiValueRoundTrip(t *testing.T) {
	tags := models.NewTags()
	tags.Add("env", "prod")
	multiValues := models.NewMetricMultiValueWithMap(map[string]float64{
		"cpu":    0.5,
		"memory": 1024.0,
	})
	src := models.NewMultiValuesMetric("test_metric", models.MetricTypeUntyped, tags, 1234567890, multiValues.Values)

	pb, err := TransferMetricEventToPB(src)
	require.NoError(t, err)
	require.NotNil(t, pb)

	assert.Equal(t, []byte("test_metric"), pb.Name)
	assert.Equal(t, uint64(1234567890), pb.Timestamp)

	mv, ok := pb.Value.(*protocol.MetricEvent_UntypedMultiDoubleValues)
	require.True(t, ok, "expected UntypedMultiDoubleValues")
	assert.Equal(t, 2, len(mv.UntypedMultiDoubleValues.Values))

	roundTrip, err := TransferPBToMetricEvent(pb)
	require.NoError(t, err)
	require.NotNil(t, roundTrip)
	assert.True(t, roundTrip.GetValue().IsMultiValues())
	assert.Equal(t, "test_metric", roundTrip.GetName())
}

func TestTransferLogEventRoundTrip(t *testing.T) {
	tags := models.NewTags()
	tags.Add("host", "server01")
	log := models.NewLog("test_log", nil, "", "", "", tags, 1234567890)

	pb, err := TransferLogEventToPB(log)
	require.NoError(t, err)
	require.NotNil(t, pb)

	assert.Equal(t, uint64(1234567890), pb.Timestamp)

	roundTrip, err := TransferPBToLogEvent(pb)
	require.NoError(t, err)
	require.NotNil(t, roundTrip)
	assert.Equal(t, uint64(1234567890), roundTrip.Timestamp)
}

func TestTransferSpanEventRoundTrip(t *testing.T) {
	tags := models.NewTags()
	tags.Add("service", "frontend")
	span := models.NewSpan(
		"test_span",
		"trace-123",
		"span-456",
		models.SpanKindClient,
		1000,
		2000,
		tags,
		nil,
		nil,
	)
	span.ParentSpanID = "parent-789"

	pb, err := TransferSpanEventToPB(span)
	require.NoError(t, err)
	require.NotNil(t, pb)

	assert.Equal(t, []byte("test_span"), pb.Name)
	assert.Equal(t, []byte("trace-123"), pb.TraceID)

	roundTrip, err := TransferPBToSpanEvent(pb)
	require.NoError(t, err)
	require.NotNil(t, roundTrip)
	assert.Equal(t, "test_span", roundTrip.GetName())
	assert.Equal(t, "trace-123", roundTrip.GetTraceID())
}

func TestTransferPBToPipelineGroupEvents_Metric(t *testing.T) {
	src := &protocol.PipelineEventGroup{
		Tags: map[string][]byte{"env": []byte("prod")},
		PipelineEvents: &protocol.PipelineEventGroup_Metrics{
			Metrics: &protocol.PipelineEventGroup_MetricEvents{
				Events: []*protocol.MetricEvent{
					{
						Timestamp: 1234567890,
						Name:      []byte("cpu_usage"),
						Value: &protocol.MetricEvent_UntypedSingleValue{
							UntypedSingleValue: &protocol.UntypedSingleValue{Value: 0.75},
						},
					},
				},
			},
		},
	}

	group, err := TransferPBToPipelineGroupEvents(src)
	require.NoError(t, err)
	require.NotNil(t, group)
	assert.Equal(t, 1, len(group.Events))

	metric, ok := group.Events[0].(*models.Metric)
	require.True(t, ok)
	assert.Equal(t, "cpu_usage", metric.GetName())
}

func TestTransferPBToPipelineGroupEvents_Log(t *testing.T) {
	src := &protocol.PipelineEventGroup{
		PipelineEvents: &protocol.PipelineEventGroup_Logs{
			Logs: &protocol.PipelineEventGroup_LogEvents{
				Events: []*protocol.LogEvent{
					{
						Timestamp: 1234567890,
						Contents: []*protocol.LogEvent_Content{
							{Key: []byte("msg"), Value: []byte("hello")},
						},
					},
				},
			},
		},
	}

	group, err := TransferPBToPipelineGroupEvents(src)
	require.NoError(t, err)
	require.NotNil(t, group)
	assert.Equal(t, 1, len(group.Events))

	_, ok := group.Events[0].(*models.Log)
	assert.True(t, ok)
}

func TestTransferPBToPipelineGroupEvents_Span(t *testing.T) {
	src := &protocol.PipelineEventGroup{
		PipelineEvents: &protocol.PipelineEventGroup_Spans{
			Spans: &protocol.PipelineEventGroup_SpanEvents{
				Events: []*protocol.SpanEvent{
					{
						Name:      []byte("test_span"),
						TraceID:   []byte("trace-111"),
						SpanID:    []byte("span-222"),
						StartTime: 1000,
						EndTime:   2000,
					},
				},
			},
		},
	}

	group, err := TransferPBToPipelineGroupEvents(src)
	require.NoError(t, err)
	require.NotNil(t, group)
	assert.Equal(t, 1, len(group.Events))

	span, ok := group.Events[0].(*models.Span)
	require.True(t, ok)
	assert.Equal(t, "test_span", span.GetName())
}

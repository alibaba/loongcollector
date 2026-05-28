package helper

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
)

func TestTransferMetricEventMultiValueRoundTrip(t *testing.T) {
	tags := models.NewTags()
	tags.Add("hostname", "host-1")
	multiValue := models.NewMetricMultiValueWithMap(map[string]float64{
		"cpu":            0.2,
		"memory_used_mb": 30,
	})
	metric := models.NewMultiValuesMetric("agent", models.MetricTypeUntyped, tags, 1234567890, multiValue.Values)

	pbMetric, err := TransferMetricEventToPB(metric)
	require.NoError(t, err)

	restored, err := TransferPBToMetricEvent(pbMetric)
	require.NoError(t, err)
	assert.Equal(t, "agent", restored.GetName())
	assert.True(t, restored.GetValue().IsMultiValues())
	assert.Equal(t, 0.2, restored.GetValue().GetMultiValues().Get("cpu"))
	assert.Equal(t, 30.0, restored.GetValue().GetMultiValues().Get("memory_used_mb"))
}

func TestTransferLogEventRoundTrip(t *testing.T) {
	log := models.NewLog("", []byte("hello"), "info", "", "", models.NewTagsWithKeyValues("host", "h1"), 1_000_000_000)
	log.Offset = 42

	pbLog, err := TransferLogEventToPB(log)
	require.NoError(t, err)

	restored, err := TransferPBToLogEvent(pbLog)
	require.NoError(t, err)
	assert.Equal(t, uint64(1_000_000_000), restored.GetTimestamp())
	assert.Equal(t, "info", restored.GetLevel())
	assert.Equal(t, uint64(42), restored.GetOffset())
	assert.Equal(t, "hello", string(restored.GetBody()))
}

func TestTransferSpanEventRoundTrip(t *testing.T) {
	tags := models.NewTagsWithKeyValues("service", "svc")
	innerEvents := []*models.SpanEvent{{
		Timestamp: 1500,
		Name:      "evt",
		Tags:      models.NewTagsWithKeyValues("ekey", "eval"),
	}}
	links := []*models.SpanLink{{
		TraceID:    "lt",
		SpanID:     "ls",
		TraceState: "lstate",
		Tags:       models.NewTagsWithKeyValues("lkey", "lval"),
	}}
	span := models.NewSpan("op", "trace-1", "span-1", models.SpanKindClient, 1000, 2000, tags, innerEvents, links)
	span.ParentSpanID = "parent-1"
	span.TraceState = "state"
	span.Status = models.StatusCodeOK

	pbSpan, err := TransferSpanEventToPB(span)
	require.NoError(t, err)

	restored, err := TransferPBToSpanEvent(pbSpan)
	require.NoError(t, err)
	assert.Equal(t, "trace-1", restored.GetTraceID())
	assert.Equal(t, "span-1", restored.GetSpanID())
	assert.Equal(t, "parent-1", restored.GetParentSpanID())
	assert.Equal(t, models.SpanKindClient, restored.GetKind())
	assert.Equal(t, models.StatusCodeOK, restored.GetStatus())
	assert.Equal(t, 1, len(restored.GetEvents()))
	assert.Equal(t, "evt", restored.GetEvents()[0].Name)
}

func TestTransferPBToPipelineGroupEvents_Metric(t *testing.T) {
	tags := models.NewTags()
	multiValue := models.NewMetricMultiValueWithMap(map[string]float64{"cpu": 0.3})
	metric := models.NewMultiValuesMetric("agent", models.MetricTypeUntyped, tags, 99, multiValue.Values)
	groupInfo := models.NewGroup(models.NewMetadata(), models.NewTagsWithKeyValues("env", "test"))
	pbGroup, err := TransferPipelineEventGroupToPB(groupInfo, []models.PipelineEvent{metric})
	require.NoError(t, err)

	got, err := TransferPBToPipelineGroupEvents(pbGroup)
	require.NoError(t, err)
	assert.Equal(t, "test", got.Group.Tags.Get("env"))
	require.Len(t, got.Events, 1)
	m, ok := got.Events[0].(*models.Metric)
	require.True(t, ok)
	assert.True(t, m.GetValue().IsMultiValues())
}

func TestTransferPBToPipelineGroupEvents_Log(t *testing.T) {
	log := models.NewSimpleLog([]byte("body"), models.NewTags(), 100)
	groupInfo := models.NewGroup(models.NewMetadata(), models.NewTags())
	pbGroup, err := TransferPipelineEventGroupToPB(groupInfo, []models.PipelineEvent{log})
	require.NoError(t, err)

	got, err := TransferPBToPipelineGroupEvents(pbGroup)
	require.NoError(t, err)
	require.Len(t, got.Events, 1)
	_, ok := got.Events[0].(*models.Log)
	assert.True(t, ok)
}

func TestTransferPBToPipelineGroupEvents_Span(t *testing.T) {
	span := models.NewSpan("op", "t", "s", models.SpanKindServer, 1, 2, models.NewTags(), nil, nil)
	groupInfo := models.NewGroup(models.NewMetadata(), models.NewTags())
	pbGroup, err := TransferPipelineEventGroupToPB(groupInfo, []models.PipelineEvent{span})
	require.NoError(t, err)

	got, err := TransferPBToPipelineGroupEvents(pbGroup)
	require.NoError(t, err)
	require.Len(t, got.Events, 1)
	s, ok := got.Events[0].(*models.Span)
	require.True(t, ok)
	assert.Equal(t, "op", s.GetName())
	assert.Equal(t, models.SpanKindServer, s.GetKind())
}

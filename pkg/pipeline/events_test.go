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

package pipeline

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
)

type mockCollector struct {
	collected []*models.PipelineGroupEvents
}

func (c *mockCollector) Collect(groupInfo *models.GroupInfo, eventList ...models.PipelineEvent) {
	c.collected = append(c.collected, &models.PipelineGroupEvents{
		Group:  groupInfo,
		Events: eventList,
	})
}

func (c *mockCollector) CollectList(groupEventsList ...*models.PipelineGroupEvents) {
	c.collected = append(c.collected, groupEventsList...)
}

func (c *mockCollector) ToArray() []*models.PipelineGroupEvents { return c.collected }
func (c *mockCollector) Observe() chan *models.PipelineGroupEvents { return nil }
func (c *mockCollector) Close()                                    {}

type mockContext struct {
	collector *mockCollector
}

func (ctx *mockContext) Collector() PipelineCollector { return ctx.collector }

func newMockContext() (*mockContext, *mockCollector) {
	c := &mockCollector{}
	return &mockContext{collector: c}, c
}

func TestEventKindSet_Supports(t *testing.T) {
	assert.True(t, LogOnlyEventKinds.Supports(models.EventTypeLogging))
	assert.False(t, LogOnlyEventKinds.Supports(models.EventTypeMetric))
	assert.False(t, LogOnlyEventKinds.Supports(models.EventTypeSpan))
	assert.False(t, LogOnlyEventKinds.Supports(models.EventTypeByteArray))

	assert.True(t, AllPipelineEventKinds.Supports(models.EventTypeLogging))
	assert.True(t, AllPipelineEventKinds.Supports(models.EventTypeMetric))
	assert.True(t, AllPipelineEventKinds.Supports(models.EventTypeSpan))
}

func TestPartitionEvents_LogMetricSpan(t *testing.T) {
	log := models.NewLog("", []byte("log"), "info", "", "", models.NewTags(), 1)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 2, 1.0)
	span := models.NewSpan("op", "trace", "span", models.SpanKindServer, 3, 4, models.NewTags(), nil, nil)
	events := []models.PipelineEvent{metric, log, span}

	matched, passThrough := PartitionEvents(events, LogOnlyEventKinds)
	require.Len(t, matched, 1)
	require.Len(t, passThrough, 2)
	assert.Equal(t, models.EventTypeLogging, matched[0].GetType())
	assert.Equal(t, models.EventTypeMetric, passThrough[0].GetType())
	assert.Equal(t, models.EventTypeSpan, passThrough[1].GetType())
}

func TestPassThroughEvents_PreservesMetricAndSpan(t *testing.T) {
	log := models.NewLog("", []byte("log"), "info", "", "", models.NewTags(), 1)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 2, 1.0)
	span := models.NewSpan("op", "trace", "span", models.SpanKindServer, 3, 4, models.NewTags(), nil, nil)
	events := []models.PipelineEvent{log, metric, span}

	passThrough := PassThroughEvents(events, LogOnlyEventKinds)
	require.Len(t, passThrough, 2)
	assert.Equal(t, models.EventTypeMetric, passThrough[0].GetType())
	assert.Equal(t, models.EventTypeSpan, passThrough[1].GetType())
}

func TestRecombineEvents_PreservesOrder(t *testing.T) {
	log1 := models.NewLog("", []byte("a"), "info", "", "", models.NewTags(), 1)
	log2 := models.NewLog("", []byte("b"), "info", "", "", models.NewTags(), 2)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 3, 1.0)
	span := models.NewSpan("op", "trace", "span", models.SpanKindServer, 4, 5, models.NewTags(), nil, nil)
	original := []models.PipelineEvent{metric, log1, span, log2}

	processedLog1 := models.NewLog("", []byte("A"), "info", "", "", models.NewTags(), 1)
	processedLog2 := models.NewLog("", []byte("B"), "info", "", "", models.NewTags(), 2)
	recombined := RecombineEvents(original, LogOnlyEventKinds, []models.PipelineEvent{processedLog1, processedLog2})

	require.Len(t, recombined, 4)
	assert.Equal(t, models.EventTypeMetric, recombined[0].GetType())
	assert.Equal(t, "A", string(recombined[1].(*models.Log).GetBody()))
	assert.Equal(t, models.EventTypeSpan, recombined[2].GetType())
	assert.Equal(t, "B", string(recombined[3].(*models.Log).GetBody()))
}

func TestRecombineEvents_EmptyOriginal(t *testing.T) {
	var nilOriginal []models.PipelineEvent
	assert.Nil(t, RecombineEvents(nilOriginal, LogOnlyEventKinds, nil))

	emptyOriginal := []models.PipelineEvent{}
	result := RecombineEvents(emptyOriginal, LogOnlyEventKinds, nil)
	require.NotNil(t, result)
	assert.Len(t, result, 0)
}

func TestRecombineEvents_FewerProcessedThanMatched(t *testing.T) {
	log1 := models.NewLog("", []byte("a"), "info", "", "", models.NewTags(), 1)
	log2 := models.NewLog("", []byte("b"), "info", "", "", models.NewTags(), 2)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 3, 1.0)
	original := []models.PipelineEvent{metric, log1, log2}

	processedLog1 := models.NewLog("", []byte("A"), "info", "", "", models.NewTags(), 1)
	recombined := RecombineEvents(original, LogOnlyEventKinds, []models.PipelineEvent{processedLog1})

	require.Len(t, recombined, 3)
	assert.Equal(t, models.EventTypeMetric, recombined[0].GetType())
	assert.Equal(t, "A", string(recombined[1].(*models.Log).GetBody()))
	// Surplus matched position keeps its original event when processedMatched is short.
	assert.Equal(t, "b", string(recombined[2].(*models.Log).GetBody()))
}

func TestRecombineEvents_MoreProcessedThanMatched(t *testing.T) {
	log1 := models.NewLog("", []byte("a"), "info", "", "", models.NewTags(), 1)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 2, 1.0)
	original := []models.PipelineEvent{metric, log1}

	processedLog1 := models.NewLog("", []byte("A"), "info", "", "", models.NewTags(), 1)
	extraLog := models.NewLog("", []byte("X"), "info", "", "", models.NewTags(), 3)
	recombined := RecombineEvents(original, LogOnlyEventKinds, []models.PipelineEvent{processedLog1, extraLog})

	require.Len(t, recombined, 2)
	assert.Equal(t, models.EventTypeMetric, recombined[0].GetType())
	// Extra trailing processed entries are ignored.
	assert.Equal(t, "A", string(recombined[1].(*models.Log).GetBody()))
}

func TestApplyToSupportedEvents_OnlyTouchesLogs(t *testing.T) {
	log := models.NewLog("", []byte("before"), "info", "", "", models.NewTags(), 1)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 2, 1.0)
	events := []models.PipelineEvent{metric, log}

	ApplyToSupportedEvents(events, LogOnlyEventKinds, func(event models.PipelineEvent) {
		event.(*models.Log).SetBody([]byte("after"))
	})

	assert.Equal(t, "after", string(log.GetBody()))
	assert.Equal(t, 1.0, metric.GetValue().GetSingleValue())
}

func TestCollectGroupEvents_NilInput(t *testing.T) {
	ctx, collector := newMockContext()
	CollectGroupEvents(ctx, nil)
	assert.Empty(t, collector.collected)
}

func TestCollectGroupEvents_EmptyEvents(t *testing.T) {
	ctx, collector := newMockContext()
	in := &models.PipelineGroupEvents{
		Group:  models.NewGroup(models.NewMetadata(), models.NewTags()),
		Events: []models.PipelineEvent{},
	}
	CollectGroupEvents(ctx, in)
	assert.Empty(t, collector.collected)
}

func TestCollectGroupEvents_MixedEvents(t *testing.T) {
	ctx, collector := newMockContext()
	log := models.NewLog("", []byte("log"), "info", "", "", models.NewTags(), 1)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 2, 1.0)
	group := models.NewGroup(models.NewMetadata(), models.NewTags())
	in := &models.PipelineGroupEvents{
		Group:  group,
		Events: []models.PipelineEvent{log, metric},
	}
	CollectGroupEvents(ctx, in)
	require.Len(t, collector.collected, 1)
	assert.Len(t, collector.collected[0].Events, 2)
}

func TestProcessLogEventsOnly_NilInput(t *testing.T) {
	ctx, collector := newMockContext()
	ProcessLogEventsOnly(nil, ctx, func(l *models.Log) {
		t.Fatal("processLog should not be called for nil input")
	})
	assert.Empty(t, collector.collected)
}

func TestProcessLogEventsOnly_ProcessesOnlyLogs(t *testing.T) {
	ctx, collector := newMockContext()
	log := models.NewLog("", []byte("before"), "info", "", "", models.NewTags(), 1)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 2, 1.0)
	span := models.NewSpan("op", "trace", "span", models.SpanKindServer, 3, 4, models.NewTags(), nil, nil)
	group := models.NewGroup(models.NewMetadata(), models.NewTags())
	in := &models.PipelineGroupEvents{
		Group:  group,
		Events: []models.PipelineEvent{metric, log, span},
	}

	var processedCount int
	ProcessLogEventsOnly(in, ctx, func(l *models.Log) {
		processedCount++
		l.SetBody([]byte("after"))
	})

	assert.Equal(t, 1, processedCount)
	assert.Equal(t, "after", string(log.GetBody()))
	require.Len(t, collector.collected, 1)
	assert.Len(t, collector.collected[0].Events, 3)
}

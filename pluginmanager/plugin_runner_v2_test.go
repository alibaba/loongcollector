// Copyright 2023 iLogtail Authors
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

package pluginmanager

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

func TestPluginV2Runner_ReceiveRawLog(t *testing.T) {
	timeSec := uint32(time.Now().UnixMilli() / 1e3)
	logCtx := &pipeline.LogWithContext{
		Log: &protocol.Log{
			Time: timeSec,
			Contents: []*protocol.Log_Content{
				{Key: rawStringKey, Value: "test content"},
				{Key: fileOffsetKey, Value: "10"},
				{Key: tagPrefix + "tenant", Value: "default"},
			},
		},
	}

	ctx := &mockContect{}

	p := &pluginv2Runner{
		InputPipeContext: ctx,
	}

	p.ReceiveRawLog(logCtx)

	logContent := models.NewLogContents()
	logContent.Add(models.BodyKey, []byte("test content"))
	assert.Len(t, ctx.logs, 1)
	assert.Equal(t, models.PipelineGroupEvents{
		Group: models.NewGroup(models.NewMetadata(), models.NewTags()),
		Events: []models.PipelineEvent{
			&models.Log{
				Timestamp: uint64(timeSec) * 1e9,
				Offset:    10,
				Tags:      models.NewTagsWithKeyValues("tenant", "default"),
				Contents:  logContent,
			},
		},
	}, ctx.logs[0])
}

type mockContect struct {
	logs []models.PipelineGroupEvents
}

func (m *mockContect) Collector() pipeline.PipelineCollector {
	return &mockPipelineCollector{ctx: m}
}

type mockPipelineCollector struct {
	ctx *mockContect
	pipeline.PipelineCollector
}

func (m *mockPipelineCollector) Collect(groupInfo *models.GroupInfo, eventList ...models.PipelineEvent) {
	m.ctx.logs = append(m.ctx.logs, models.PipelineGroupEvents{Group: groupInfo, Events: eventList})
}

func TestPluginV2Runner_ReceivePipelineEventGroup_MetricMultiValue(t *testing.T) {
	ctx := &mockContect{}
	p := &pluginv2Runner{InputPipeContext: ctx}

	multiValue := models.NewMetricMultiValueWithMap(map[string]float64{"cpu": 0.5, "mem": 10})
	metric := models.NewMultiValuesMetric("agent", models.MetricTypeUntyped, models.NewTags(), 123, multiValue.Values)
	groupInfo := models.NewGroup(models.NewMetadata(), models.NewTagsWithKeyValues("env", "prod"))
	pbGroup, err := helper.TransferPipelineEventGroupToPB(groupInfo, []models.PipelineEvent{metric})
	assert.NoError(t, err)

	p.ReceivePipelineEventGroup(pbGroup, map[string]interface{}{"source": "pack-1"})
	assert.Len(t, ctx.logs, 1)
	m, ok := ctx.logs[0].Events[0].(*models.Metric)
	assert.True(t, ok)
	assert.True(t, m.GetValue().IsMultiValues())
	assert.Equal(t, 0.5, m.GetValue().GetMultiValues().Get("cpu"))
	assert.Equal(t, "pack-1", ctx.logs[0].Group.Metadata.Get("source"))
}

func TestPluginV2Runner_ReceivePipelineEventGroup_Span(t *testing.T) {
	ctx := &mockContect{}
	p := &pluginv2Runner{InputPipeContext: ctx}

	span := models.NewSpan("op", "trace", "span", models.SpanKindServer, 1, 2, models.NewTagsWithKeyValues("k", "v"), nil, nil)
	pbGroup, err := helper.TransferPipelineEventGroupToPB(models.NewGroup(models.NewMetadata(), models.NewTags()), []models.PipelineEvent{span})
	assert.NoError(t, err)

	p.ReceivePipelineEventGroup(pbGroup, nil)
	assert.Len(t, ctx.logs, 1)
	s, ok := ctx.logs[0].Events[0].(*models.Span)
	assert.True(t, ok)
	assert.Equal(t, "op", s.GetName())
}

func TestPluginV2Runner_ReceivePipelineEventGroup_Log(t *testing.T) {
	ctx := &mockContect{}
	p := &pluginv2Runner{InputPipeContext: ctx}

	log := models.NewLog("", []byte("alarm"), "", "", "", models.NewTags(), 200)
	pbGroup, err := helper.TransferPipelineEventGroupToPB(models.NewGroup(models.NewMetadata(), models.NewTags()), []models.PipelineEvent{log})
	assert.NoError(t, err)

	p.ReceivePipelineEventGroup(pbGroup, map[string]interface{}{"source": "alarm-pack"})
	assert.Len(t, ctx.logs, 1)
	_, ok := ctx.logs[0].Events[0].(*models.Log)
	assert.True(t, ok)
}

type timerMetricInput struct{}

func (*timerMetricInput) Init(pipeline.Context) (int, error) { return 0, nil }
func (*timerMetricInput) Description() string                { return "timer counter regression" }
func (*timerMetricInput) Read(ctx pipeline.PipelineContext) error {
	event := models.NewSingleValueMetric("timer_metric", models.MetricTypeCounter, models.NewTags(), time.Now().UnixNano(), 1)
	ctx.Collector().Collect(models.NewGroup(models.NewMetadata(), models.NewTags()), event)
	return nil
}

func TestPluginV2Runner_MetricTimerRecordsSourceCounters(t *testing.T) {
	ctx := &ContextImp{}
	ctx.InitContext("project", "store", "timer-pipeline")
	output := helper.NewObservePipelineContext(4)
	runner := &pluginv2Runner{
		LogstoreConfig:   &LogstoreConfig{Context: ctx, GlobalConfig: &config.GlobalConfig{}},
		InputPipeContext: output,
	}
	require.NoError(t, runner.addMetricInput(&pipeline.PluginMeta{PluginID: "1", PluginType: "timer_metric"}, &timerMetricInput{}, int(time.Hour/time.Millisecond)))
	require.Same(t, runner.MetricPlugins[0], runner.TimerRunner[0].state)
	// A non-input timer must not be mistaken for a failing input plugin.
	runner.TimerRunner = append(runner.TimerRunner, &timerRunner{state: struct{}{}})
	control := pipeline.NewAsyncControl()
	defer control.WaitCancel()
	runner.runMetricInput(control)
	select {
	case group := <-output.Collector().Observe():
		require.Len(t, group.Events, 1)
	case <-time.After(3 * time.Second):
		t.Fatal("metric input timer did not publish an event")
	}
	records := ctx.ExportMetricRecords()
	require.Len(t, records, 2)
	for _, record := range records {
		counters, category := parseExportedRecord(t, record)
		require.Contains(t, []string{selfmonitor.MetricLabelValueMetricCategoryPlugin, selfmonitor.MetricLabelValueMetricCategoryPluginSource}, category)
		assert.Equal(t, "1.0000", counters[selfmonitor.MetricPluginOutEventsTotal])
		assert.Equal(t, "1.0000", counters[selfmonitor.MetricPluginOutEventGroupsTotal])
	}
}

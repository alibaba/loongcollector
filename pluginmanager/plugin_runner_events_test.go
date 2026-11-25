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
	"fmt"
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
	"github.com/alibaba/ilogtail/plugins/processor/regex"
	"github.com/alibaba/ilogtail/plugins/processor/split/logstring"
)

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

func capturePipelineEventGroup(t *testing.T, pbGroup *protocol.PipelineEventGroup, packID string) *mockContect {
	t.Helper()
	wire, err := pbGroup.Marshal()
	require.NoError(t, err)

	ctx := &mockContect{}
	config := &LogstoreConfig{Context: &ContextImp{}}
	config.Context.InitContext("", "", "native-test")
	config.PluginRunner = &pluginRunner{LogstoreConfig: config, InputPipeContext: ctx}
	require.Equal(t, 0, config.ProcessPipelineEventGroup(wire, packID))
	return ctx
}

func TestEventRunner_ProcessPipelineEventGroup_MetricMultiValue(t *testing.T) {
	multiValue := models.NewMetricMultiValueWithMap(map[string]float64{"cpu": 0.5, "mem": 10})
	metric := models.NewMultiValuesMetric("agent", models.MetricTypeUntyped, models.NewTags(), 123, multiValue.Values)
	groupInfo := models.NewGroup(models.NewMetadata(), models.NewTagsWithKeyValues("env", "prod"))
	pbGroup, err := helper.TransferPipelineEventGroupToPB(groupInfo, []models.PipelineEvent{metric})
	require.NoError(t, err)

	ctx := capturePipelineEventGroup(t, pbGroup, "pack-1")
	require.Len(t, ctx.logs, 1)
	m, ok := ctx.logs[0].Events[0].(*models.Metric)
	assert.True(t, ok)
	assert.True(t, m.GetValue().IsMultiValues())
	assert.Equal(t, 0.5, m.GetValue().GetMultiValues().Get("cpu"))
	assert.Equal(t, "pack-1", ctx.logs[0].Group.Metadata.Get("source"))
}

func TestEventRunner_ProcessPipelineEventGroup_Span(t *testing.T) {
	span := models.NewSpan("op", "trace", "span", models.SpanKindServer, 1, 2, models.NewTagsWithKeyValues("k", "v"), nil, nil)
	pbGroup, err := helper.TransferPipelineEventGroupToPB(models.NewGroup(models.NewMetadata(), models.NewTags()), []models.PipelineEvent{span})
	require.NoError(t, err)

	ctx := capturePipelineEventGroup(t, pbGroup, "")
	require.Len(t, ctx.logs, 1)
	s, ok := ctx.logs[0].Events[0].(*models.Span)
	assert.True(t, ok)
	assert.Equal(t, "op", s.GetName())
}

func TestEventRunner_ProcessPipelineEventGroup_Log(t *testing.T) {
	log := models.NewLog("", []byte("alarm"), "", "", "", models.NewTags(), 200)
	pbGroup, err := helper.TransferPipelineEventGroupToPB(models.NewGroup(models.NewMetadata(), models.NewTags()), []models.PipelineEvent{log})
	require.NoError(t, err)

	ctx := capturePipelineEventGroup(t, pbGroup, "alarm-pack")
	require.Len(t, ctx.logs, 1)
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

func TestEventRunner_MetricTimerRecordsSourceCounters(t *testing.T) {
	ctx := &ContextImp{}
	ctx.InitContext("project", "store", "timer-pipeline")
	output := helper.NewObservePipelineContext(4)
	runner := &pluginRunner{
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

func TestEventRunner_ProcessorMetricsFollowCollectedOutput(t *testing.T) {
	for _, tt := range []struct {
		name      string
		processor pipeline.Processor
		body      string
		outGroups string
		outEvents string
		outSize   string
	}{
		{
			name:      "regex replaces the body with fields",
			processor: &regex.ProcessorRegex{Regex: "^([^:]+):(.+)$", SourceKey: models.BodyKey, Keys: []string{"level", "message"}},
			body:      "info:消息",
			outGroups: "1.0000",
			outEvents: "1.0000",
			outSize:   "29.0000", // level=info (9), message=消息 (13), env=prod (7)
		},
		{
			name:      "split emits replacement events",
			processor: &logstring.ProcessorSplit{SplitSep: ","},
			body:      "a,bb",
			outGroups: "1.0000",
			outEvents: "2.0000",
			outSize:   "24.0000", // Two content keys (14), three payload bytes, and one group tag (7).
		},
		{
			name:      "split drops empty events",
			processor: &logstring.ProcessorSplit{SplitSep: ","},
			body:      ",",
			outGroups: "0.0000",
			outEvents: "0.0000",
			outSize:   "0.0000",
		},
	} {
		t.Run(tt.name, func(t *testing.T) {
			ctx := &ContextImp{}
			ctx.InitContext("project", "store", "processor-pipeline")
			runner := &pluginRunner{
				LogstoreConfig:       &LogstoreConfig{Context: ctx, GlobalConfig: &config.GlobalConfig{}},
				InputPipeContext:     helper.NewObservePipelineContext(1),
				ProcessPipeContext:   helper.NewGroupedPipelineContext(),
				AggregatePipeContext: helper.NewObservePipelineContext(1),
				ProcessControl:       pipeline.NewAsyncControl(),
			}
			require.NoError(t, runner.addProcessor(&pipeline.PluginMeta{PluginID: "1", PluginType: "processor_test"}, tt.processor, 0))
			log := models.NewLogWithOrderedContents(1)
			models.SetOwnedLogBody(log, []byte(tt.body))
			runner.InputPipeContext.Collector().Collect(models.NewGroup(nil, models.NewTagsWithKeyValues("env", "prod")), log)
			runner.runProcessor()
			runner.ProcessControl.WaitCancel()

			records := ctx.ExportMetricRecords()
			require.Len(t, records, 1)
			counters, category := parseExportedRecord(t, records[0])
			assert.Equal(t, selfmonitor.MetricLabelValueMetricCategoryPlugin, category)
			assert.Equal(t, "1.0000", counters[selfmonitor.MetricPluginInEventGroupsTotal])
			assert.Equal(t, "1.0000", counters[selfmonitor.MetricPluginInEventsTotal])
			assert.Equal(t, fmt.Sprintf("%d.0000", len(models.BodyKey)+len(tt.body)+len("envprod")), counters[selfmonitor.MetricPluginInSizeBytes])
			assert.Equal(t, tt.outGroups, counters[selfmonitor.MetricPluginOutEventGroupsTotal])
			assert.Equal(t, tt.outEvents, counters[selfmonitor.MetricPluginOutEventsTotal])
			assert.Equal(t, tt.outSize, counters[selfmonitor.MetricPluginOutSizeBytes])
		})
	}
}

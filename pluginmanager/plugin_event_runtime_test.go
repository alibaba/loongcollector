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
	"math"
	"testing"
	"time"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
)

func TestDefaultEventPipeline(t *testing.T) {
	for _, input := range []map[string]interface{}{nil, {}, {"global": map[string]interface{}{}}, {"global": map[string]interface{}{"StructureType": "v2"}}} {
		require.NoError(t, validateStructureType(input))
	}
	for _, value := range []interface{}{"v1", "unknown", 1, nil} {
		require.Error(t, validateStructureType(map[string]interface{}{"global": map[string]interface{}{"StructureType": value}}))
	}
}
func TestProcessorDirectlyForwardsAllEventTypes(t *testing.T) {
	events := []models.PipelineEvent{
		models.NewLog("", []byte("log"), "", "", "", models.NewTags(), 1),
		models.NewSingleValueMetric("metric", models.MetricTypeCounter, models.NewTags(), 1, 1),
		models.NewSpan("span", "trace", "span", models.SpanKindServer, 1, 2, models.NewTags(), nil, nil),
	}
	for _, event := range events {
		t.Run(fmt.Sprint(event.GetType()), func(t *testing.T) {
			ctx := &ContextImp{}
			ctx.InitContext("project", "store", "direct")
			runner := &pluginRunner{LogstoreConfig: &LogstoreConfig{Context: ctx, GlobalConfig: &config.GlobalConfig{}}, FlushOutStore: NewFlushOutStore[models.PipelineGroupEvents]()}
			require.NoError(t, runner.Init(2, 2))
			require.Empty(t, runner.AggregatorPlugins)
			require.Empty(t, runner.TimerRunner)
			group := &models.PipelineGroupEvents{Group: models.NewGroup(models.NewMetadataWithKeyValues("source", "id"), models.NewTagsWithKeyValues("route", "test")), Events: []models.PipelineEvent{event}}
			runner.InputPipeContext.Collector().CollectList(group)
			runner.runProcessor()
			defer runner.ProcessControl.WaitCancel()
			select {
			case out := <-runner.AggregatePipeContext.Collector().Observe():
				require.Same(t, group, out)
				require.Same(t, event, out.Events[0])
				require.Equal(t, "id", out.Group.Metadata.Get("source"))
			case <-time.After(3 * time.Second):
				t.Fatal("processor dropped events without an aggregator")
			}
		})
	}
}
func TestCollectorInputEmitsNamedContents(t *testing.T) {
	ctx := helper.NewObservePipelineContext(4)
	collector := &inputCollector{context: ctx}
	collector.AddDataWithContext(map[string]string{"tag": "value"}, map[string]string{"content": "body", "field": "named"}, map[string]interface{}{"source": "id"}, time.Unix(10, 20))
	group := <-ctx.Collector().Observe()
	log := group.Events[0].(*models.Log)
	require.Equal(t, "named", log.Contents.Get("field"))
	require.Equal(t, "value", log.Contents.Get("tag"))
	require.Equal(t, []byte("body"), log.GetBody())
	require.Equal(t, uint64(10000000020), log.Timestamp)
}
func BenchmarkPipelineConfigInit(b *testing.B) {
	jsonConfig := `{"global":{"StructureType":"v2"},"flushers":[{"type":"flusher_stdout"}]}`
	b.ReportAllocs()
	for i := 0; i < b.N; i++ {
		if _, err := createLogstoreConfig("project", "store", "benchmark", 0, jsonConfig); err != nil {
			b.Fatal(err)
		}
	}
}

func TestRunnerRestoresPendingGroupsBeforeStart(t *testing.T) {
	ctx := &ContextImp{}
	ctx.InitContext("project", "store", "restore")
	store := NewFlushOutStore[models.PipelineGroupEvents]()
	for i := 0; i < 8; i++ {
		store.Add(&models.PipelineGroupEvents{Group: models.NewGroup(models.NewMetadata(), models.NewTags())})
	}
	runner := &pluginRunner{LogstoreConfig: &LogstoreConfig{Context: ctx, GlobalConfig: &config.GlobalConfig{}}, FlushOutStore: store}
	done := make(chan error, 1)
	go func() { done <- runner.Init(1, 1) }()
	select {
	case err := <-done:
		require.NoError(t, err)
	case <-time.After(3 * time.Second):
		t.Fatal("restoring unsent groups blocked before flusher startup")
	}
	require.Equal(t, 8, len(runner.AggregatePipeContext.Collector().Observe()))
	require.Zero(t, store.Len())
}

type collectorOnlyInput struct{}

func (*collectorOnlyInput) Init(pipeline.Context) (int, error) { return 0, nil }
func (*collectorOnlyInput) Description() string                { return "collector compatibility" }
func (*collectorOnlyInput) Collect(collector pipeline.Collector) error {
	collector.AddData(nil, map[string]string{"content": "body", "key": "value"})
	return nil
}
func TestCollectorOnlyInputUsesEventCounters(t *testing.T) {
	ctx := &ContextImp{}
	ctx.InitContext("project", "store", "collector")
	runner := &pluginRunner{LogstoreConfig: &LogstoreConfig{Context: ctx, GlobalConfig: &config.GlobalConfig{}}}
	require.NoError(t, runner.addMetricInput(&pipeline.PluginMeta{PluginID: "1", PluginType: "collector"}, &collectorOnlyInput{}, 1000))
	output := helper.NewObservePipelineContext(2)
	require.NoError(t, runner.MetricPlugins[0].Read(output))
	group := <-output.Collector().Observe()
	require.Equal(t, "value", group.Events[0].(*models.Log).Contents.Get("key"))
	records := ctx.ExportMetricRecords()
	require.Len(t, records, 2)
	for _, record := range records {
		counters, _ := parseExportedRecord(t, record)
		require.Equal(t, "1.0000", counters["out_events_total"])
	}
}
func TestCollectorMetricLogBecomesMetric(t *testing.T) {
	output := helper.NewObservePipelineContext(2)
	collector := &inputCollector{context: output}
	labels := &helper.MetricLabels{}
	labels.Append("city", "hz")
	collector.AddRawLog(helper.NewMetricLog("counter", time.Unix(10, 20).UnixNano(), 42, labels))
	event := (<-output.Collector().Observe()).Events[0]
	metric, ok := event.(*models.Metric)
	require.True(t, ok)
	require.Equal(t, "counter", metric.Name)
	require.Equal(t, "hz", metric.Tags.Get("city"))
	require.Equal(t, float64(42), metric.Value.GetSingleValue())
}

func TestCollectorStaleMetricPreservesNaNPayload(t *testing.T) {
	output := helper.NewObservePipelineContext(1)
	collector := &inputCollector{context: output}
	collector.AddRawLog(helper.NewMetricLog("stale", time.Now().UnixNano(), math.Float64frombits(helper.StaleNaN), &helper.MetricLabels{}))
	metric, ok := (<-output.Collector().Observe()).Events[0].(*models.Metric)
	require.True(t, ok)
	require.Equal(t, helper.StaleNaN, math.Float64bits(metric.Value.GetSingleValue()))
}

func TestNativeInputPreservesNamedFieldsAndContentTags(t *testing.T) {
	ctx := &ContextImp{}
	ctx.InitContext("project", "store", "native")
	runner := &pluginRunner{LogstoreConfig: &LogstoreConfig{Context: ctx, GlobalConfig: &config.GlobalConfig{UsingOldContentTag: true}}, FlushOutStore: NewFlushOutStore[models.PipelineGroupEvents]()}
	require.NoError(t, runner.Init(1, 1))
	runner.ReceivePipelineEventGroup(&protocol.PipelineEventGroup{
		Tags: map[string][]byte{"path": []byte("/logs/app.log")},
		PipelineEvents: &protocol.PipelineEventGroup_Logs{Logs: &protocol.PipelineEventGroup_LogEvents{Events: []*protocol.LogEvent{{
			Contents: []*protocol.LogEvent_Content{{Key: []byte("named"), Value: []byte("first")}, {Key: []byte("content"), Value: []byte("body")}},
		}}}},
	}, map[string]interface{}{"source": "pack"})
	group := <-runner.InputPipeContext.Collector().Observe()
	log := group.Events[0].(*models.Log)
	require.Equal(t, "first", log.Contents.Get("named"))
	require.Equal(t, []byte("body"), log.GetBody())
	require.Equal(t, "/logs/app.log", log.Contents.Get("__tag__:path"))
	require.Equal(t, "pack", group.Group.Metadata.Get("source"))
}

// Both Collector entry points must preserve fields before processors run and
// after conversion to the LogGroup wire protocol used by existing flushers.
func TestCollectorTagsRemainLogFields(t *testing.T) {
	for _, array := range []bool{false, true} {
		t.Run(fmt.Sprintf("array=%t", array), func(t *testing.T) {
			output := helper.NewObservePipelineContext(1)
			collector := &inputCollector{context: output, configTags: map[string]string{"configured": "value"}}
			tags := map[string]string{"_method_": "GET", "_image_name_": "example-container:latest"}
			ctx := map[string]interface{}{"source": "container", "tags": []*protocol.LogTag{{Key: "group", Value: "tag"}}}
			if array {
				collector.AddDataArrayWithContext(tags, []string{"content", "field"}, []string{"body", "value"}, ctx, time.Unix(10, 20))
			} else {
				collector.AddDataWithContext(tags, map[string]string{"content": "body", "field": "value"}, ctx, time.Unix(10, 20))
			}
			group := <-output.Collector().Observe()
			log := group.Events[0].(*models.Log)
			require.Equal(t, "GET", log.Contents.Get("_method_"))
			require.Equal(t, "example-container:latest", log.Contents.Get("_image_name_"))
			require.Equal(t, "value", log.Contents.Get("configured"))
			require.Zero(t, log.Tags.Len())
			require.Equal(t, "tag", group.Group.Tags.Get("group"))
			wire, err := converter.PipelineGroupEventsToLogGroup(group)
			require.NoError(t, err)
			fields := make(map[string]string)
			for _, field := range wire.Logs[0].Contents {
				fields[field.Key] = field.Value
			}
			require.Equal(t, "GET", fields["_method_"])
			require.Equal(t, "example-container:latest", fields["_image_name_"])
			require.Equal(t, "value", fields["configured"])
			require.Equal(t, "body", fields["content"])
		})
	}
}

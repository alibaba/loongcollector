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

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
	_ "github.com/alibaba/ilogtail/plugins/aggregator"
	_ "github.com/alibaba/ilogtail/plugins/aggregator/contentvaluegroup"
	_ "github.com/alibaba/ilogtail/plugins/aggregator/context"
	_ "github.com/alibaba/ilogtail/plugins/aggregator/logstorerouter"
	_ "github.com/alibaba/ilogtail/plugins/aggregator/metadatagroup"
	_ "github.com/alibaba/ilogtail/plugins/aggregator/opentelemetry"
	_ "github.com/alibaba/ilogtail/plugins/aggregator/shardhash"
	_ "github.com/alibaba/ilogtail/plugins/aggregator/skywalking"
	"github.com/alibaba/ilogtail/plugins/flusher/checker"
	_ "github.com/alibaba/ilogtail/plugins/processor/drop"
	_ "github.com/alibaba/ilogtail/plugins/processor/rename"
	_ "github.com/alibaba/ilogtail/plugins/processor/split/logregex"
	_ "github.com/alibaba/ilogtail/plugins/processor/split/logstring"
)

func newAggregatorTestConfig(t *testing.T, name, aggregator, detail, processors string) *LogstoreConfig {
	t.Helper()
	cfg, err := createLogstoreConfig("project", "logstore", name, 0, fmt.Sprintf(`{
		"global":{"AggregatIntervalMs":3600000},
		"processors":%s,
		"aggregators":[{"type":%q,"detail":%s}],
		"flushers":[{"type":"flusher_checker"}]
	}`, processors, aggregator, detail))
	require.NoError(t, err)
	return cfg
}

func aggregatorTestFlusher(cfg *LogstoreConfig) *checker.FlusherChecker {
	return GetConfigFlushers(cfg.PluginRunner)[0].(*checker.FlusherChecker)
}

func TestExplicitAggregatorsDeliverTypedEvents(t *testing.T) {
	cases := []struct {
		kind        string
		detail      string
		logRoute    string
		metricRoute string
		spanRoute   string
	}{
		{kind: "aggregator_default", detail: `{}`},
		{kind: "aggregator_base", detail: `{"MaxLogCount":1}`},
		{kind: "aggregator_context", detail: `{}`},
		{kind: "aggregator_metadata_group", detail: `{"GroupMetadataKeys":["source"],"GroupMaxEventLength":2}`},
		{kind: "aggregator_content_value_group", detail: `{"GroupKeys":["content"],"Topic":"selected"}`},
		{kind: "aggregator_shardhash", detail: `{"SourceKeys":["content"],"ShardCount":8}`},
		{kind: "aggregator_logstore_router", detail: `{"SourceKey":"content","RouterRegex":["message"],"RouterLogstore":["matched"]}`, logRoute: "matched"},
		{kind: "aggregator_opentelemetry", detail: `{"LogLogstore":"logs","MetricsLogstore":"metrics","TraceLogstore":"traces"}`, logRoute: "logs", metricRoute: "metrics", spanRoute: "traces"},
		{kind: "aggregator_skywalking", detail: `{"LogLogstore":"logs","MetricsLogstore":"metrics","TraceLogstore":"traces"}`, logRoute: "logs", metricRoute: "metrics", spanRoute: "traces"},
	}
	for _, tt := range cases {
		t.Run(tt.kind, func(t *testing.T) {
			cfg := newAggregatorTestConfig(t, tt.kind, tt.kind, tt.detail, "[]")
			run := cfg.PluginRunner.(*pluginRunner)
			require.Len(t, run.AggregatorPlugins, 1)
			info := models.NewGroup(models.NewMetadataWithKeyValues("source", "pack", "private", "metadata"), models.NewTagsWithKeyValues("host", "node"))
			const ts uint64 = 1700000000123456789
			log := models.NewSimpleLog([]byte("message"), models.NewTagsWithKeyValues("event", "tag"), ts)
			metric := models.NewSingleValueMetric("requests", models.MetricTypeCounter, models.NewTags(), int64(ts), 2)
			span := models.NewSpan("operation", "trace", "span", models.SpanKindServer, ts, ts+1, models.NewTags(), nil, nil)
			events := []models.PipelineEvent{log, metric, span}
			empty := &models.Log{Timestamp: ts}
			if tt.kind == "aggregator_content_value_group" || tt.kind == "aggregator_shardhash" || tt.kind == "aggregator_logstore_router" {
				events = append([]models.PipelineEvent{empty}, events...)
			}
			cfg.Start()
			run.InputPipeContext.Collector().Collect(info, events...)
			require.NoError(t, cfg.Stop(true))

			expected := map[models.PipelineEvent]string{log: tt.logRoute, metric: tt.metricRoute, span: tt.spanRoute}
			if len(events) > 3 {
				expected[empty] = ""
			}
			for _, group := range aggregatorTestFlusher(cfg).GroupEvents {
				require.Equal(t, "pack", group.Group.GetMetadata().Get("source"))
				require.Equal(t, "node", group.Group.GetTags().Get("host"))
				if tt.kind == "aggregator_metadata_group" {
					require.False(t, group.Group.GetMetadata().Contains("private"))
				} else {
					require.Equal(t, "metadata", group.Group.GetMetadata().Get("private"))
				}
				for _, event := range group.Events {
					require.Contains(t, expected, event, "event must be delivered once and preserve its type")
					require.Equal(t, expected[event], group.Group.GetDestinationLogstore())
					require.Equal(t, ts, event.GetTimestamp())
					delete(expected, event)
					if event == log {
						require.Equal(t, "tag", log.Tags.Get("event"))
						require.Equal(t, []byte("message"), log.GetBody())
						switch tt.kind {
						case "aggregator_content_value_group":
							require.Equal(t, "message", group.Group.GetTags().Get("content"))
							require.Equal(t, "selected", group.Group.GetTags().Get("__topic__"))
						case "aggregator_shardhash":
							require.Len(t, group.Group.GetTags().Get(util.ShardHashTagKey), 32)
						}
					}
				}
			}
			require.Empty(t, expected)
			require.Equal(t, map[string]string{"host": "node"}, info.GetTags().Iterator())
		})
	}
}

func TestRouterUsesInputFieldOrderAfterProcessors(t *testing.T) {
	cases := []struct {
		name      string
		processor string
		store     string
		input     func(t *testing.T) models.PipelineEvent
	}{
		{name: "array-drop", processor: `{"type":"processor_drop","detail":{"DropKeys":["z"]}}`, store: "second",
			input: func(t *testing.T) models.PipelineEvent {
				log, err := helper.NewInputLogRow(nil, []string{"z", "b", "a"}, []string{"first", "second", "third"})
				require.NoError(t, err)
				return log
			}},
		{name: "wire-rename", processor: `{"type":"processor_rename","detail":{"SourceKeys":["z"],"DestKeys":["y"]}}`, store: "first",
			input: func(t *testing.T) models.PipelineEvent {
				return helper.InputEventFromProtocolLog(&protocol.Log{Contents: []*protocol.Log_Content{{Key: "z", Value: "first"}, {Key: "b", Value: "second"}, {Key: "a", Value: "third"}}})
			}},
		{name: "typed-metric", processor: "", store: "metric",
			input: func(t *testing.T) models.PipelineEvent {
				return models.NewSingleValueMetric("metric", models.MetricTypeUntyped, models.NewTags(), time.Now().UnixNano(), 1)
			}},
	}
	for _, tt := range cases {
		t.Run(tt.name, func(t *testing.T) {
			cfg := newAggregatorTestConfig(t, tt.name, "aggregator_logstore_router",
				`{"DropDisMatch":true,"RouterRegex":["first","second","third","metric"],"RouterLogstore":["first","second","third","metric"]}`, "["+tt.processor+"]")
			run := cfg.PluginRunner.(*pluginRunner)
			cfg.Start()
			run.InputPipeContext.Collector().Collect(models.NewGroup(nil, nil), tt.input(t))
			require.NoError(t, cfg.Stop(true))
			groups := aggregatorTestFlusher(cfg).GroupEvents
			require.Len(t, groups, 1)
			require.Equal(t, tt.store, groups[0].Group.GetDestinationLogstore())
		})
	}
}

func TestAggregatorRetainsBatchesAcrossReload(t *testing.T) {
	const name = "aggregator-stop-reload"
	const detail = `{"SourceKey":"content","RouterRegex":["routed"],"RouterLogstore":["destination"]}`
	old := newAggregatorTestConfig(t, name, "aggregator_logstore_router", detail, "[]")
	aggregatorTestFlusher(old).Ready = false
	old.Start()
	first := models.NewSimpleLog([]byte("routed"), models.NewTagsWithKeyValues("event", "first"), 1700000000123456789)
	second := models.NewSimpleLog([]byte("fallback"), models.NewTagsWithKeyValues("event", "second"), 1700000000987654321)
	info := models.NewGroup(models.NewMetadataWithKeyValues("source", "pack"), models.NewTagsWithKeyValues("host", "agent"))
	old.PluginRunner.(*pluginRunner).InputPipeContext.Collector().Collect(info, first, second)
	require.NoError(t, old.Stop(false))
	require.Equal(t, 2, GetFlushStoreLen(old.PluginRunner))
	require.Zero(t, aggregatorTestFlusher(old).GetLogCount())

	LastUnsendBuffer[name] = old.PluginRunner
	t.Cleanup(func() { delete(LastUnsendBuffer, name) })
	current := newAggregatorTestConfig(t, name, "aggregator_logstore_router", detail, "[]")
	current.Start()
	require.NoError(t, current.Stop(true))

	restored := aggregatorTestFlusher(current).GroupEvents
	require.Len(t, restored, 2)
	require.Zero(t, GetFlushStoreLen(current.PluginRunner))
	for _, group := range restored {
		require.Equal(t, "pack", group.Group.GetMetadata().Get("source"))
		require.Equal(t, "agent", group.Group.GetTags().Get("host"))
		require.Len(t, group.Events, 1)
		switch event := group.Events[0]; event {
		case first:
			require.Equal(t, "destination", group.Group.GetDestinationLogstore())
			require.Equal(t, "first", event.GetTags().Get("event"))
			require.EqualValues(t, 1700000000123456789, event.GetTimestamp())
		case second:
			require.Empty(t, group.Group.GetDestinationLogstore())
			require.Equal(t, "second", event.GetTags().Get("event"))
			require.EqualValues(t, 1700000000987654321, event.GetTimestamp())
		default:
			t.Fatal("reloaded an unexpected event")
		}
	}
}

func TestContextAggregatorPreservesSourceOrderAndPackSequence(t *testing.T) {
	cfg := newAggregatorTestConfig(t, "source-batches", "aggregator_context", `{"MaxLogCount":2,"MaxLogGroupCount":2,"Topic":"configured"}`, "[]")
	cfg.Start()
	ctx := cfg.PluginRunner.(*pluginRunner).InputPipeContext
	sources := []string{"a", "b", "a-", "a"}
	hosts := []string{"one", "two", "three", "one"}
	for index, source := range sources {
		info := models.NewGroup(models.NewMetadataWithKeyValues("source", source), models.NewTagsWithKeyValues("host", hosts[index]))
		ctx.Collector().Collect(info, models.NewSimpleLog([]byte(fmt.Sprint(index)), models.NewTags(), uint64(index+1)))
	}
	require.NoError(t, cfg.Stop(true))
	var sourceA []string
	var timestamps []uint64
	for _, group := range aggregatorTestFlusher(cfg).GroupEvents {
		require.Len(t, group.Events, 1)
		require.Equal(t, "configured", group.Group.GetTags().Get("__topic__"))
		if group.Group.GetMetadata().Get("source") == "b" {
			require.Equal(t, "b-1", group.Group.GetTags().Get(util.PackIDTagKey))
			continue
		}
		sourceA = append(sourceA, group.Group.GetTags().Get(util.PackIDTagKey))
		timestamps = append(timestamps, group.Events[0].GetTimestamp())
		require.Equal(t, hosts[int(group.Events[0].GetTimestamp())-1], group.Group.GetTags().Get("host"))
	}
	require.Equal(t, []string{"a-1", "a-2", "a-3"}, sourceA)
	require.Equal(t, []uint64{1, 3, 4}, timestamps)
}

func TestSplitterPreservesDistinctBodiesAndSourceRouting(t *testing.T) {
	cases := []struct {
		name      string
		processor string
		keys      []string
		values    []string
		field     string
		want      map[string]string
	}{
		{
			name: "string", processor: `{"type":"processor_split_log_string","detail":{"SplitSep":"|","PreserveOthers":true}}`,
			keys: []string{"content"}, values: []string{"first|second"}, field: "content",
			want: map[string]string{"first": "one", "second": "two"},
		},
		{
			name: "string-preserve-first", processor: `{"type":"processor_split_log_string","detail":{"SplitKey":"content","SplitSep":"|","PreserveOthers":true}}`,
			keys: []string{"content", "z"}, values: []string{"first|second", "route"}, field: "content",
			want: map[string]string{"first": "preserved", "second": "preserved"},
		},
		{
			name: "regex", processor: `{"type":"processor_split_log_regex","detail":{"SplitKey":"payload","SplitRegex":"first|second","PreserveOthers":true}}`,
			keys: []string{"z", "content", "payload"}, values: []string{"route", "body", "first\nsecond"}, field: "payload",
			want: map[string]string{"first": "preserved", "second": "preserved"},
		},
	}
	for _, tt := range cases {
		t.Run(tt.name, func(t *testing.T) {
			cfg := newAggregatorTestConfig(t, "split-"+tt.name, "aggregator_logstore_router",
				`{"DropDisMatch":true,"RouterRegex":["first","second","route"],"RouterLogstore":["one","two","preserved"]}`,
				"["+tt.processor+"]")
			cfg.Start()
			run := cfg.PluginRunner.(*pluginRunner)
			log, err := helper.NewInputLogRow(map[string]string{"tenant": "test"}, tt.keys, tt.values, time.Unix(1700000000, 123456789))
			require.NoError(t, err)
			run.InputPipeContext.Collector().Collect(models.NewGroup(nil, nil), log)
			require.NoError(t, cfg.Stop(true))
			got := make(map[string]string)
			for _, group := range aggregatorTestFlusher(cfg).GroupEvents {
				for _, event := range group.Events {
					log := event.(*models.Log)
					require.Equal(t, "test", log.Tags.Get("tenant"))
					require.Equal(t, uint64(1700000000123456789), log.Timestamp)
					key := pipeline.GetStringValue(log.GetIndices().Get(tt.field))
					require.NotContains(t, got, key)
					got[key] = group.Group.GetDestinationLogstore()
				}
			}
			require.Equal(t, tt.want, got)
		})
	}
}

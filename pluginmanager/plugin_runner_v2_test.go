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

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
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
	src := &protocol.PipelineEventGroup{
		Tags: map[string][]byte{"env": []byte("test")},
		PipelineEvents: &protocol.PipelineEventGroup_Metrics{
			Metrics: &protocol.PipelineEventGroup_MetricEvents{
				Events: []*protocol.MetricEvent{
					{
						Timestamp: 1700000001,
						Name:      []byte("system_metrics"),
						Value: &protocol.MetricEvent_UntypedMultiDoubleValues{
							UntypedMultiDoubleValues: &protocol.UntypedMultiDoubleValues{
								Values: map[string]*protocol.UntypedMultiDoubleValue{
									"cpu": {Value: 0.6},
									"mem": {Value: 1024.0},
								},
							},
						},
					},
				},
			},
		},
	}

	ctx := &mockContect{}
	p := &pluginv2Runner{
		InputPipeContext: ctx,
	}

	p.ReceivePipelineEventGroup(src, nil)

	assert.Len(t, ctx.logs, 1)
	assert.Len(t, ctx.logs[0].Events, 1)
	metric, ok := ctx.logs[0].Events[0].(*models.Metric)
	assert.True(t, ok)
	assert.Equal(t, "system_metrics", metric.GetName())
	assert.Equal(t, "test", ctx.logs[0].Group.Tags.Get("env"))
}

func TestPluginV2Runner_ReceivePipelineEventGroup_Span(t *testing.T) {
	src := &protocol.PipelineEventGroup{
		PipelineEvents: &protocol.PipelineEventGroup_Spans{
			Spans: &protocol.PipelineEventGroup_SpanEvents{
				Events: []*protocol.SpanEvent{
					{
						Name:      []byte("http.request"),
						TraceID:   []byte("trace-001"),
						SpanID:    []byte("span-001"),
						StartTime: 1700000003000000000,
						EndTime:   1700000003500000000,
					},
				},
			},
		},
	}

	ctx := &mockContect{}
	p := &pluginv2Runner{
		InputPipeContext: ctx,
	}

	p.ReceivePipelineEventGroup(src, nil)

	assert.Len(t, ctx.logs, 1)
	assert.Len(t, ctx.logs[0].Events, 1)
	span, ok := ctx.logs[0].Events[0].(*models.Span)
	assert.True(t, ok)
	assert.Equal(t, "http.request", span.GetName())
	assert.Equal(t, "trace-001", span.GetTraceID())
}

func TestPluginV2Runner_ReceivePipelineEventGroup_Log(t *testing.T) {
	src := &protocol.PipelineEventGroup{
		PipelineEvents: &protocol.PipelineEventGroup_Logs{
			Logs: &protocol.PipelineEventGroup_LogEvents{
				Events: []*protocol.LogEvent{
					{
						Timestamp: 1700000004,
						Contents: []*protocol.LogEvent_Content{
							{Key: []byte("level"), Value: []byte("INFO")},
							{Key: []byte("msg"), Value: []byte("startup complete")},
						},
					},
				},
			},
		},
	}

	ctx := &mockContect{}
	p := &pluginv2Runner{
		InputPipeContext: ctx,
	}

	p.ReceivePipelineEventGroup(src, nil)

	assert.Len(t, ctx.logs, 1)
	assert.Len(t, ctx.logs[0].Events, 1)
	log, ok := ctx.logs[0].Events[0].(*models.Log)
	assert.True(t, ok)
	assert.Equal(t, "startup complete", log.Contents.Get("msg"))
}

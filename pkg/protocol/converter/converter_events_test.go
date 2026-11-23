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

package protocol

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
)

func TestConvertToSingleProtocolStreamMixedEvents(t *testing.T) {
	c, err := NewConverter(ProtocolCustomSingle, EncodingJSON, nil, nil)
	require.NoError(t, err)

	log := models.NewLog("", []byte("hello"), "info", "", "", models.NewTags(), 1e9)
	metric := models.NewSingleValueMetric("cpu", models.MetricTypeGauge, models.NewTagsWithKeyValues("host", "h1"), 2e9, 0.5)
	span := models.NewSpan("op", "trace-id", "span-id", models.SpanKindServer, 3e9, 4e9, models.NewTags(), nil, nil)
	groupEvents := &models.PipelineGroupEvents{
		Group:  models.NewGroup(models.NewMetadata(), models.NewTags()),
		Events: []models.PipelineEvent{log, metric, span},
	}

	stream, _, err := c.ConvertToSingleProtocolStream(groupEvents, nil)
	require.NoError(t, err)
	assert.Len(t, stream, 3)
	// Metric is converted structurally into a canonical metric log, never a
	// pass-through blob.
	assert.NotContains(t, string(stream[1]), "__pipeline_passthrough__")
	assert.Contains(t, string(stream[1]), metricNameKey)
	assert.Contains(t, string(stream[1]), `cpu`)
	assert.Contains(t, string(stream[1]), metricValueKey)
	// Span is converted structurally with its own fields.
	assert.NotContains(t, string(stream[2]), "__pipeline_passthrough__")
	assert.Contains(t, string(stream[2]), "traceID")
	assert.Contains(t, string(stream[2]), "trace-id")
}

func TestPipelineGroupEventsToLogGroupConvertsMetricStructurally(t *testing.T) {
	log := models.NewLog("", []byte("hello"), "info", "", "", models.NewTags(), 1)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 2, 1.0)
	groupEvents := &models.PipelineGroupEvents{
		Group:  models.NewGroup(models.NewMetadata(), models.NewTags()),
		Events: []models.PipelineEvent{log, metric},
	}

	logGroups, err := PipelineGroupEventsToLogGroups(groupEvents)
	require.NoError(t, err)
	require.Len(t, logGroups, 1)
	logGroup := logGroups[0]
	require.Len(t, logGroup.Logs, 2)

	metricContents := make(map[string]string, len(logGroup.Logs[1].Contents))
	for _, content := range logGroup.Logs[1].Contents {
		metricContents[content.GetKey()] = content.GetValue()
	}
	assert.Equal(t, "m", metricContents[metricNameKey])
	assert.Equal(t, "1", metricContents[metricValueKey])
	assert.NotContains(t, metricContents, "__pipeline_passthrough__")
}

func TestPipelineGroupEventsToLogGroupsPreservesSeparateTags(t *testing.T) {
	group := &models.PipelineGroupEvents{Group: models.NewGroup(models.NewMetadata(), models.NewTagsWithKeyValues("shared", "group"))}
	for _, value := range []string{"a", "a", "b", "a"} {
		group.Events = append(group.Events, models.NewSimpleLog([]byte(value), models.NewTagsWithKeyValues("container", value), 1))
	}
	wires, err := PipelineGroupEventsToLogGroups(group)
	require.NoError(t, err)
	require.Len(t, wires, 3)
	for i, wire := range wires {
		tags := map[string]string{}
		for _, tag := range wire.LogTags {
			tags[tag.Key] = tag.Value
		}
		require.Equal(t, []string{"a", "b", "a"}[i], tags["container"])
		require.Equal(t, "group", tags["shared"])
		require.Len(t, wire.Logs, []int{2, 1, 1}[i])
		for _, log := range wire.Logs {
			for _, field := range log.Contents {
				require.NotEqual(t, "container", field.Key)
			}
		}
	}
	require.False(t, group.Group.Tags.Contains("container"))
}

func TestPipelineGroupEventsToLogGroupsPreservesReservedTopic(t *testing.T) {
	tests := []struct {
		name     string
		tags     map[string]string
		topic    string
		wantTags map[string]string
	}{
		{name: "no tags"},
		{
			name:     "ordinary tags without topic",
			tags:     map[string]string{"host": "h1", "topic": "ordinary"},
			wantTags: map[string]string{"host": "h1", "topic": "ordinary"},
		},
		{
			name:  "topic only",
			tags:  map[string]string{"__topic__": "access"},
			topic: "access",
		},
		{
			name:     "topic with ordinary tags",
			tags:     map[string]string{"__topic__": "access", "host": "h1"},
			topic:    "access",
			wantTags: map[string]string{"host": "h1"},
		},
		{
			name:     "empty topic",
			tags:     map[string]string{"__topic__": "", "host": "h1"},
			wantTags: map[string]string{"host": "h1"},
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			groupInfo := models.NewGroup(models.NewMetadata(), models.NewTagsWithMap(tt.tags))
			log := models.NewLog("", []byte("body"), "", "", "", models.NewTags(), 1_000_000_000)
			pbGroup, err := helper.TransferPipelineEventGroupToPB(groupInfo, []models.PipelineEvent{log})
			require.NoError(t, err)

			incoming, err := helper.TransferPBToPipelineGroupEvents(pbGroup)
			require.NoError(t, err)
			groups, err := PipelineGroupEventsToLogGroups(incoming)
			require.NoError(t, err)
			require.Len(t, groups, 1)
			lg := groups[0]
			require.Equal(t, len(tt.tags), incoming.Group.Tags.Len())
			for k, v := range tt.tags {
				assert.Equal(t, v, incoming.Group.Tags.Get(k))
			}
			assert.Equal(t, tt.topic, lg.Topic)
			require.Len(t, lg.Logs, 1)
			assert.Equal(t, "body", lg.Logs[0].Contents[0].Value)
			require.Len(t, lg.LogTags, len(tt.wantTags))
			for _, tag := range lg.LogTags {
				value, ok := tt.wantTags[tag.Key]
				assert.True(t, ok, "unexpected tag %q", tag.Key)
				assert.Equal(t, value, tag.Value)
			}
		})
	}
}

func TestEventConverterSelectedFieldsAndRename(t *testing.T) {
	log := models.NewSimpleLog([]byte("<body>"), models.NewTagsWithKeyValues("tenant", "event", "omit", "private"), 1700000000123456789)
	log.Contents.Add("method", "PUT")
	group := &models.PipelineGroupEvents{Group: models.NewGroup(nil, models.NewTagsWithKeyValues("tenant", "group", "region", "cn")), Events: []models.PipelineEvent{log}}
	for _, protocolName := range []string{ProtocolCustomSingle, ProtocolCustomSingleFlatten, ProtocolJsonline} {
		t.Run(protocolName, func(t *testing.T) {
			converter, err := NewConverter(protocolName, EncodingJSON,
				map[string]string{"tenant": "renamed", "omit": ""},
				map[string]string{"contents": "fields", "time": "timestamp"})
			require.NoError(t, err)
			payload, selected, err := converter.ToByteStreamWithSelectedFields(group, []string{"content.method", "tag.tenant", "tag.region"})
			require.NoError(t, err)
			switch protocolName {
			case ProtocolCustomSingle:
				rows := payload.([][]byte)
				require.Len(t, rows, 1)
				require.JSONEq(t, `{"fields":{"content":"<body>","method":"PUT"},"tags":{"region":"cn","renamed":"group"},"timestamp":1700000000}`, string(rows[0]))
				require.Contains(t, string(rows[0]), "<body>")
			case ProtocolCustomSingleFlatten:
				require.JSONEq(t, `{"content":"<body>","method":"PUT","region":"cn","renamed":"group","timestamp":1700000000}`, string(payload.([][]byte)[0]))
			case ProtocolJsonline:
				require.JSONEq(t, `{"content":"<body>","method":"PUT","region":"cn","renamed":"group","timestamp":1700000000}`, string(payload.([]byte)))
				return
			}
			require.Equal(t, []map[string]string{{"content.method": "PUT", "tag.tenant": "group", "tag.region": "cn"}}, selected)
			_, _, err = converter.ToByteStreamWithSelectedFields(group, []string{"invalid.method"})
			require.ErrorContains(t, err, "unsupported field")
		})
	}
}

func TestLogGroupWireTimestamp(t *testing.T) {
	logs := []models.PipelineEvent{
		models.NewSimpleLog([]byte("seconds"), models.NewTags(), 1700000000000000000),
		models.NewSimpleLog([]byte("nanos"), models.NewTags(), 1700000000123456789),
	}
	group := &models.PipelineGroupEvents{Group: &models.GroupInfo{DestinationLogstore: "routed"}, Events: logs}
	wire, err := PipelineGroupEventsToLogGroups(group)
	require.NoError(t, err)
	require.Len(t, wire, 1)
	require.Equal(t, "routed", wire[0].Category)
	require.Equal(t, uint32(1700000000), wire[0].Logs[0].GetTime())
	require.Nil(t, wire[0].Logs[0].TimeNs)
	require.Equal(t, uint32(1700000000), wire[0].Logs[1].GetTime())
	require.Equal(t, uint32(123456789), wire[0].Logs[1].GetTimeNs())
}

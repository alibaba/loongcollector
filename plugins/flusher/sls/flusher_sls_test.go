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

//go:build linux || windows
// +build linux windows

package sls

import (
	"bytes"
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/encoding/protowire"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logtail"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

// capturedSend records both ABIs without requiring a live Logtail.
type capturedSend struct {
	destinations []string
	configName   string
	logstore     string
	pbBuffer     []byte
	lines        int
	hash         string
	calls        int
	pegCalls     int

	buffers [][]byte
}

func newTestFlusher(enableShardHash bool) (*SlsFlusher, *capturedSend) {
	captured := &capturedSend{}
	flusher := &SlsFlusher{
		EnableShardHash: enableShardHash,
		KeepShardHash:   true,
	}
	flusher.sendPipelineEventGroup = func(configName, logstore string, pbBuffer []byte, events int, hash string) int {
		captured.configName, captured.logstore = configName, logstore
		pbBuffer = bytes.Clone(pbBuffer)
		captured.pbBuffer, captured.lines, captured.hash = pbBuffer, events, hash
		captured.calls++
		captured.pegCalls++
		captured.buffers = append(captured.buffers, pbBuffer)
		captured.destinations = append(captured.destinations, logstore)
		return 0
	}
	// mock.NewEmptyContext(project, logstore, configName)
	_ = flusher.Init(mock.NewEmptyContext("test-project", "test-logstore", "test-config"))
	return flusher, captured
}

// Export must use the typed protobuf ABI and retain log contents.
func TestSlsFlusher_ExportConvertsLogEvent(t *testing.T) {
	flusher, captured := newTestFlusher(false)

	log := models.NewLog("", []byte(""), "", "", "", models.NewTags(), 100)
	log.GetIndices().Add("key1", "value1")
	groups := []*models.PipelineGroupEvents{
		{
			Group:  models.NewGroup(models.NewMetadata(), models.NewTags()),
			Events: []models.PipelineEvent{log},
		},
	}

	err := flusher.Export(groups, nil)
	require.NoError(t, err)

	assert.Equal(t, 1, captured.calls)
	assert.Equal(t, "test-config", captured.configName)
	assert.Equal(t, "test-logstore", captured.logstore)
	assert.Equal(t, 1, captured.lines)

	assert.Equal(t, 1, captured.pegCalls)

	var decoded protocol.PipelineEventGroup
	require.NoError(t, decoded.Unmarshal(captured.pbBuffer))
	require.NotNil(t, decoded.GetLogs())
	require.Len(t, decoded.GetLogs().Events, 1)
	found := false
	for _, c := range decoded.GetLogs().Events[0].Contents {
		if string(c.Key) == "key1" && string(c.Value) == "value1" {
			found = true
		}
	}
	assert.True(t, found, "converted log should preserve the index key/value")
}

func TestSlsFlusher_ExportPreservesReservedTopic(t *testing.T) {
	tests := []struct {
		name string
		tags map[string]string
	}{
		{name: "absent topic", tags: map[string]string{"host": "h1"}},
		{name: "empty topic", tags: map[string]string{"__topic__": "", "host": "h1"}},
		{name: "nonempty topic", tags: map[string]string{"__topic__": "access", "host": "h1"}},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			log := models.NewLog("", []byte("body"), "", "", "", models.NewTags(), 100)
			group := models.NewGroup(models.NewMetadata(), models.NewTagsWithMap(tt.tags))
			incoming, err := helper.TransferPipelineEventGroupToPB(group, []models.PipelineEvent{log})
			require.NoError(t, err)
			// Use the same conversion as the event runner before exporting back to C++.
			converted, err := helper.TransferPBToPipelineGroupEvents(incoming)
			require.NoError(t, err)
			flusher, captured := newTestFlusher(false)
			require.NoError(t, flusher.Export([]*models.PipelineGroupEvents{converted}, nil))
			require.Equal(t, 1, captured.pegCalls)

			var outgoing protocol.PipelineEventGroup
			require.NoError(t, outgoing.Unmarshal(captured.pbBuffer))
			tags := make(map[string]string)
			for k, v := range outgoing.Tags {
				tags[k] = string(v)
			}
			assert.Equal(t, tt.tags, tags)
			assert.NotContains(t, outgoing.Tags, "__tag__:__topic__")
			require.NotNil(t, outgoing.GetLogs())
			require.Len(t, outgoing.GetLogs().Events, 1)
			assert.Equal(t, incoming.GetLogs(), outgoing.GetLogs())
		})
	}
}

// TestSlsFlusher_ExportPassesThroughMetric verifies Metric events are not dropped
// by the v2 Export path: their original metric structure is retained.
func TestSlsFlusher_ExportPassesThroughMetric(t *testing.T) {
	flusher, captured := newTestFlusher(false)

	metric := models.NewSingleValueMetric("cpu.usage", models.MetricTypeGauge, models.NewTags(), 100, 3.14)
	groups := []*models.PipelineGroupEvents{
		{
			Group:  models.NewGroup(models.NewMetadata(), models.NewTags()),
			Events: []models.PipelineEvent{metric},
		},
	}

	err := flusher.Export(groups, nil)
	require.NoError(t, err)

	require.Equal(t, 1, captured.calls)
	var decoded protocol.PipelineEventGroup
	require.NoError(t, decoded.Unmarshal(captured.pbBuffer))
	require.NotNil(t, decoded.GetMetrics())
	require.Len(t, decoded.GetMetrics().Events, 1)
	assert.Equal(t, "cpu.usage", string(decoded.GetMetrics().Events[0].Name))
	assert.Equal(t, 3.14, decoded.GetMetrics().Events[0].GetUntypedSingleValue().Value)
}

// TestSlsFlusher_ExportEmptyGroupsNoSend verifies nil / empty groups do not
// trigger a send and do not error.
func TestSlsFlusher_ExportEmptyGroupsNoSend(t *testing.T) {
	flusher, captured := newTestFlusher(false)

	require.NoError(t, flusher.Export(nil, nil))
	require.NoError(t, flusher.Export([]*models.PipelineGroupEvents{nil}, nil))
	require.NoError(t, flusher.Export([]*models.PipelineGroupEvents{{
		Group:  models.NewGroup(models.NewMetadata(), models.NewTags()),
		Events: []models.PipelineEvent{},
	}}, nil))

	assert.Equal(t, 0, captured.calls, "empty groups must not push data into the send queue")
}

// TestSlsFlusher_ExportShardHash verifies that with EnableShardHash the shard
// hash tag flows from group tags into SendPipelineEventGroup.
func TestSlsFlusher_ExportShardHash(t *testing.T) {
	flusher, captured := newTestFlusher(true)

	tags := models.NewTags()
	tags.Add(util.ShardHashTagKey, "abc123")
	log := models.NewLog("", []byte(""), "", "", "", models.NewTags(), 100)
	log.GetIndices().Add("key1", "value1")
	groups := []*models.PipelineGroupEvents{
		{
			Group:  models.NewGroup(models.NewMetadata(), tags),
			Events: []models.PipelineEvent{log},
		},
	}

	err := flusher.Export(groups, nil)
	require.NoError(t, err)

	require.Equal(t, 1, captured.calls)
	assert.Equal(t, "abc123", captured.hash)
}

func TestSlsFlusher_ExportMixedEventsPreservesOrderAndMetadata(t *testing.T) {
	flusher, captured := newTestFlusher(false)
	log := models.NewLog("", nil, "", "", "", models.NewTags(), 123)
	log.GetIndices().Add("content", "hello")
	metric := models.NewSingleValueMetric("cpu", models.MetricTypeGauge, models.NewTags(), 456, 1.5)
	span := models.NewSpan("request", "trace", "span", models.SpanKind(1), 100, 200, models.NewTags(), nil, nil)
	metadata, tags := models.NewMetadata(), models.NewTags()
	metadata.Add("source_id", "source")
	tags.Add("host", "test-host")
	group := models.NewGroup(metadata, tags)
	require.NoError(t, flusher.Export([]*models.PipelineGroupEvents{{
		Group: group, Events: []models.PipelineEvent{log, log, metric, span, log},
	}}, nil))
	require.Len(t, captured.buffers, 4)
	for i, buf := range captured.buffers {
		var pb protocol.PipelineEventGroup
		require.NoError(t, pb.Unmarshal(buf))
		assert.Equal(t, []byte("source"), pb.Metadata["source_id"])
		assert.Equal(t, []byte("test-host"), pb.Tags["host"])
		switch i {
		case 0:
			require.NotNil(t, pb.GetLogs())
			require.Len(t, pb.GetLogs().Events, 2)
			assert.Equal(t, uint64(123), pb.GetLogs().Events[0].Timestamp)
		case 1:
			require.NotNil(t, pb.GetMetrics())
			require.Len(t, pb.GetMetrics().Events, 1)
		case 2:
			require.NotNil(t, pb.GetSpans())
			require.Len(t, pb.GetSpans().Events, 1)
			assert.Equal(t, []byte("trace"), pb.GetSpans().Events[0].TraceID)
		case 3:
			require.NotNil(t, pb.GetLogs())
			require.Len(t, pb.GetLogs().Events, 1)
		}
	}

}

func TestSlsFlusher_ExportShardHashDoesNotMutateInput(t *testing.T) {
	for _, enable := range []bool{false, true} {
		for _, keep := range []bool{false, true} {
			flusher, captured := newTestFlusher(enable)
			flusher.KeepShardHash = keep
			tags := models.NewTags()
			tags.Add(util.ShardHashTagKey, "hash")
			log := models.NewSimpleLog([]byte("message"), models.NewTags(), 1)
			require.NoError(t, flusher.Export([]*models.PipelineGroupEvents{{
				Group: models.NewGroup(models.NewMetadata(), tags), Events: []models.PipelineEvent{log},
			}}, nil))
			var pb protocol.PipelineEventGroup
			require.NoError(t, pb.Unmarshal(captured.pbBuffer))
			assert.Equal(t, !enable || keep, pb.Tags[util.ShardHashTagKey] != nil)
			if enable {
				assert.Equal(t, "hash", captured.hash)
			} else {
				assert.Empty(t, captured.hash)
			}
			assert.Equal(t, "hash", tags.Get(util.ShardHashTagKey))
		}
	}
}

func TestSlsFlusher_ExportKeepsLogTagsSeparate(t *testing.T) {
	flusher, captured := newTestFlusher(false)
	group := models.NewGroup(models.NewMetadataWithKeyValues("source", "pack"), models.NewTagsWithKeyValues("shared", "group"))
	var events []models.PipelineEvent
	for _, container := range []string{"a", "a", "b", "a"} {
		events = append(events, models.NewSimpleLog([]byte(container), models.NewTagsWithKeyValues("container", container), 100))
	}
	require.NoError(t, flusher.Export([]*models.PipelineGroupEvents{{Group: group, Events: events}}, nil))
	require.Len(t, captured.buffers, 3)
	for i, data := range captured.buffers {
		var pb protocol.PipelineEventGroup
		require.NoError(t, pb.Unmarshal(data))
		require.Equal(t, []byte([]string{"a", "b", "a"}[i]), pb.Tags["container"])
		require.Equal(t, []byte("group"), pb.Tags["shared"])
		require.Equal(t, []byte("pack"), pb.Metadata["source"])
		require.Len(t, pb.GetLogs().Events, []int{2, 1, 1}[i])
		for _, log := range pb.GetLogs().Events {
			for _, field := range log.Contents {
				require.NotEqual(t, "container", string(field.Key))
			}
		}
	}
	require.False(t, group.Tags.Contains("container"))
	require.Equal(t, "a", events[0].GetTags().Get("container"))
}

func TestSlsFlusher_ExportErrors(t *testing.T) {
	log := models.NewSimpleLog([]byte("message"), models.NewTags(), 1)
	log.Contents.Add("bad", 42)
	for _, event := range []models.PipelineEvent{nil, log, (*models.Log)(nil), models.ByteArray("unsupported")} {
		flusher, captured := newTestFlusher(false)
		require.Error(t, flusher.Export([]*models.PipelineGroupEvents{{Events: []models.PipelineEvent{event}}}, nil))
		assert.Zero(t, captured.calls)
	}
	flusher, _ := newTestFlusher(false)
	calls := 0
	flusher.sendPipelineEventGroup = func(_, _ string, _ []byte, _ int, _ string) int { calls++; return -2 }
	log = models.NewSimpleLog([]byte("message"), models.NewTags(), 1)
	require.ErrorContains(t, flusher.Export([]*models.PipelineGroupEvents{
		{Events: []models.PipelineEvent{log}}, {Events: []models.PipelineEvent{log}},
	}, nil), "error -2")
	assert.Equal(t, 1, calls)
}

func TestSlsFlusherExportUsesExplicitRouteAndIgnoresOrdinaryMetadata(t *testing.T) {
	flusher, capture := newTestFlusher(true)
	flusher.KeepShardHash = false
	var groups []*models.PipelineGroupEvents
	for _, destination := range []string{"first", "second", ""} {
		info := models.NewGroup(
			models.NewMetadataWithKeyValues("__logstore__", "ordinary-resource-attribute"),
			models.NewTagsWithKeyValues(util.ShardHashTagKey, "bucket"),
		)
		info.DestinationLogstore = destination
		log := models.NewSimpleLog([]byte("message"), models.NewTagsWithKeyValues("event", "tag"), 1700000000123456789)
		groups = append(groups, &models.PipelineGroupEvents{Group: info, Events: []models.PipelineEvent{log}})
	}

	require.NoError(t, flusher.Export(groups, nil))
	require.Equal(t, []string{"first", "second", "test-logstore"}, capture.destinations)
	require.Equal(t, "bucket", capture.hash)
	for i, buf := range capture.buffers {
		var wire protocol.PipelineEventGroup
		require.NoError(t, wire.Unmarshal(buf))
		require.Equal(t, []byte("tag"), wire.Tags["event"])
		require.NotContains(t, wire.Tags, util.ShardHashTagKey)
		require.Equal(t, "bucket", groups[i].Group.GetTags().Get(util.ShardHashTagKey))
		require.Equal(t, []byte("ordinary-resource-attribute"), wire.Metadata["__logstore__"])
		require.Equal(t, uint64(1700000000123456789), wire.GetLogs().Events[0].Timestamp)
	}
}

func TestSlsFlusherExportContinuesPastEmptyLogs(t *testing.T) {
	flusher, capture := newTestFlusher(false)
	empty := &models.Log{Timestamp: 1}
	valid := models.NewSimpleLog([]byte("valid"), models.NewTags(), 1700000000123456789)
	require.NoError(t, flusher.Export([]*models.PipelineGroupEvents{
		{Group: &models.GroupInfo{DestinationLogstore: "empty"}, Events: []models.PipelineEvent{empty}},
		{Group: &models.GroupInfo{DestinationLogstore: "valid"}, Events: []models.PipelineEvent{empty, valid}},
	}, nil))
	require.Equal(t, []string{"valid"}, capture.destinations)
	require.Equal(t, 1, capture.lines)
	var wire protocol.PipelineEventGroup
	require.NoError(t, wire.Unmarshal(capture.pbBuffer))
	require.Len(t, wire.GetLogs().Events, 1)
	require.Equal(t, valid.Timestamp, wire.GetLogs().Events[0].Timestamp)
}

func BenchmarkSlsFlusherExport(b *testing.B) {
	for _, mixed := range []bool{false, true} {
		name := "peg/logs"
		if mixed {
			name = "peg/mixed"
		}
		b.Run(name, func(b *testing.B) {
			flusher := &SlsFlusher{sendPipelineEventGroup: func(_, _ string, _ []byte, _ int, _ string) int { return 0 }}
			require.NoError(b, flusher.Init(mock.NewEmptyContext("project", "store", "config")))
			events := make([]models.PipelineEvent, 100)
			for i := range events {
				if mixed && i%2 == 0 {
					events[i] = models.NewSingleValueMetric("cpu", models.MetricTypeGauge, models.NewTags(), 100, 1.5)
				} else {
					log := models.NewLog("", nil, "", "", "", models.NewTags(), 100)
					log.GetIndices().Add("content", "benchmark log payload")
					events[i] = log
				}
			}
			groups := []*models.PipelineGroupEvents{{Group: models.NewGroup(models.NewMetadata(), models.NewTags()), Events: events}}
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				if err := flusher.Export(groups, nil); err != nil {
					b.Fatal(err)
				}
			}
		})
	}
}

func TestMarshalLogGroupMatchesGeneratedEncoding(t *testing.T) {
	log := &models.Log{
		Timestamp: 1700000001234567890,
		Level:     "warn",
		Offset:    65001,
		RawSize:   65536,
		Contents:  models.NewOrderedLogContents(4),
	}
	log.GetIndices().Add("message", "a UTF-8 message 日志")
	log.GetIndices().Add("empty", "")
	log.GetIndices().Add("", []byte{0xff, 0})
	log.GetIndices().Add("data", []byte{})
	header := &protocol.PipelineEventGroup{
		Tags:     map[string][]byte{"__topic__": []byte("test"), "": {}},
		Metadata: map[string][]byte{"source": []byte("input")},
	}
	var buffer logWireBuffer
	encoded, count, err := buffer.marshalLogGroup(header, []models.PipelineEvent{
		&models.Log{}, log,
	})
	require.NoError(t, err)
	require.Equal(t, 1, count)
	referenceLog, err := helper.TransferLogEventToPB(log)
	require.NoError(t, err)
	reference := &protocol.PipelineEventGroup{
		Tags:     header.Tags,
		Metadata: header.Metadata,
		PipelineEvents: &protocol.PipelineEventGroup_Logs{
			Logs: &protocol.PipelineEventGroup_LogEvents{Events: []*protocol.LogEvent{referenceLog}},
		},
	}
	originalWire, err := reference.Marshal()
	require.NoError(t, err)
	var actual, expected protocol.PipelineEventGroup
	require.NoError(t, actual.Unmarshal(encoded))
	require.NoError(t, expected.Unmarshal(originalWire))
	require.True(t, proto.Equal(&expected, &actual))
	// An explicit empty value means the native serializer can borrow
	// the same content bytes when writing the required SLS fields.
	for _, field := range actual.GetLogs().Events[0].Contents {
		require.NotNil(t, field.Key)
		require.NotNil(t, field.Value)
	}
}

func TestWireMessageBackfillsCanonicalLength(t *testing.T) {
	for _, size := range []int{0, 127, 128, 16383, 16384, 2097152} {
		body := bytes.Repeat([]byte{0xFE}, size)
		for _, reserved := range []int{2, 3} {
			wire, lengthAt := beginWireMessage([]byte("header"), logEventField, reserved)
			wire = finishWireMessage(append(wire, body...), lengthAt, reserved)
			require.Equal(t, "header", string(wire[:6]))
			number, kind, used := protowire.ConsumeTag(wire[6:])
			require.EqualValues(t, logEventField, number)
			require.Equal(t, protowire.BytesType, kind)
			actual, consumed := protowire.ConsumeBytes(wire[6+used:])
			require.Equal(t, protowire.SizeBytes(size), consumed)
			require.Equal(t, body, actual)
		}
	}
}

func TestSlsFlusherSplitsOversizedPipelineBatch(t *testing.T) {
	flusher, _ := newTestFlusher(true)
	tags := models.NewTagsWithMap(map[string]string{util.ShardHashTagKey: "hash", "__topic__": "topic"})
	group := models.NewGroup(models.NewMetadata(), tags)
	group.DestinationLogstore = "routed"
	var sent []string
	var largeAttempts int
	flusher.sendPipelineEventGroup = func(_, logstore string, wire []byte, count int, hash string) int {
		var out protocol.PipelineEventGroup
		require.NoError(t, out.Unmarshal(wire))
		require.Equal(t, "routed", logstore)
		require.Equal(t, "hash", hash)
		require.Equal(t, "topic", string(out.Tags["__topic__"]))
		require.Len(t, out.GetLogs().Events, count)
		if count > 1 {
			largeAttempts++
			return logtail.SendPipelineEventGroupTooLarge
		}
		value := string(out.GetLogs().Events[0].GetContents()[0].GetValue())
		if value == "single event over limit" {
			return logtail.SendPipelineEventGroupTooLarge
		}
		sent = append(sent, value)
		return 0
	}
	batch := &models.PipelineGroupEvents{Group: group}
	for _, value := range []string{"first", "single event over limit", "last"} {
		batch.Events = append(batch.Events, helper.NewInputLog(nil, map[string]string{"content": value}))
	}

	require.NoError(t, flusher.Export([]*models.PipelineGroupEvents{batch}, nil))
	require.Positive(t, largeAttempts)
	require.Equal(t, []string{"first", "last"}, sent)
}

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
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

// capturedSend records both ABIs without requiring a live Logtail.
type capturedSend struct {
	configName  string
	logstore    string
	pbBuffer    []byte
	lines       int
	hash        string
	calls       int
	pegCalls    int
	legacyCalls int
	buffers     [][]byte
}

func newTestFlusher(enableShardHash bool) (*SlsFlusher, *capturedSend) {
	captured := &capturedSend{}
	flusher := &SlsFlusher{
		EnableShardHash: enableShardHash,
		KeepShardHash:   true,
		sendPb: func(configName, logstore string, pbBuffer []byte, lines int) int {
			captured.configName = configName
			captured.logstore = logstore
			captured.pbBuffer = pbBuffer
			captured.lines = lines
			captured.calls++
			captured.legacyCalls++
			return 0
		},
		sendPbV2: func(configName, logstore string, pbBuffer []byte, lines int, hash string) int {
			captured.configName = configName
			captured.logstore = logstore
			captured.pbBuffer = pbBuffer
			captured.lines = lines
			captured.hash = hash
			captured.calls++
			captured.legacyCalls++
			return 0
		},
	}
	flusher.sendPipelineEventGroup = func(configName, logstore string, pbBuffer []byte, events int, hash string) int {
		captured.configName, captured.logstore = configName, logstore
		captured.pbBuffer, captured.lines, captured.hash = pbBuffer, events, hash
		captured.calls++
		captured.pegCalls++
		captured.buffers = append(captured.buffers, pbBuffer)
		return 0
	}
	// mock.NewEmptyContext(project, logstore, configName)
	_ = flusher.Init(mock.NewEmptyContext("test-project", "test-logstore", "test-config"))
	return flusher, captured
}

// TestSlsFlusher_ImplementsFlusherV2 is a compile-and-runtime guard that the SLS
// flusher can be loaded by the v2 Runner.
func TestSlsFlusher_ImplementsFlusherV2(t *testing.T) {
	flusher, _ := newTestFlusher(false)
	require.Implements(t, (*pipeline.FlusherV2)(nil), flusher, "SlsFlusher must be loadable by the v2 Runner")
	require.Implements(t, (*pipeline.FlusherV1)(nil), flusher, "SlsFlusher must remain loadable by the v1 Runner")
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
	assert.Zero(t, captured.legacyCalls)
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
	assert.Zero(t, captured.legacyCalls)
}

func TestSlsFlusher_ExportShardHashDoesNotMutateInput(t *testing.T) {
	for _, enable := range []bool{false, true} {
		for _, keep := range []bool{false, true} {
			flusher, captured := newTestFlusher(enable)
			flusher.KeepShardHash = keep
			tags := models.NewTags()
			tags.Add(util.ShardHashTagKey, "hash")
			log := models.NewLog("", nil, "", "", "", models.NewTags(), 1)
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

func TestSlsFlusher_ExportErrors(t *testing.T) {
	log := models.NewLog("", nil, "", "", "", models.NewTags(), 1)
	log.Contents.Add("bad", 42)
	for _, event := range []models.PipelineEvent{nil, log, (*models.Log)(nil), models.ByteArray("unsupported")} {
		flusher, captured := newTestFlusher(false)
		require.Error(t, flusher.Export([]*models.PipelineGroupEvents{{Events: []models.PipelineEvent{event}}}, nil))
		assert.Zero(t, captured.calls)
	}
	flusher, _ := newTestFlusher(false)
	calls := 0
	flusher.sendPipelineEventGroup = func(_, _ string, _ []byte, _ int, _ string) int { calls++; return -2 }
	log = models.NewLog("", nil, "", "", "", models.NewTags(), 1)
	require.ErrorContains(t, flusher.Export([]*models.PipelineGroupEvents{
		{Events: []models.PipelineEvent{log}}, {Events: []models.PipelineEvent{log}},
	}, nil), "error -2")
	assert.Equal(t, 1, calls)
}

func TestSlsFlusher_FlushRetainsLegacyABI(t *testing.T) {
	for _, enable := range []bool{false, true} {
		flusher, captured := newTestFlusher(enable)
		group := &protocol.LogGroup{Category: "legacy-store", Logs: []*protocol.Log{{Time: 100}},
			LogTags: []*protocol.LogTag{{Key: util.ShardHashTagKey, Value: "hash"}}}
		require.NoError(t, flusher.Flush("project", "ignored", "legacy-config", []*protocol.LogGroup{group}))
		assert.Equal(t, 1, captured.legacyCalls)
		assert.Zero(t, captured.pegCalls)
		assert.Equal(t, "legacy-store", captured.logstore)
		var pb protocol.LogGroup
		require.NoError(t, pb.Unmarshal(captured.pbBuffer))
		assert.Equal(t, uint32(100), pb.Logs[0].Time)
	}
}

func BenchmarkSlsFlusherExport(b *testing.B) {
	for _, mixed := range []bool{false, true} {
		name := "logs"
		if mixed {
			name = "mixed"
		}
		b.Run(name, func(b *testing.B) {
			flusher := &SlsFlusher{sendPipelineEventGroup: func(_, _ string, _ []byte, _ int, _ string) int { return 0 }}
			if err := flusher.Init(mock.NewEmptyContext("project", "store", "config")); err != nil {
				b.Fatal(err)
			}
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

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

// capturedSend records the arguments a Send*Pb call was made with, so tests can
// assert the v2 Export path builds the right LogGroup without a live Logtail.
type capturedSend struct {
	configName string
	logstore   string
	pbBuffer   []byte
	lines      int
	hash       string
	calls      int
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
			return 0
		},
		sendPbV2: func(configName, logstore string, pbBuffer []byte, lines int, hash string) int {
			captured.configName = configName
			captured.logstore = logstore
			captured.pbBuffer = pbBuffer
			captured.lines = lines
			captured.hash = hash
			captured.calls++
			return 0
		},
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

// TestSlsFlusher_ExportConvertsLogEvent verifies a v2 Log event is converted to a
// legacy LogGroup and pushed through the SendPb path with config/logstore filled.
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
	// Converter leaves Category empty; Export fills it from the context logstore.
	assert.Equal(t, "test-logstore", captured.logstore)
	assert.Equal(t, 1, captured.lines)

	var decoded protocol.LogGroup
	require.NoError(t, decoded.Unmarshal(captured.pbBuffer))
	require.Len(t, decoded.Logs, 1)
	found := false
	for _, c := range decoded.Logs[0].Contents {
		if c.Key == "key1" && c.Value == "value1" {
			found = true
		}
	}
	assert.True(t, found, "converted log should preserve the index key/value, got %+v", decoded.Logs[0].Contents)
}

// TestSlsFlusher_ExportPassesThroughMetric verifies Metric events are not dropped
// by the v2 Export path: they are structurally converted and sent.
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
	var decoded protocol.LogGroup
	require.NoError(t, decoded.Unmarshal(captured.pbBuffer))
	require.GreaterOrEqual(t, len(decoded.Logs), 1, "metric must be converted to at least one log, never dropped")

	hasName := false
	for _, c := range decoded.Logs[0].Contents {
		if c.Key == "__name__" && c.Value == "cpu.usage" {
			hasName = true
		}
	}
	assert.True(t, hasName, "metric should be emitted structurally with __name__, got %+v", decoded.Logs[0].Contents)
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
// hash tag flows from the group tags into SendPbV2.
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
	assert.Equal(t, "abc123", captured.hash, "shard hash tag should be forwarded to SendPbV2")
}

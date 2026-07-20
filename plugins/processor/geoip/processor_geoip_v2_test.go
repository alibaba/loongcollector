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

package geoip

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
)

// The plugin ships no GeoIP database, so a db-backed transform cannot be
// exercised in a unit test (the v1 geoip_test.go is empty for the same reason).
// These tests cover the observable v2 behaviors that do not require a database:
// the p.db == nil no-op guard (mirroring the v1 ProcessLogs guard) and the
// mandatory Metric pass-through.

func TestProcessorGeoIP_ProcessV2NilDBIsNoOp(t *testing.T) {
	// db is nil: the v1 path returns logArray unchanged; the v2 path must
	// likewise leave the log untouched while still emitting it.
	processor := &ProcessorGeoIP{SourceKey: "ip", KeepSource: true, Language: "zh-CN"}

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("ip", "1.2.3.4")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.Equal(t, "1.2.3.4", contents.Get("ip"))
	assert.Equal(t, 1, contents.Len(), "no geo fields must be added when the database is not loaded")

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Log)
	assert.True(t, ok, "log event must pass through even when db is nil")
}

func TestProcessorGeoIP_ProcessV2PassesThroughMetric(t *testing.T) {
	processor := &ProcessorGeoIP{SourceKey: "ip", KeepSource: true, Language: "zh-CN"}

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Metric)
	assert.True(t, ok, "metric event must pass through unchanged")
}

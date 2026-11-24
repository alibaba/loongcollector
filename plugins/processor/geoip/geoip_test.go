// Copyright 2021 iLogtail Authors
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
	"path/filepath"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestProcessorGeoIPInitRequiresSourceKey(t *testing.T) {
	processor := &ProcessorGeoIP{DBPath: filepath.Join(t.TempDir(), "missing.mmdb")}
	ctx := mock.NewEmptyContext("p", "l", "c")
	require.ErrorContains(t, processor.Init(ctx), "SourceKey")

	for _, key := range []string{"ip", "__source__"} {
		processor.SourceKey = key
		require.ErrorContains(t, processor.Init(ctx), "missing.mmdb")
	}
}

// The plugin ships no GeoIP database, so a db-backed transform cannot be
// exercised without a database fixture.
// the p.db == nil no-op guard and the
// mandatory Metric pass-through.

func TestProcessorGeoIP_ProcessNilDBIsNoOp(t *testing.T) {
	// A missing database leaves the log unchanged.
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

func TestProcessorGeoIP_ProcessPassesThroughMetric(t *testing.T) {
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

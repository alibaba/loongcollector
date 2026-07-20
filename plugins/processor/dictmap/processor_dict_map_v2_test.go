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

package dictmap

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newDictMapV2(t *testing.T, p *ProcessorDictMap) *ProcessorDictMap {
	if p.MaxDictSize == 0 {
		p.MaxDictSize = 1000
	}
	if p.Mode == "" {
		p.Mode = "overwrite"
	}
	require.NoError(t, p.Init(mock.NewEmptyContext("p", "l", "c")))
	return p
}

func TestProcessorDictMap_ProcessV2InPlace(t *testing.T) {
	processor := newDictMapV2(t, &ProcessorDictMap{SourceKey: "k", MapDict: map[string]string{"a": "b"}})

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("k", "a")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.Equal(t, "b", log.GetIndices().Get("k"))
}

func TestProcessorDictMap_ProcessV2ToDestKey(t *testing.T) {
	processor := newDictMapV2(t, &ProcessorDictMap{SourceKey: "k", DestKey: "d", MapDict: map[string]string{"a": "b"}})

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("k", "a")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.Equal(t, "a", log.GetIndices().Get("k"), "source is preserved")
	assert.Equal(t, "b", log.GetIndices().Get("d"))
}

func TestProcessorDictMap_ProcessV2MissingHandled(t *testing.T) {
	processor := newDictMapV2(t, &ProcessorDictMap{SourceKey: "k", DestKey: "d", MapDict: map[string]string{"a": "b"}, HandleMissing: true, Missing: "Unknown"})

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("other", "x")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.Equal(t, "Unknown", log.GetIndices().Get("d"))
}

func TestProcessorDictMap_ProcessV2PassesThroughMetric(t *testing.T) {
	processor := newDictMapV2(t, &ProcessorDictMap{SourceKey: "k", MapDict: map[string]string{"a": "b"}})

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Metric)
	assert.True(t, ok, "metric event must pass through unchanged")
}

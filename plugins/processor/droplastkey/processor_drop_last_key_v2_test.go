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

package droplastkey

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newV2Processor(t *testing.T) *ProcessorDropLastKey {
	processor := &ProcessorDropLastKey{Include: []string{"trigger"}, DropKey: "drop_me"}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))
	return processor
}

func TestProcessorDropLastKey_ProcessV2DropsWhenIncludePresent(t *testing.T) {
	processor := newV2Processor(t)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("trigger", "1")
	log.GetIndices().Add("drop_me", "x")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.False(t, log.GetIndices().Contains("drop_me"), "DropKey must be removed when an Include key exists")
	assert.True(t, log.GetIndices().Contains("trigger"))
}

func TestProcessorDropLastKey_ProcessV2KeepsWhenIncludeAbsent(t *testing.T) {
	processor := newV2Processor(t)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("drop_me", "x")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.True(t, log.GetIndices().Contains("drop_me"), "DropKey must be kept when no Include key exists")
}

// TestProcessorDropLastKey_ProcessV2PassesThroughMetric verifies Metric events
// are passed through by the log-only processor.
func TestProcessorDropLastKey_ProcessV2PassesThroughMetric(t *testing.T) {
	processor := newV2Processor(t)

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	group := &models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}

	context := helper.NewObservePipelineContext(10)
	processor.Process(group, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Metric)
	assert.True(t, ok, "metric event must pass through unchanged")
}

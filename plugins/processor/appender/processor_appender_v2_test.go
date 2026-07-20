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

package appender

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newAppenderV2(t *testing.T, key, value string) *ProcessorAppender {
	processor := pipeline.Processors["processor_appender"]().(*ProcessorAppender)
	processor.Key = key
	processor.Value = value
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))
	return processor
}

func TestProcessorAppender_ProcessV2AppendsToExistingKey(t *testing.T) {
	processor := newAppenderV2(t, "k", "_suffix")

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("k", "base")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.Equal(t, "base_suffix", log.GetIndices().Get("k"))
}

func TestProcessorAppender_ProcessV2CreatesMissingKey(t *testing.T) {
	processor := newAppenderV2(t, "k", "_suffix")

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.Equal(t, "_suffix", log.GetIndices().Get("k"))
}

func TestProcessorAppender_ProcessV2PassesThroughMetric(t *testing.T) {
	processor := newAppenderV2(t, "k", "_suffix")

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Metric)
	assert.True(t, ok, "metric event must pass through unchanged")
}

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

package logstring

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newSplitLogStringV2(t *testing.T) *ProcessorSplit {
	processor := pipeline.Processors["processor_split_log_string"]().(*ProcessorSplit)
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))
	return processor
}

func TestProcessorSplitLogString_ProcessV2SplitsLog(t *testing.T) {
	processor := newSplitLogStringV2(t)

	log := models.NewSimpleLog([]byte("hello1\nhello2\nhello3"), models.NewTags(), 0)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	results := context.Collector().ToArray()
	var logs int
	for _, group := range results {
		for _, event := range group.Events {
			_, ok := event.(*models.Log)
			assert.True(t, ok, "split result must be Log events")
			logs++
		}
	}
	assert.Equal(t, 3, logs, "input log must be split into 3 logs")
}

func TestProcessorSplitLogString_ProcessV2PassesThroughMetric(t *testing.T) {
	processor := newSplitLogStringV2(t)

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Metric)
	assert.True(t, ok, "metric event must pass through unchanged")
}

func TestProcessorSplitLogString_ProcessV2PassesThroughSpan(t *testing.T) {
	processor := newSplitLogStringV2(t)

	span := models.NewSpan("s", "trace", "span", models.SpanKindClient, 0, 0, models.NewTags(), nil, nil)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{span}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Span)
	assert.True(t, ok, "span event must pass through unchanged")
}

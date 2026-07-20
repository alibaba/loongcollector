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

package logregex

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

// newSplitLogRegexV2 builds the processor via its registered factory and
// applies the given config, mirroring the SplitRegex used by the v1 tests.
func newSplitLogRegexV2(t *testing.T, splitRegex, splitKey string, preserveOthers bool) *ProcessorSplitRegex {
	processor := pipeline.Processors["processor_split_log_regex"]().(*ProcessorSplitRegex)
	processor.SplitRegex = splitRegex
	processor.SplitKey = splitKey
	processor.PreserveOthers = preserveOthers
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))
	return processor
}

func newLogWithContent(key, value string) *models.Log {
	contents := models.NewLogContents()
	contents.Add(key, value)
	return &models.Log{Contents: contents}
}

// TestProcessorSplitLogRegex_ProcessV2SplitsLog asserts that a single Log whose
// SplitKey holds a multiline value is split into the expected number of Logs
// with the same segment values as the v1 processor (see TestMultiLine).
func TestProcessorSplitLogRegex_ProcessV2SplitsLog(t *testing.T) {
	processor := newSplitLogRegexV2(t, "\\[.*", "content", false)

	raw := "[2017-12-12 00:00:00] 你好\nhello\n\n[2017xxxxxx]yyyy\n [zzzz\n["
	log := newLogWithContent("content", raw)

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)

	values := make([]string, 0, len(results[0].Events))
	for _, event := range results[0].Events {
		logEvent, ok := event.(*models.Log)
		require.True(t, ok, "split result must be Log events")
		v, ok := logEvent.GetIndices().Get("content").(string)
		require.True(t, ok)
		values = append(values, v)
	}

	assert.Equal(t, []string{
		"[2017-12-12 00:00:00] 你好\nhello\n",
		"[2017xxxxxx]yyyy\n [zzzz",
		"[",
	}, values)
}

// TestProcessorSplitLogRegex_ProcessV2PassesThroughMetric asserts that the 1->N
// split still emits every produced Log while a Metric event in the same group
// passes through unchanged (never dropped).
func TestProcessorSplitLogRegex_ProcessV2PassesThroughMetric(t *testing.T) {
	processor := newSplitLogRegexV2(t, "\\[.*", "content", false)

	raw := "[2017-12-12 00:00:00] 你好\nhello\n\n[2017xxxxxx]yyyy\n [zzzz\n["
	log := newLogWithContent("content", raw)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log, metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)

	var logs, metrics int
	var passedMetric *models.Metric
	for _, event := range results[0].Events {
		switch e := event.(type) {
		case *models.Log:
			logs++
		case *models.Metric:
			metrics++
			passedMetric = e
		}
	}

	assert.Equal(t, 3, logs, "input log must be split into 3 logs")
	assert.Equal(t, 1, metrics, "metric event must pass through unchanged")
	require.NotNil(t, passedMetric)
	assert.Equal(t, "m", passedMetric.GetName())
}

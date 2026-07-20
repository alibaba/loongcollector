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

package grok

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newV2Processor(t *testing.T) *ProcessorGrok {
	processor := &ProcessorGrok{
		CustomPatternDir:    []string{},
		CustomPatterns:      map[string]string{},
		SourceKey:           "content",
		Match:               []string{"%{WORD:word1} %{NUMBER:request_time} %{WORD:word2}"},
		TimeoutMilliSeconds: 0,
		IgnoreParseFailure:  true,
		KeepSource:          true,
		NoKeyError:          true,
		NoMatchError:        true,
		TimeoutError:        true,
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))
	return processor
}

// TestProcessorGrok_ProcessV2Parse verifies the v2 Process path extracts the
// grok-matched fields into new keys and keeps the source (KeepSource=true).
func TestProcessorGrok_ProcessV2Parse(t *testing.T) {
	processor := newV2Processor(t)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "begin 123.456 end")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.Equal(t, "begin", contents.Get("word1"))
	assert.Equal(t, "123.456", contents.Get("request_time"))
	assert.Equal(t, "end", contents.Get("word2"))
	assert.Equal(t, "begin 123.456 end", contents.Get("content"), "source kept when KeepSource is true")
}

// TestProcessorGrok_ProcessV2KeepSourceFalse verifies the source field is
// dropped on a successful match when KeepSource is false.
func TestProcessorGrok_ProcessV2KeepSourceFalse(t *testing.T) {
	processor := newV2Processor(t)
	processor.KeepSource = false

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "begin 123.456 end")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.False(t, contents.Contains("content"), "source removed when KeepSource is false")
	assert.Equal(t, "begin", contents.Get("word1"))
}

// TestProcessorGrok_ProcessV2NoMatchKeepsSource verifies that on a parse
// failure the source is kept (IgnoreParseFailure) and no fields are extracted.
func TestProcessorGrok_ProcessV2NoMatchKeepsSource(t *testing.T) {
	processor := newV2Processor(t)
	processor.KeepSource = false
	processor.IgnoreParseFailure = true

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "no-grok-match-here")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.True(t, contents.Contains("content"), "source kept on parse failure (IgnoreParseFailure)")
	assert.False(t, contents.Contains("word1"), "no fields extracted on no-match")
}

// TestProcessorGrok_ProcessV2PassesThroughMetric verifies a Metric event is
// emitted unchanged (not dropped) by the log-only processor.
func TestProcessorGrok_ProcessV2PassesThroughMetric(t *testing.T) {
	processor := newV2Processor(t)

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Metric)
	assert.True(t, ok, "metric event must pass through unchanged")
}

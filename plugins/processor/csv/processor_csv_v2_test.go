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

package csv

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestProcessorCSVDecoder_ProcessV2Splits(t *testing.T) {
	processor := &ProcessorCSVDecoder{
		SourceKey: "content",
		SplitSep:  ",",
		SplitKeys: []string{"f1", "f2", "f3"},
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "12,34,56")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.Equal(t, "12", contents.Get("f1"))
	assert.Equal(t, "34", contents.Get("f2"))
	assert.Equal(t, "56", contents.Get("f3"))
	// KeepSource defaults to false, so the source key is removed on success.
	assert.False(t, contents.Contains("content"), "source key must be removed on successful decode")
}

func TestProcessorCSVDecoder_ProcessV2ExpandOthers(t *testing.T) {
	processor := &ProcessorCSVDecoder{
		SourceKey:       "content",
		SplitSep:        ",",
		SplitKeys:       []string{"f1", "f2", "f3"},
		PreserveOthers:  true,
		ExpandOthers:    true,
		ExpandKeyPrefix: "expand_",
		KeepSource:      true,
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "12,34,56,78,90")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.Equal(t, "12", contents.Get("f1"))
	assert.Equal(t, "34", contents.Get("f2"))
	assert.Equal(t, "56", contents.Get("f3"))
	assert.Equal(t, "78", contents.Get("expand_1"))
	assert.Equal(t, "90", contents.Get("expand_2"))
	assert.True(t, contents.Contains("content"), "source key must be kept when KeepSource is true")
}

func TestProcessorCSVDecoder_ProcessV2PassesThroughMetric(t *testing.T) {
	processor := &ProcessorCSVDecoder{
		SourceKey: "content",
		SplitSep:  ",",
		SplitKeys: []string{"f1"},
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Metric)
	assert.True(t, ok, "metric event must pass through unchanged")
}

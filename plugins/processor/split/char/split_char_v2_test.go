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

package char

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestProcessorSplitChar_ProcessV2SplitsIntoKeys(t *testing.T) {
	processor := &ProcessorSplitChar{
		SplitSep:  ",",
		SplitKeys: []string{"k1", "k2", "k3"},
		SourceKey: "src",
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("src", "a,b,c")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.Equal(t, "a", contents.Get("k1"))
	assert.Equal(t, "b", contents.Get("k2"))
	assert.Equal(t, "c", contents.Get("k3"))
	// KeepSource defaults to false, so the source key is removed.
	assert.False(t, contents.Contains("src"), "source key must be removed by default")
}

func TestProcessorSplitChar_ProcessV2KeepSource(t *testing.T) {
	processor := &ProcessorSplitChar{
		SplitSep:   ",",
		SplitKeys:  []string{"k1", "k2"},
		SourceKey:  "src",
		KeepSource: true,
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("src", "a,b")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.Equal(t, "a", contents.Get("k1"))
	assert.Equal(t, "b", contents.Get("k2"))
	assert.True(t, contents.Contains("src"), "source key must be kept when KeepSource is set")
}

func TestProcessorSplitChar_ProcessV2MissingSourceKey(t *testing.T) {
	processor := &ProcessorSplitChar{
		SplitSep:  ",",
		SplitKeys: []string{"k1", "k2"},
		SourceKey: "src",
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("other", "a,b")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.False(t, contents.Contains("k1"), "missing source key must not produce split keys")
	assert.Equal(t, "a,b", contents.Get("other"), "unrelated key must be untouched")
}

func TestProcessorSplitChar_ProcessV2PassesThroughMetric(t *testing.T) {
	processor := &ProcessorSplitChar{
		SplitSep:  ",",
		SplitKeys: []string{"k1"},
		SourceKey: "src",
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

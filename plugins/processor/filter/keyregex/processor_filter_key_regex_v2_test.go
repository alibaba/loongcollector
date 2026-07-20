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

package keyregex

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newV2Log(keys ...string) *models.Log {
	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	for _, key := range keys {
		log.GetIndices().Add(key, "v")
	}
	return log
}

// TestProcessorKeyFilter_ProcessV2Filter verifies that the v2 Process drops Log
// events whose keys fail the Include/Exclude rules while keeping matching ones
// and passing Metric events through unchanged.
func TestProcessorKeyFilter_ProcessV2Filter(t *testing.T) {
	processor := &ProcessorKeyFilter{
		Include: []string{"key1", "key2"},
		Exclude: []string{"secret"},
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	keep := newV2Log("key1", "key2")                  // has all required keys, no excluded key
	dropInclude := newV2Log("key1")                   // missing key2 -> drop
	dropExclude := newV2Log("key1", "key2", "secret") // excluded key present -> drop
	metric := models.NewSingleValueMetric("m", models.MetricTypeCounter, models.NewTags(), 0, 1.0)

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{
		Events: []models.PipelineEvent{keep, dropInclude, metric, dropExclude},
	}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	events := results[0].Events
	require.Len(t, events, 2, "only the matching log and the metric survive")
	assert.Equal(t, keep, events[0])
	assert.Equal(t, metric, events[1], "metric events must pass through unchanged")
}

// TestProcessorKeyFilter_ProcessV2AllDropped verifies zero surviving log events
// results in no collected group.
func TestProcessorKeyFilter_ProcessV2AllDropped(t *testing.T) {
	processor := &ProcessorKeyFilter{
		Include: []string{"required"},
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{
		Events: []models.PipelineEvent{newV2Log("other")},
	}, context)

	require.Len(t, context.Collector().ToArray(), 0)
}

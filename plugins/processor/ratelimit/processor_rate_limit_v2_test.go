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

package ratelimit

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newV2Log(kv ...string) *models.Log {
	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	for i := 0; i+1 < len(kv); i += 2 {
		log.GetIndices().Add(kv[i], kv[i+1])
	}
	return log
}

// TestProcessorRateLimit_ProcessV2Filter verifies that the v2 Process drops Log
// events once the shared rate limit is exhausted while keeping earlier ones and
// passing Metric events through unchanged. Order is preserved.
func TestProcessorRateLimit_ProcessV2Filter(t *testing.T) {
	processor := &ProcessorRateLimit{Limit: "3/s"}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log1 := newV2Log("k", "1")
	log2 := newV2Log("k", "2")
	log3 := newV2Log("k", "3")
	log4 := newV2Log("k", "4") // exceeds 3/s -> drop
	metric := models.NewSingleValueMetric("m", models.MetricTypeCounter, models.NewTags(), 0, 1.0)

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{
		Events: []models.PipelineEvent{log1, log2, metric, log3, log4},
	}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	events := results[0].Events
	require.Len(t, events, 4, "first three logs plus the metric survive")
	assert.Equal(t, log1, events[0])
	assert.Equal(t, log2, events[1])
	assert.Equal(t, metric, events[2], "metric events must pass through unchanged")
	assert.Equal(t, log3, events[3])
	assert.Equal(t, int64(1), int64(processor.limitMetric.Collect().Value))
}

// TestProcessorRateLimit_ProcessV2Fields verifies the per-key limiting when
// Fields are configured: separate limit keys have independent budgets.
func TestProcessorRateLimit_ProcessV2Fields(t *testing.T) {
	processor := &ProcessorRateLimit{Limit: "3/s", Fields: []string{"key1"}}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	events := []models.PipelineEvent{
		newV2Log("key1", "a"),
		newV2Log("key1", "a"),
		newV2Log("key1", "a"),
		newV2Log("key1", "a"), // 4th "a" -> drop
		newV2Log("key1", "b"), // different key -> allowed
	}

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: events}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 4, "three 'a' logs plus one 'b' log survive")
	assert.Equal(t, int64(1), int64(processor.limitMetric.Collect().Value))
}

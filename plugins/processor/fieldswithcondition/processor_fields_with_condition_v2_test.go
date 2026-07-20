// Copyright 2021 iLogtail Authors
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

package fieldswithcondition

import (
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
)

func TestProcessLogEventMatchV2(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("content", "time:2017.09.12 20:55:36\\tOut of memory: Kill process ")

	processor.processLogEvent(log)

	// Matching case adds eventcode/cid; test1.1/test1.2 are added then dropped.
	require.Equal(t, "c1", contents.Get("eventcode"))
	require.Equal(t, "c-1", contents.Get("cid"))
	require.False(t, contents.Contains("test1.1"))
	require.False(t, contents.Contains("test1.2"))
	require.True(t, contents.Contains("content"))
}

func TestProcessLogEventSecondCaseV2(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("content", "time:2017.09.12 20:55:36\\tBIOS-provided physical RAM map ")

	processor.processLogEvent(log)

	require.Equal(t, "c2", contents.Get("eventcode"))
	require.Equal(t, "c-2", contents.Get("cid"))
	require.False(t, contents.Contains("test2.1"))
	require.False(t, contents.Contains("test2.2"))
}

func TestProcessLogEventNoMatchDropsEventV2(t *testing.T) {
	// newProcessor sets DropIfNotMatchCondition=true: an unmatched Log event is
	// dropped (matching v1); a Metric event still passes through unchanged.
	processor, err := newProcessor()
	require.NoError(t, err)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "nothing matches here")
	metric := models.NewMetric("cpu", models.MetricTypeCounter, models.NewTags(), 0, &models.MetricSingleValue{Value: 1}, nil)

	group := &models.PipelineGroupEvents{Events: []models.PipelineEvent{log, metric}}
	context := helper.NewObservePipelineContext(10)
	processor.Process(group, context)

	results := context.Collector().ToArray()
	require.Equal(t, 1, len(results))
	// The unmatched Log is dropped; only the Metric survives.
	require.Equal(t, 1, len(results[0].Events))
	require.Equal(t, models.EventTypeMetric, results[0].Events[0].GetType())
}

func TestProcessMetricPassThroughV2(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "time:2017.09.12 20:55:36\\tOut of memory: Kill process ")
	metric := models.NewMetric("cpu", models.MetricTypeCounter, models.NewTags(), 0, &models.MetricSingleValue{Value: 1}, nil)

	group := &models.PipelineGroupEvents{Events: []models.PipelineEvent{log, metric}}
	context := helper.NewObservePipelineContext(10)
	processor.Process(group, context)

	results := context.Collector().ToArray()
	require.Equal(t, 1, len(results))
	require.Equal(t, 2, len(results[0].Events))
	require.Equal(t, models.EventTypeMetric, results[0].Events[1].GetType())
	// Log event condition action applied.
	require.Equal(t, "c1", log.GetIndices().Get("eventcode"))
}

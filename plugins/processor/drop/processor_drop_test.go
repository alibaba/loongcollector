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

package drop

import (
	"strconv"
	"testing"

	"github.com/alibaba/ilogtail/plugins/processor/internal/processortest"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newProcessor(idxs ...int) (*ProcessorDrop, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	var dropKeys []string
	for _, i := range idxs {
		k := "test_" + strconv.Itoa(i)
		dropKeys = append(dropKeys, k)
	}
	processor := &ProcessorDrop{
		DropKeys: dropKeys,
	}
	err := processor.Init(ctx)
	return processor, err
}

func getLog() *models.Log {
	log := processortest.Log()
	log.Contents.Add("test_1", "test_1")
	log.Contents.Add("test_2", "test_2")
	log.Contents.Add("test_3", "test_3")
	log.Contents.Add("test_4", "test_4")
	log.Contents.Add("test_5", "test_5")
	return log
}

// drop all
func TestSourceKey(t *testing.T) {
	processor, err := newProcessor(1, 2, 3, 4, 5)
	require.NoError(t, err)
	log := getLog()
	processortest.Apply(processor, log)
	assert.Equal(t, 0, log.Contents.Len())
}

// drop first two
func TestSourceKey1(t *testing.T) {
	processor, err := newProcessor(1, 2)
	require.NoError(t, err)
	log := getLog()
	processortest.Apply(processor, log)
	assert.Equal(t, "test_3", log.Contents.Get("test_3"))
	assert.Equal(t, "test_4", log.Contents.Get("test_4"))
	assert.Equal(t, "test_5", log.Contents.Get("test_5"))
	assert.Equal(t, 3, log.Contents.Len())
}

// drop middle two
func TestSourceKey2(t *testing.T) {
	processor, err := newProcessor(2, 3)
	require.NoError(t, err)
	log := getLog()
	processortest.Apply(processor, log)
	assert.Equal(t, "test_1", log.Contents.Get("test_1"))
	assert.Equal(t, "test_4", log.Contents.Get("test_4"))
	assert.Equal(t, "test_5", log.Contents.Get("test_5"))
	assert.Equal(t, 3, log.Contents.Len())
}

func TestSourceKey3(t *testing.T) {
	processor, err := newProcessor(1, 5)
	require.NoError(t, err)
	log := getLog()
	processortest.Apply(processor, log)
	assert.Equal(t, "test_2", log.Contents.Get("test_2"))
	assert.Equal(t, "test_3", log.Contents.Get("test_3"))
	assert.Equal(t, "test_4", log.Contents.Get("test_4"))
	assert.Equal(t, 3, log.Contents.Len())
}

func TestSourceKey4(t *testing.T) {
	processor, err := newProcessor(1, 3)
	require.NoError(t, err)
	log := getLog()
	processortest.Apply(processor, log)
	assert.Equal(t, "test_2", log.Contents.Get("test_2"))
	assert.Equal(t, "test_4", log.Contents.Get("test_4"))
	assert.Equal(t, "test_5", log.Contents.Get("test_5"))
	assert.Equal(t, 3, log.Contents.Len())
}

func TestSourceKey5(t *testing.T) {
	processor, err := newProcessor(3, 5)
	require.NoError(t, err)
	log := getLog()
	processortest.Apply(processor, log)
	assert.Equal(t, "test_1", log.Contents.Get("test_1"))
	assert.Equal(t, "test_2", log.Contents.Get("test_2"))
	assert.Equal(t, "test_4", log.Contents.Get("test_4"))
	assert.Equal(t, 3, log.Contents.Len())
}

func TestProcessorDrop_ProcessDropsKeys(t *testing.T) {
	processor := &ProcessorDrop{DropKeys: []string{"drop_me"}}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("drop_me", "x")
	log.GetIndices().Add("keep_me", "y")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.False(t, log.GetIndices().Contains("drop_me"), "configured key must be dropped")
	assert.True(t, log.GetIndices().Contains("keep_me"), "other keys must be kept")
}

// TestProcessorDrop_ProcessPassesThroughMetric verifies Metric events are not
// dropped by the log-only processor.
func TestProcessorDrop_ProcessPassesThroughMetric(t *testing.T) {
	processor := &ProcessorDrop{DropKeys: []string{"drop_me"}}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	metric.GetTags().Add("drop_me", "still_here")
	group := &models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}

	context := helper.NewObservePipelineContext(10)
	processor.Process(group, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	m, ok := results[0].Events[0].(*models.Metric)
	require.True(t, ok, "metric event must pass through unchanged")
	assert.Equal(t, "still_here", m.GetTags().Get("drop_me"), "metric tags must not be dropped")
}

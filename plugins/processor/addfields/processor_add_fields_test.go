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

package addfields

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newProcessor() (*ProcessorAddFields, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorAddFields{
		Fields: map[string]string{
			"a": "1",
		},
		IgnoreIfExist: true,
	}
	err := processor.Init(ctx)
	return processor, err
}

func TestSourceKey(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "test_key", Value: "test_value"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "a", Value: "6"})
	processor.processLog(log)
	assert.Equal(t, "test_value", log.Contents[0].Value)
	assert.Equal(t, "6", log.Contents[1].Value)
}

func TestIgnoreIfExistFalse(t *testing.T) {
	processor, err := newProcessor()
	processor.IgnoreIfExist = false
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "test_key", Value: "test_value"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "a", Value: "6"})
	processor.processLog(log)
	assert.Equal(t, "test_value", log.Contents[0].Value)
	assert.Equal(t, "6", log.Contents[1].Value)
	assert.Equal(t, "1", log.Contents[2].Value)
}

func TestIgnoreIfExistTrue(t *testing.T) {
	processor, err := newProcessor()
	processor.IgnoreIfExist = true
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "test_key", Value: "test_value"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "a", Value: "6"})
	processor.processLog(log)
	assert.Equal(t, "test_value", log.Contents[0].Value)
	assert.Equal(t, "6", log.Contents[1].Value)
}

func TestParameterCheck(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorAddFields{}
	err := processor.Init(ctx)
	assert.Error(t, err)
}

// ---- v2 (PipelineEvent / SendPb) Process path tests ----

func TestProcessorAddFields_ProcessV2AddsFields(t *testing.T) {
	processor := &ProcessorAddFields{Fields: map[string]string{"new_key": "new_value"}}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("existing", "v")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.True(t, log.GetIndices().Contains("new_key"))
	assert.Equal(t, "new_value", log.GetIndices().Get("new_key"))
	assert.Equal(t, "v", log.GetIndices().Get("existing"))
}

func TestProcessorAddFields_ProcessV2IgnoreIfExist(t *testing.T) {
	processor := &ProcessorAddFields{Fields: map[string]string{"a": "override"}, IgnoreIfExist: true}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("a", "original")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.Equal(t, "original", log.GetIndices().Get("a"), "IgnoreIfExist must keep the original value")
}

// TestProcessorAddFields_ProcessV2PassesThroughMetric verifies Metric events are
// not modified and not dropped by the log-only processor.
func TestProcessorAddFields_ProcessV2PassesThroughMetric(t *testing.T) {
	processor := &ProcessorAddFields{Fields: map[string]string{"new_key": "new_value"}}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	group := &models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}

	context := helper.NewObservePipelineContext(10)
	processor.Process(group, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	m, ok := results[0].Events[0].(*models.Metric)
	require.True(t, ok, "metric event must pass through unchanged")
	assert.Equal(t, "m", m.GetName())
	assert.False(t, m.GetTags().Contains("new_key"), "log-only processor must not touch metric tags")
}

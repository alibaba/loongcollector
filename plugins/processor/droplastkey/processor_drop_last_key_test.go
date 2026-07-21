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

package droplastkey

import (
	"reflect"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func init() {
	logger.InitTestLogger(logger.OptionOpenMemoryReceiver)
}

func newProcessor() (*ProcessorDropLastKey, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorDropLastKey{
		Include: []string{"src", "test"},
		DropKey: "src",
	}
	err := processor.Init(ctx)
	return processor, err
}

func TestSourceKey(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "src", Value: "123"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "xxx", Value: "234"})
	processor.ProcessLogs([]*protocol.Log{log})
	assert.Equal(t, "234", log.Contents[0].Value)
}

func TestDropKeyError(t *testing.T) {
	logger.ClearMemoryLog()
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorDropLastKey{
		Include: []string{"src", "test"},
		DropKey: "",
	}
	err := processor.Init(ctx)
	assert.Equal(t, err.Error(), "Invalid config, DropKey is empty")
}

func TestIncludeError(t *testing.T) {
	logger.ClearMemoryLog()
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorDropLastKey{
		Include: []string{},
		DropKey: "src",
	}
	err := processor.Init(ctx)
	assert.Equal(t, err.Error(), "Invalid config, Include is empty")
}

func TestDropFlag(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "xxx", Value: "234"})
	processor.ProcessLogs([]*protocol.Log{log})
	assert.Equal(t, "234", log.Contents[0].Value)
}

func TestDescription(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	assert.Equal(t, processor.Description(), "processor_drop_last_key is used to drop log content when process done")
}

func TestInit(t *testing.T) {
	p := pipeline.Processors["processor_drop_last_key"]()
	assert.Equal(t, reflect.TypeOf(p).String(), "*droplastkey.ProcessorDropLastKey")
}

// ---- v2 (PipelineEvent / SendPb) Process path tests ----

func newV2Processor(t *testing.T) *ProcessorDropLastKey {
	processor := &ProcessorDropLastKey{Include: []string{"trigger"}, DropKey: "drop_me"}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))
	return processor
}

func TestProcessorDropLastKey_ProcessV2DropsWhenIncludePresent(t *testing.T) {
	processor := newV2Processor(t)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("trigger", "1")
	log.GetIndices().Add("drop_me", "x")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.False(t, log.GetIndices().Contains("drop_me"), "DropKey must be removed when an Include key exists")
	assert.True(t, log.GetIndices().Contains("trigger"))
}

func TestProcessorDropLastKey_ProcessV2KeepsWhenIncludeAbsent(t *testing.T) {
	processor := newV2Processor(t)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("drop_me", "x")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.True(t, log.GetIndices().Contains("drop_me"), "DropKey must be kept when no Include key exists")
}

// TestProcessorDropLastKey_ProcessV2PassesThroughMetric verifies Metric events
// are passed through by the log-only processor.
func TestProcessorDropLastKey_ProcessV2PassesThroughMetric(t *testing.T) {
	processor := newV2Processor(t)

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	group := &models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}

	context := helper.NewObservePipelineContext(10)
	processor.Process(group, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Metric)
	assert.True(t, ok, "metric event must pass through unchanged")
}

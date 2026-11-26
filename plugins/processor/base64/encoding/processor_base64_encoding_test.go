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

package encoding

import (
	"reflect"
	"strings"
	"testing"

	"github.com/alibaba/ilogtail/plugins/processor/internal/processortest"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func init() {
	logger.InitTestLogger(logger.OptionOpenMemoryReceiver)
}

func newProcessor() (*ProcessorBase64Encoding, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorBase64Encoding{
		SourceKey: "src",
		NewKey:    "dest",
	}
	err := processor.Init(ctx)
	return processor, err
}

func TestNoKeyError(t *testing.T) {
	logger.ClearMemoryLog()
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorBase64Encoding{
		SourceKey:  "src",
		NewKey:     "dest",
		NoKeyError: true,
	}
	err := processor.Init(ctx)
	require.NoError(t, err)
	log := processortest.Log()
	log.Contents.Add("test_key", "test_value")
	processortest.Apply(processor, log)
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.Equal(t, 1, logger.GetMemoryLogCount())
	assert.True(t, strings.Contains(memoryLog, "AlarmType:BASE64_E_FIND_ALARM\tcannot find key:src"))
}

func TestDescription(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	assert.Equal(t, processor.Description(), "base64 encoding processor for logtail")
}

func TestInit(t *testing.T) {
	p := pipeline.Processors["processor_base64_encoding"]()
	assert.Equal(t, reflect.TypeOf(p).String(), "*encoding.ProcessorBase64Encoding")
}

func TestProcessorBase64Encoding_ProcessToNewKey(t *testing.T) {
	processor := &ProcessorBase64Encoding{SourceKey: "src", NewKey: "dst"}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("src", "123")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.Equal(t, "123", log.GetIndices().Get("src"), "source is preserved when NewKey is set")
	assert.Equal(t, "MTIz", log.GetIndices().Get("dst"))
}

func TestProcessorBase64Encoding_ProcessInPlace(t *testing.T) {
	processor := &ProcessorBase64Encoding{SourceKey: "src"}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("src", "123")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.Equal(t, "MTIz", log.GetIndices().Get("src"))
}

func TestProcessorBase64Encoding_ProcessPassesThroughMetric(t *testing.T) {
	processor := &ProcessorBase64Encoding{SourceKey: "src", NewKey: "dst"}
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

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

package packjson

import (
	"encoding/json"
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

func newProcessor() (*ProcessorPackjson, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorPackjson{
		SourceKeys:        []string{"a", "b"},
		DestKey:           "d_key", // 目标Key，为空不生效
		KeepSource:        true,    // 是否保留源字段
		AlarmIfIncomplete: true,
	}
	err := processor.Init(ctx)
	return processor, err
}

func TestSourceKey(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	log := processortest.Log()
	log.Contents.Add("a", "1")
	log.Contents.Add("b", "2")
	processortest.Apply(processor, log)
	assert.Equal(t, "{\"a\":\"1\",\"b\":\"2\"}", pipeline.GetStringValue(log.Contents.Get("d_key")))
}

func TestKeepSource(t *testing.T) {
	processor, err := newProcessor()
	if processor == nil {
		return
	}
	processor.KeepSource = false
	require.NoError(t, err)
	log := processortest.Log()
	log.Contents.Add("a", "1")
	log.Contents.Add("b", "2")
	processortest.Apply(processor, log)
	assert.Equal(t, "{\"a\":\"1\",\"b\":\"2\"}", pipeline.GetStringValue(log.Contents.Get("d_key")))
}

func TestAlarmIfEmpty(t *testing.T) {
	logger.ClearMemoryLog()
	processor, err := newProcessor()
	require.NoError(t, err)
	log := processortest.Log()
	log.Contents.Add("a", "1")
	processortest.Apply(processor, log)
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.Truef(t, strings.Contains(memoryLog, "PACK_JSON_ALARM\tSourceKeys not found [b]"), "got %s", memoryLog)
}

func TestProcessorPackjson_ProcessPacksFields(t *testing.T) {
	processor := &ProcessorPackjson{
		SourceKeys: []string{"a", "b"},
		DestKey:    "packed",
		KeepSource: false,
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("a", "1")
	log.GetIndices().Add("b", "2")
	log.GetIndices().Add("other", "keep")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	require.True(t, log.GetIndices().Contains("packed"))
	var packed map[string]string
	require.NoError(t, json.Unmarshal([]byte(log.GetIndices().Get("packed").(string)), &packed))
	assert.Equal(t, map[string]string{"a": "1", "b": "2"}, packed)

	// KeepSource=false removes the packed source keys but leaves others.
	assert.False(t, log.GetIndices().Contains("a"))
	assert.False(t, log.GetIndices().Contains("b"))
	assert.True(t, log.GetIndices().Contains("other"))
}

func TestProcessorPackjson_ProcessKeepSource(t *testing.T) {
	processor := &ProcessorPackjson{
		SourceKeys: []string{"a"},
		DestKey:    "packed",
		KeepSource: true,
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("a", "1")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.True(t, log.GetIndices().Contains("a"), "KeepSource=true must keep source keys")
	assert.True(t, log.GetIndices().Contains("packed"))
}

// TestProcessorPackjson_ProcessPassesThroughMetric verifies Metric events pass
// through the log-only processor unchanged.
func TestProcessorPackjson_ProcessPassesThroughMetric(t *testing.T) {
	processor := &ProcessorPackjson{
		SourceKeys: []string{"a"},
		DestKey:    "packed",
		KeepSource: true,
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

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

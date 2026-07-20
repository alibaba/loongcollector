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

package anchor

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestProcessorAnchor_ProcessV2Extracts(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	value := "time:2017.09.12 20:55:36" +
		"\\tlevel:info" +
		"\\tjson:{\"key1\" :\"xx\", \"key2\": false, \"key3\":123.456, \"key4\" : { \"inner1\" : 1, \"inner2\" : false}}" +
		"\\tjson2:{\"key\" : { \"inner1\" : 1, \"inner2\" : false}}"

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", value)

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	// String anchors.
	assert.Equal(t, "2017.09.12 20:55:36", contents.Get("time"))
	assert.Equal(t, "info", contents.Get("level"))
	// Expanded JSON anchor "json:" -> FieldName "val".
	assert.Equal(t, "xx", contents.Get("val_key1"))
	assert.Equal(t, "false", contents.Get("val_key2"))
	assert.Equal(t, "123.456", contents.Get("val_key3"))
	assert.Equal(t, "1", contents.Get("val_key4_inner1"))
	assert.Equal(t, "false", contents.Get("val_key4_inner2"))
	// json2 anchor uses connector "-" and MaxExpondDepth 1 (no expand).
	assert.Equal(t, "{ \"inner1\" : 1, \"inner2\" : false}", contents.Get("val2-key"))
	// SourceKey is not kept by default.
	assert.False(t, contents.Contains("content"), "source key must be removed when KeepSource is false")
}

func TestProcessorAnchor_ProcessV2KeepSource(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorAnchor{
		Anchors: []Anchor{
			{Start: "k:", Stop: "", FieldName: "k", FieldType: "string"},
		},
		SourceKey:  "content",
		KeepSource: true,
	}
	require.NoError(t, processor.Init(ctx))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "k:v")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.Equal(t, "v", contents.Get("k"))
	assert.True(t, contents.Contains("content"), "source key must be kept when KeepSource is true")
}

func TestProcessorAnchor_ProcessV2PassesThroughMetric(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Metric)
	assert.True(t, ok, "metric event must pass through unchanged")
}

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

package otel

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

// A Log carrying an OTLP trace payload is converted to native models.Span
// events ("v2场景下输入Log输出Span"); the source Log is replaced (not preserved).
func TestProcessorOtelTraceParser_ProcessV2LogToSpan(t *testing.T) {
	parser := &ProcessorOtelTraceParser{SourceKey: "otel", Format: "protojson"}
	require.NoError(t, parser.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("otel", protoJSONData)

	context := helper.NewObservePipelineContext(10)
	parser.Process(&models.PipelineGroupEvents{Group: models.NewGroup(models.NewMetadata(), models.NewTags()), Events: []models.PipelineEvent{log}}, context)

	results := context.Collector().ToArray()
	require.NotEmpty(t, results)

	spanCount := 0
	for _, group := range results {
		for _, event := range group.Events {
			assert.Equal(t, models.EventTypeSpan, event.GetType(), "source log must be replaced by span events")
			_, ok := event.(*models.Span)
			assert.True(t, ok)
			spanCount++
		}
	}
	// protoJSONData carries ten spans.
	assert.Equal(t, 10, spanCount)
}

// A pre-existing Span event passes through unchanged.
func TestProcessorOtelTraceParser_ProcessV2PassesThroughSpan(t *testing.T) {
	parser := &ProcessorOtelTraceParser{SourceKey: "otel", Format: "protojson"}
	require.NoError(t, parser.Init(mock.NewEmptyContext("p", "l", "c")))

	span := models.NewSpan("preexist", "trace", "span", models.SpanKindClient, 0, 0, models.NewTags(), nil, nil)
	context := helper.NewObservePipelineContext(10)
	parser.Process(&models.PipelineGroupEvents{Group: models.NewGroup(models.NewMetadata(), models.NewTags()), Events: []models.PipelineEvent{span}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	passed, ok := results[0].Events[0].(*models.Span)
	require.True(t, ok, "span event must pass through unchanged")
	assert.Equal(t, "preexist", passed.GetName())
}

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

package pipeline

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
)

func TestEventKindSet_Supports(t *testing.T) {
	assert.True(t, LogOnlyEventKinds.Supports(models.EventTypeLogging))
	assert.False(t, LogOnlyEventKinds.Supports(models.EventTypeMetric))
	assert.False(t, LogOnlyEventKinds.Supports(models.EventTypeSpan))
	assert.False(t, LogOnlyEventKinds.Supports(models.EventTypeByteArray))

	assert.True(t, AllPipelineEventKinds.Supports(models.EventTypeLogging))
	assert.True(t, AllPipelineEventKinds.Supports(models.EventTypeMetric))
	assert.True(t, AllPipelineEventKinds.Supports(models.EventTypeSpan))
}

func TestPartitionEvents_LogMetricSpan(t *testing.T) {
	log := models.NewLog("", []byte("log"), "info", "", "", models.NewTags(), 1)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 2, 1.0)
	span := models.NewSpan("op", "trace", "span", models.SpanKindServer, 3, 4, models.NewTags(), nil, nil)
	events := []models.PipelineEvent{metric, log, span}

	matched, passThrough := PartitionEvents(events, LogOnlyEventKinds)
	require.Len(t, matched, 1)
	require.Len(t, passThrough, 2)
	assert.Equal(t, models.EventTypeLogging, matched[0].GetType())
	assert.Equal(t, models.EventTypeMetric, passThrough[0].GetType())
	assert.Equal(t, models.EventTypeSpan, passThrough[1].GetType())
}

func TestPassThroughEvents_PreservesMetricAndSpan(t *testing.T) {
	log := models.NewLog("", []byte("log"), "info", "", "", models.NewTags(), 1)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 2, 1.0)
	span := models.NewSpan("op", "trace", "span", models.SpanKindServer, 3, 4, models.NewTags(), nil, nil)
	events := []models.PipelineEvent{log, metric, span}

	passThrough := PassThroughEvents(events, LogOnlyEventKinds)
	require.Len(t, passThrough, 2)
	assert.Equal(t, models.EventTypeMetric, passThrough[0].GetType())
	assert.Equal(t, models.EventTypeSpan, passThrough[1].GetType())
}

func TestRecombineEvents_PreservesOrder(t *testing.T) {
	log1 := models.NewLog("", []byte("a"), "info", "", "", models.NewTags(), 1)
	log2 := models.NewLog("", []byte("b"), "info", "", "", models.NewTags(), 2)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 3, 1.0)
	span := models.NewSpan("op", "trace", "span", models.SpanKindServer, 4, 5, models.NewTags(), nil, nil)
	original := []models.PipelineEvent{metric, log1, span, log2}

	processedLog1 := models.NewLog("", []byte("A"), "info", "", "", models.NewTags(), 1)
	processedLog2 := models.NewLog("", []byte("B"), "info", "", "", models.NewTags(), 2)
	recombined := RecombineEvents(original, LogOnlyEventKinds, []models.PipelineEvent{processedLog1, processedLog2})

	require.Len(t, recombined, 4)
	assert.Equal(t, models.EventTypeMetric, recombined[0].GetType())
	assert.Equal(t, "A", string(recombined[1].(*models.Log).GetBody()))
	assert.Equal(t, models.EventTypeSpan, recombined[2].GetType())
	assert.Equal(t, "B", string(recombined[3].(*models.Log).GetBody()))
}

func TestApplyToSupportedEvents_OnlyTouchesLogs(t *testing.T) {
	log := models.NewLog("", []byte("before"), "info", "", "", models.NewTags(), 1)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 2, 1.0)
	events := []models.PipelineEvent{metric, log}

	ApplyToSupportedEvents(events, LogOnlyEventKinds, func(event models.PipelineEvent) {
		event.(*models.Log).SetBody([]byte("after"))
	})

	assert.Equal(t, "after", string(log.GetBody()))
	assert.Equal(t, 1.0, metric.GetValue().GetSingleValue())
}

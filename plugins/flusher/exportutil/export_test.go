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

package exportutil

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

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

func TestSerializePassthroughEvent_MetricAndSpan(t *testing.T) {
	metric := models.NewSingleValueMetric("cpu", models.MetricTypeGauge, models.NewTagsWithKeyValues("host", "h1"), 100, 0.5)
	metricPayload, err := SerializePassthroughEvent(metric)
	require.NoError(t, err)
	assert.Contains(t, string(metricPayload), `"eventType":"metric"`)
	assert.Contains(t, string(metricPayload), `"name":"cpu"`)

	span := models.NewSpan("op", "trace-id", "span-id", models.SpanKindServer, 200, 201, models.NewTags(), nil, nil)
	spanPayload, err := SerializePassthroughEvent(span)
	require.NoError(t, err)
	assert.Contains(t, string(spanPayload), `"eventType":"span"`)
	assert.Contains(t, string(spanPayload), `"traceID":"trace-id"`)
}

func TestExportLogOnly_PreservesMetricAndSpan(t *testing.T) {
	var flushedLogs int

	log := models.NewLog("", []byte("hello"), "info", "", "", models.NewTags(), 1)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 2, 1.0)
	span := models.NewSpan("op", "trace", "span", models.SpanKindServer, 3, 4, models.NewTags(), nil, nil)
	groups := []*models.PipelineGroupEvents{{
		Group:  models.NewGroup(models.NewMetadata(), models.NewTags()),
		Events: []models.PipelineEvent{log, metric, span},
	}}

	err := ExportLogOnly(groups, "p", "l", "c",
		func(_, _, _ string, logGroups []*protocol.LogGroup) error {
			for _, lg := range logGroups {
				flushedLogs += len(lg.Logs)
			}
			return nil
		},
	)
	require.NoError(t, err)
	assert.Equal(t, 3, flushedLogs)
}

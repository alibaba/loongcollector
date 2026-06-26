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

package kafka

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
)

func TestExportLogOnlyPreservesMetricAndSpan(t *testing.T) {
	groups := mixedLogMetricSpanGroups()
	var flushed int
	for _, groupEvents := range groups {
		logEvents, passThrough := partitionLogOnlyEvents(groupEvents.Events)
		logGroup, err := eventsToLogGroup(groupEvents.Group, logEvents)
		require.NoError(t, err)
		require.NoError(t, appendPassthroughLogs(logGroup, passThrough))
		flushed += len(logGroup.Logs)
	}
	assert.Equal(t, 3, flushed)
}

func TestSerializePassthroughEventMetricAndSpan(t *testing.T) {
	metric := models.NewSingleValueMetric("cpu", models.MetricTypeGauge, models.NewTagsWithKeyValues("host", "h1"), 100, 0.5)
	metricPayload, err := serializePassthroughEvent(metric)
	require.NoError(t, err)
	assert.Contains(t, string(metricPayload), `"eventType":"metric"`)

	span := models.NewSpan("op", "trace-id", "span-id", models.SpanKindServer, 200, 201, models.NewTags(), nil, nil)
	spanPayload, err := serializePassthroughEvent(span)
	require.NoError(t, err)
	assert.Contains(t, string(spanPayload), `"eventType":"span"`)
}

func mixedLogMetricSpanGroups() []*models.PipelineGroupEvents {
	log := models.NewLog("", []byte("hello"), "info", "", "", models.NewTags(), 1)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 2, 1.0)
	span := models.NewSpan("op", "trace", "span", models.SpanKindServer, 3, 4, models.NewTags(), nil, nil)
	return []*models.PipelineGroupEvents{{
		Group:  models.NewGroup(models.NewMetadata(), models.NewTags()),
		Events: []models.PipelineEvent{log, metric, span},
	}}
}

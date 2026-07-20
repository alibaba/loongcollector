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

package logtoslsmetric

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newV2Processor(t *testing.T) *ProcessorLogToSlsMetric {
	processor := &ProcessorLogToSlsMetric{
		MetricTimeKey:   "timeKey",
		MetricLabelKeys: []string{"labelA", "labelB", "labelC"},
		MetricValues: map[string]string{
			"nameA": "valueA",
			"nameB": "valueB",
		},
		CustomMetricLabels: map[string]string{
			"labelD": "CustomD",
		},
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))
	return processor
}

func collectMetrics(t *testing.T, results []*models.PipelineGroupEvents) []*models.Metric {
	var metrics []*models.Metric
	for _, group := range results {
		for _, event := range group.Events {
			if event.GetType() == models.EventTypeMetric {
				metric, ok := event.(*models.Metric)
				require.True(t, ok, "EventTypeMetric must cast to *models.Metric")
				metrics = append(metrics, metric)
			}
		}
	}
	return metrics
}

// A Log carrying the configured name/value/label fields is converted into
// models.Metric events (one per configured (name, value) pair) with the correct
// name, tags, value, and timestamp. The source Log is not preserved.
func TestProcessorLogToSlsMetric_ProcessV2EmitsMetrics(t *testing.T) {
	processor := newV2Processor(t)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("labelA", "1")
	contents.Add("labelB", "2")
	contents.Add("labelC", "3")
	contents.Add("nameA", "myname_a")
	contents.Add("valueA", "1.5")
	contents.Add("nameB", "myname_b")
	contents.Add("valueB", "2.5")
	contents.Add("timeKey", "1658806869597190887")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	metrics := collectMetrics(t, context.Collector().ToArray())
	require.Len(t, metrics, 2, "one metric per configured (name, value) pair")

	byName := map[string]*models.Metric{}
	for _, metric := range metrics {
		// No source Log should be present in the output.
		assert.Equal(t, models.EventTypeMetric, metric.GetType())
		byName[metric.GetName()] = metric
	}

	metricA := byName["myname_a"]
	require.NotNil(t, metricA)
	assert.Equal(t, models.MetricTypeGauge, metricA.GetMetricType())
	assert.Equal(t, 1.5, metricA.GetValue().GetSingleValue())
	assert.Equal(t, uint64(1658806869597190887), metricA.GetTimestamp())
	assert.Equal(t, "1", metricA.GetTags().Get("labelA"))
	assert.Equal(t, "2", metricA.GetTags().Get("labelB"))
	assert.Equal(t, "3", metricA.GetTags().Get("labelC"))
	assert.Equal(t, "CustomD", metricA.GetTags().Get("labelD"))

	metricB := byName["myname_b"]
	require.NotNil(t, metricB)
	assert.Equal(t, 2.5, metricB.GetValue().GetSingleValue())

	// Independent tag copies per metric.
	assert.NotSame(t, metricA.GetTags(), metricB.GetTags())
}

// A Log failing validation (here: invalid metric value) is skipped and produces
// no output events, matching v1 skip semantics.
func TestProcessorLogToSlsMetric_ProcessV2SkipsInvalidLog(t *testing.T) {
	processor := newV2Processor(t)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("labelA", "1")
	contents.Add("labelB", "2")
	contents.Add("labelC", "3")
	contents.Add("nameA", "myname_a")
	contents.Add("valueA", "not_a_number") // invalid float -> whole log dropped
	contents.Add("nameB", "myname_b")
	contents.Add("valueB", "2.5")
	contents.Add("timeKey", "1658806869597190887")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.Empty(t, context.Collector().ToArray(), "an invalid log must be dropped, emitting nothing")
}

// A Log that does not carry the configured metric fields fails the count checks
// and is dropped (v1 does not preserve non-metric logs).
func TestProcessorLogToSlsMetric_ProcessV2DropsNonMetricLog(t *testing.T) {
	processor := newV2Processor(t)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("unrelated", "value")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.Empty(t, context.Collector().ToArray(), "a log without metric fields is dropped, not preserved")
}

// Pre-existing Metric and Span events pass through unchanged.
func TestProcessorLogToSlsMetric_ProcessV2PassesThroughNonLog(t *testing.T) {
	processor := newV2Processor(t)

	metric := models.NewSingleValueMetric("existing", models.MetricTypeGauge, models.NewTags(), 42, 3.0)
	span := models.NewSpan("s", "t", "sp", models.SpanKindClient, 0, 1, models.NewTags(), nil, nil)

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{metric, span}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 2)

	gotMetric, ok := results[0].Events[0].(*models.Metric)
	require.True(t, ok, "metric event must pass through unchanged")
	assert.Equal(t, "existing", gotMetric.GetName())
	assert.Equal(t, uint64(42), gotMetric.GetTimestamp())

	_, ok = results[0].Events[1].(*models.Span)
	assert.True(t, ok, "span event must pass through unchanged")
}

// Copyright 2023 iLogtail Authors
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

	"github.com/alibaba/ilogtail/plugins/processor/internal/processortest"

	. "github.com/smartystreets/goconvey/convey"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestInitError(t *testing.T) {
	Convey("Init error.", t, func() {
		Convey("Regex error", func() {
			processor := &ProcessorLogToSlsMetric{}
			err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldNotBeNil)
		})
	})
}

func TestProcessorLogToSlsMetricTimestamps(t *testing.T) {
	for _, input := range []string{"1234567890", "1234567890000", "1234567890000000", "1234567890000000000", ""} {
		t.Run(input, func(t *testing.T) {
			processor := &ProcessorLogToSlsMetric{MetricValues: map[string]string{"name": "value"}, CustomMetricLabels: map[string]string{"service": "test"}}
			if input != "" {
				processor.MetricTimeKey = "time"
			}
			require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))
			log := processortest.Log("name", "requests", "value", "20", "time", input)
			log.Timestamp = 1234567890000000042

			context := helper.NewGroupedPipelineContext()
			processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)
			metrics := collectMetrics(t, context.Collector().ToArray())
			require.Len(t, metrics, 1)
			expected := uint64(1234567890000000000)
			if input == "" {
				expected = log.GetTimestamp()
			}
			assert.Equal(t, expected, metrics[0].GetTimestamp())
			assert.Equal(t, "requests", metrics[0].GetName())
			assert.Equal(t, 20.0, metrics[0].GetValue().GetSingleValue())
		})
	}
}

func TestProcessorLogToSlsMetricValidatesFields(t *testing.T) {
	for _, test := range []struct {
		name  string
		key   string
		value string
	}{
		{"malformed timestamp", "time", "12345678900000000.0"},
		{"invalid timestamp length", "time", "123456789000000000"},
		{"missing timestamp", "time", ""},
		{"invalid metric name", "name", "2a0"},
		{"invalid metric value", "value", "2a0"},
		{"invalid label separators", metricLabelsKey, "a#$#1#$#|b#$#2"},
		{"duplicate configured label", metricLabelsKey, "label1#$#1|b#$#2"},
		{"invalid label name", metricLabelsKey, "-a#$#1|b#$#2"},
	} {
		t.Run(test.name, func(t *testing.T) {
			processor := &ProcessorLogToSlsMetric{
				MetricTimeKey:   "time",
				MetricValues:    map[string]string{"name": "value"},
				MetricLabelKeys: []string{"label1"},
			}
			require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))
			log := processortest.Log("name", "requests", "value", "20", "time", "1234567890", "label1", "region")
			log.Contents.Add(test.key, test.value)
			context := helper.NewGroupedPipelineContext()
			processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)
			assert.Empty(t, context.Collector().ToArray())
		})
	}
}

func TestIsTimeNano(t *testing.T) {
	Convey("Given a time string", t, func() {
		Convey("When the length is not valid", func() {
			So(isTimeNano("123"), ShouldBeFalse)
			So(isTimeNano("12345678901234567"), ShouldBeFalse)
		})

		Convey("When the length is valid", func() {
			Convey("And the string contains only digits", func() {
				So(isTimeNano("1234567890123456789"), ShouldBeTrue)
				So(isTimeNano("1234567890123456"), ShouldBeTrue)
				So(isTimeNano("1234567890"), ShouldBeTrue)
			})

			Convey("And the string contains non-digit characters", func() {
				So(isTimeNano("12345678901234567.9"), ShouldBeFalse)
				So(isTimeNano("12345678901234567a"), ShouldBeFalse)
				So(isTimeNano("1234567890123456@"), ShouldBeFalse)
				So(isTimeNano("123456789"), ShouldBeFalse)
			})
		})
	})
}

func TestCanParseToFloat64(t *testing.T) {
	Convey("Given a string", t, func() {
		Convey("When the string can be parsed to float64", func() {
			So(canParseToFloat64("3.14123123124123123"), ShouldBeTrue)
			So(canParseToFloat64("0.123"), ShouldBeTrue)
			So(canParseToFloat64("-2.5"), ShouldBeTrue)
		})

		Convey("When the string cannot be parsed to float64", func() {
			So(canParseToFloat64("abc"), ShouldBeFalse)
			So(canParseToFloat64("1.23.45"), ShouldBeFalse)
			So(canParseToFloat64("12a"), ShouldBeFalse)
			So(canParseToFloat64("3.1412312312.4123123"), ShouldBeFalse)
		})
	})
}

func newEventProcessor(t *testing.T) *ProcessorLogToSlsMetric {
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
func TestProcessorLogToSlsMetric_ProcessEmitsMetrics(t *testing.T) {
	processor := newEventProcessor(t)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add(metricLabelsKey, "region#$#west|cluster#$#development")
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
	assert.Equal(t, "west", metricA.GetTags().Get("region"))
	assert.Equal(t, "development", metricA.GetTags().Get("cluster"))

	metricB := byName["myname_b"]
	require.NotNil(t, metricB)
	assert.Equal(t, 2.5, metricB.GetValue().GetSingleValue())

	// Independent tag copies per metric.
	assert.NotSame(t, metricA.GetTags(), metricB.GetTags())
}

// A Log failing validation (here: invalid metric value) is skipped and produces
// no output events.
func TestProcessorLogToSlsMetric_ProcessSkipsInvalidLog(t *testing.T) {
	processor := newEventProcessor(t)

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
// and is dropped.
func TestProcessorLogToSlsMetric_ProcessDropsNonMetricLog(t *testing.T) {
	processor := newEventProcessor(t)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("unrelated", "value")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.Empty(t, context.Collector().ToArray(), "a log without metric fields is dropped, not preserved")
}

// Pre-existing Metric and Span events pass through unchanged.
func TestProcessorLogToSlsMetric_ProcessPassesThroughNonLog(t *testing.T) {
	processor := newEventProcessor(t)

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

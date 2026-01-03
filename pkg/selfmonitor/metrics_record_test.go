// Copyright 2025 iLogtail Authors
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

package selfmonitor

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestExportMetricRecords(t *testing.T) {
	metricRecord := MetricsRecord{}
	addEventCount := NewCounterMetricAndRegister(&metricRecord, MetricRunnerK8sMetaAddEventTotal)
	updateEventCount := NewCounterMetricAndRegister(&metricRecord, MetricRunnerK8sMetaUpdateEventTotal)
	deleteEventCount := NewCounterMetricAndRegister(&metricRecord, MetricRunnerK8sMetaDeleteEventTotal)
	cacheResourceGauge := NewGaugeMetricAndRegister(&metricRecord, MetricRunnerK8sMetaCacheSize)
	queueSizeGauge := NewGaugeMetricAndRegister(&metricRecord, MetricRunnerK8sMetaQueueSize)
	httpRequestCount := NewCounterMetricAndRegister(&metricRecord, MetricRunnerK8sMetaHTTPRequestTotal)
	httpAvgDelayMs := NewAverageMetricAndRegister(&metricRecord, MetricRunnerK8sMetaHTTPAvgDelayMs)
	httpMaxDelayMs := NewMaxMetricAndRegister(&metricRecord, MetricRunnerK8sMetaHTTPMaxDelayMs)

	metricRecord.Labels = []LabelPair{
		{
			Key:   MetricLabelKeyMetricCategory,
			Value: MetricLabelValueMetricCategoryRunner,
		},
		{
			Key:   MetricLabelKeyClusterID,
			Value: "test-cluster-id",
		},
		{
			Key:   MetricLabelKeyRunnerName,
			Value: MetricLabelValueRunnerNameK8sMeta,
		},
		{
			Key:   MetricLabelKeyProject,
			Value: "test-project",
		},
	}

	addEventCount.Add(1)
	updateEventCount.Add(2)
	deleteEventCount.Add(3)
	cacheResourceGauge.Set(4)
	queueSizeGauge.Set(5)
	httpRequestCount.Add(6)
	httpAvgDelayMs.Add(7)
	httpMaxDelayMs.Set(8)

	results := metricRecord.ExportMetricRecords()
	assert.Len(t, results, 1)
	result := results[0]
	assert.Equal(t, 3, len(result))
	assert.Equal(t, "{\"add_event_total\":\"1.0000\",\"delete_event_total\":\"3.0000\",\"http_request_total\":\"6.0000\",\"update_event_total\":\"2.0000\"}", result["counters"])
	assert.Equal(t, "{\"avg_delay_ms\":\"7.0000\",\"cache_size\":\"4.0000\",\"max_delay_ms\":\"8.0000\",\"queue_size\":\"5.0000\"}", result["gauges"])
	assert.Equal(t, "{\"cluster_id\":\"test-cluster-id\",\"metric_category\":\"runner\",\"project\":\"test-project\",\"runner_name\":\"k8s_meta\"}", result["labels"])
}

func TestExportMetricRecordsWithDayamicLabels(t *testing.T) {
	metricsRecord := &MetricsRecord{
		Labels: []LabelPair{
			{
				Key:   "PluginType",
				Value: "flusher_http",
			},
			{
				Key:   "PluginId",
				Value: "13",
			},
		},
	}
	constMetricLabels := map[string]string{"RemoteURL": "http://localhost:8081"}
	matchedEvents := NewCounterMetricVectorAndRegister(metricsRecord, "http_flusher_matched_events", constMetricLabels, nil).WithLabels()
	unmatchedEvents := NewCounterMetricVectorAndRegister(metricsRecord, "http_flusher_unmatched_events", constMetricLabels, nil).WithLabels()
	droppedEvents := NewCounterMetricVectorAndRegister(metricsRecord, "http_flusher_dropped_events", constMetricLabels, nil).WithLabels()
	retryCount := NewCounterMetricVectorAndRegister(metricsRecord, "http_flusher_retry_count", constMetricLabels, nil).WithLabels()
	flushFailure := NewCounterMetricVectorAndRegister(metricsRecord, "http_flusher_flush_failure_count", constMetricLabels, nil).WithLabels()
	flushLatency := NewAverageMetricVectorAndRegister(metricsRecord, "http_flusher_flush_latency_ns", constMetricLabels, nil).WithLabels()
	flushStatusCodeCount := NewCounterMetricVectorAndRegister(metricsRecord, "http_flusher_status_code_count", constMetricLabels, []string{"status_code"})
	flushErrorLevelCount := NewCounterMetricVectorAndRegister(metricsRecord, "http_flusher_error_count", constMetricLabels, []string{"level", "reason"})

	matchedEvents.Add(1)
	unmatchedEvents.Add(2)
	droppedEvents.Add(3)
	retryCount.Add(5)
	flushFailure.Add(6)
	flushLatency.Add(7)
	flushStatusCodeCount.WithLabels(LabelPair{Key: "status_code", Value: "200"}).Add(8)
	flushStatusCodeCount.WithLabels(LabelPair{Key: "status_code", Value: "400"}).Add(9)
	flushErrorLevelCount.WithLabels(LabelPair{Key: "level", Value: "error"}, LabelPair{Key: "reason", Value: "timeout"}).Add(10)
	flushErrorLevelCount.WithLabels(LabelPair{Key: "level", Value: "warn"}, LabelPair{Key: "reason", Value: "retry"}).Add(11)
	flushErrorLevelCount.WithLabels(LabelPair{Key: "level", Value: "error"}, LabelPair{Key: "reason", Value: "dropped"}).Add(12)

	results := metricsRecord.ExportMetricRecords()
	assert.Len(t, results, 6)

	expectedResults := []struct {
		counter string
		gauge   string
		labels  string
	}{
		{
			counter: "{\"http_flusher_dropped_events\":\"3.0000\",\"http_flusher_flush_failure_count\":\"6.0000\",\"http_flusher_matched_events\":\"1.0000\",\"http_flusher_retry_count\":\"5.0000\",\"http_flusher_unmatched_events\":\"2.0000\"}",
			gauge:   "{\"http_flusher_flush_latency_ns\":\"7.0000\"}",
			labels:  "{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\"}",
		},
		{
			counter: "{\"http_flusher_status_code_count\":\"8.0000\"}",
			gauge:   "{}",
			labels:  "{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\",\"status_code\":\"200\"}",
		},
		{
			counter: "{\"http_flusher_status_code_count\":\"9.0000\"}",
			gauge:   "{}",
			labels:  "{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\",\"status_code\":\"400\"}",
		},
		{
			counter: "{\"http_flusher_error_count\":\"10.0000\"}",
			gauge:   "{}",
			labels:  "{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\",\"level\":\"error\",\"reason\":\"timeout\"}",
		},
		{
			counter: "{\"http_flusher_error_count\":\"11.0000\"}",
			gauge:   "{}",
			labels:  "{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\",\"level\":\"warn\",\"reason\":\"retry\"}",
		},
		{
			counter: "{\"http_flusher_error_count\":\"12.0000\"}",
			gauge:   "{}",
			labels:  "{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\",\"level\":\"error\",\"reason\":\"dropped\"}",
		},
	}

	matched := 0
	for _, result := range results {
		for _, expect := range expectedResults {
			if result[MetricLabelPrefix] == expect.labels {
				matched++
				assert.Equal(t, expect.counter, result["counters"])
				assert.Equal(t, expect.gauge, result["gauges"])
			}
		}
	}
	assert.Equal(t, matched, len(results))
}

func TestExportMetricRecordsWithNextHasEmptyLabels(t *testing.T) {
	metricsRecord := &MetricsRecord{
		Labels: []LabelPair{
			{
				Key:   "PluginType",
				Value: "flusher_http",
			},
			{
				Key:   "PluginId",
				Value: "13",
			},
		},
	}
	constMetricLabels := map[string]string{"RemoteURL": "http://localhost:8081"}
	metric1 := NewCounterMetricVectorAndRegister(metricsRecord, "http_flusher_matched_events", constMetricLabels, []string{"test0"})
	metric1.WithLabels(LabelPair{Key: "test0", Value: "value0"}).Add(1)

	metric2 := NewCounterMetricVectorAndRegister(metricsRecord, "http_flusher_unmatched_events", constMetricLabels, nil).WithLabels()
	metric2.Add(2)

	results := metricsRecord.ExportMetricRecords()
	assert.Len(t, results, 2)

	expectedResults := []struct {
		counter string
		gauge   string
		labels  string
	}{
		{
			counter: "{\"http_flusher_matched_events\":\"1.0000\"}",
			gauge:   "{}",
			labels:  "{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\",\"test0\":\"value0\"}",
		},
		{
			counter: "{\"http_flusher_unmatched_events\":\"2.0000\"}",
			gauge:   "{}",
			labels:  "{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\"}",
		},
	}

	matched := 0
	for _, result := range results {
		for _, expect := range expectedResults {
			if result[MetricLabelPrefix] == expect.labels {
				matched++
				assert.Equal(t, expect.counter, result["counters"])
				assert.Equal(t, expect.gauge, result["gauges"])
			}
		}
	}
	assert.Equal(t, matched, len(results))
}

func TestExportMetricRecordsWithPrevHasEmptyLabels(t *testing.T) {
	metricsRecord := &MetricsRecord{
		Labels: []LabelPair{
			{
				Key:   "PluginType",
				Value: "flusher_http",
			},
			{
				Key:   "PluginId",
				Value: "13",
			},
		},
	}
	constMetricLabels := map[string]string{"RemoteURL": "http://localhost:8081"}
	metric2 := NewCounterMetricVectorAndRegister(metricsRecord, "http_flusher_unmatched_events", constMetricLabels, nil).WithLabels()
	metric2.Add(2)

	metric1 := NewCounterMetricVectorAndRegister(metricsRecord, "http_flusher_matched_events", constMetricLabels, []string{"test0"})
	metric1.WithLabels(LabelPair{Key: "test0", Value: "value0"}).Add(1)

	results := metricsRecord.ExportMetricRecords()
	assert.Len(t, results, 2)

	expectedResults := []struct {
		counter string
		gauge   string
		labels  string
	}{
		{
			counter: "{\"http_flusher_unmatched_events\":\"2.0000\"}",
			gauge:   "{}",
			labels:  "{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\"}",
		},
		{
			counter: "{\"http_flusher_matched_events\":\"1.0000\"}",
			gauge:   "{}",
			labels:  "{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\",\"test0\":\"value0\"}",
		},
	}

	matched := 0
	for _, result := range results {
		for _, expect := range expectedResults {
			if result[MetricLabelPrefix] == expect.labels {
				matched++
				assert.Equal(t, expect.counter, result["counters"])
				assert.Equal(t, expect.gauge, result["gauges"])
			}
		}
	}
	assert.Equal(t, matched, len(results))
}

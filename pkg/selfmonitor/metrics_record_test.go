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
	"encoding/json"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
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

	result := metricRecord.ExportMetricRecords()
	assert.Equal(t, 3, len(result))
	assert.Equal(t, "{\"add_event_total\":\"1.0000\",\"delete_event_total\":\"3.0000\",\"http_request_total\":\"6.0000\",\"update_event_total\":\"2.0000\"}", result["counters"])
	assert.Equal(t, "{\"avg_delay_ms\":\"7.0000\",\"cache_size\":\"4.0000\",\"max_delay_ms\":\"8.0000\",\"queue_size\":\"5.0000\"}", result["gauges"])
	assert.Equal(t, "{\"cluster_id\":\"test-cluster-id\",\"metric_category\":\"runner\",\"project\":\"test-project\",\"runner_name\":\"k8s_meta\"}", result["labels"])
}

func TestExportMetricRecords_AllTypes(t *testing.T) {
	metricRecord := MetricsRecord{
		Labels: []LabelPair{
			{Key: MetricLabelKeyMetricCategory, Value: MetricLabelValueMetricCategoryPluginSource},
			{Key: MetricLabelKeyPluginType, Value: "service_canal"},
		},
	}
	delta := NewCounterMetricAndRegister(&metricRecord, MetricPluginOutEventsTotal)
	cumulative := NewCumulativeCounterMetricAndRegister(&metricRecord, MetricPluginOutSizeBytes)
	gauge := NewGaugeMetricAndRegister(&metricRecord, MetricPluginSourceSizeBytes)
	avg := NewAverageMetricAndRegister(&metricRecord, MetricPluginCollectAvgCostTimeMs)
	maxDelay := NewMaxMetricAndRegister(&metricRecord, MetricRunnerK8sMetaHTTPMaxDelayMs)
	latency := NewLatencyMetricAndRegister(&metricRecord, MetricPluginTotalProcessTimeMs)
	str := NewStringMetricAndRegister(&metricRecord, MetricPluginBinlogFilename)

	delta.Add(3)
	cumulative.Add(9)
	gauge.Set(42)
	avg.Add(10)
	avg.Add(20)
	maxDelay.Set(8)
	latency.Observe(2000)
	str.Set("mysql-bin.000001")

	result := metricRecord.ExportMetricRecords()

	var counters map[string]string
	require.NoError(t, json.Unmarshal([]byte(result[MetricCounterPrefix]), &counters))
	assert.Equal(t, "3.0000", counters[MetricPluginOutEventsTotal])
	assert.Equal(t, "9.0000", counters[MetricPluginOutSizeBytes])

	var gauges map[string]string
	require.NoError(t, json.Unmarshal([]byte(result[MetricGaugePrefix]), &gauges))
	assert.Equal(t, "42.0000", gauges[MetricPluginSourceSizeBytes])
	assert.Equal(t, "15.0000", gauges[MetricPluginCollectAvgCostTimeMs])
	assert.Equal(t, "8.0000", gauges[MetricRunnerK8sMetaHTTPMaxDelayMs])
	assert.Equal(t, "2.0000", gauges[MetricPluginTotalProcessTimeMs]) // 2000ns -> 2us

	var labels map[string]string
	require.NoError(t, json.Unmarshal([]byte(result[MetricLabelPrefix]), &labels))
	assert.Equal(t, MetricLabelValueMetricCategoryPluginSource, labels[MetricLabelKeyMetricCategory])
	assert.Equal(t, "service_canal", labels[MetricLabelKeyPluginType])
	assert.Equal(t, "mysql-bin.000001", labels[MetricPluginBinlogFilename])

	parsed, err := ParseMetricExportRecord(result)
	require.NoError(t, err)
	assert.Equal(t, MetricLabelValueMetricCategoryPluginSource, parsed.Category)
	assert.Equal(t, uint64(3), parsed.Counters[MetricPluginOutEventsTotal])
	assert.Equal(t, uint64(9), parsed.Counters[MetricPluginOutSizeBytes])
	assert.Equal(t, 42.0, parsed.Gauges[MetricPluginSourceSizeBytes])
	assert.Equal(t, 15.0, parsed.Gauges[MetricPluginCollectAvgCostTimeMs])
	assert.Equal(t, "mysql-bin.000001", parsed.Labels[MetricPluginBinlogFilename])
	_, hasCategory := parsed.Labels[MetricLabelKeyMetricCategory]
	assert.False(t, hasCategory)
}

func TestCloneLabelsWithCategory(t *testing.T) {
	labels := []LabelPair{
		{Key: MetricLabelKeyMetricCategory, Value: MetricLabelValueMetricCategoryPlugin},
		{Key: MetricLabelKeyPluginType, Value: "service_mysql"},
		{Key: MetricLabelKeyPluginID, Value: "1"},
	}
	got := CloneLabelsWithCategory(labels, MetricLabelValueMetricCategoryPluginSource)
	assert.Equal(t, []LabelPair{
		{Key: MetricLabelKeyMetricCategory, Value: MetricLabelValueMetricCategoryPluginSource},
		{Key: MetricLabelKeyPluginType, Value: "service_mysql"},
		{Key: MetricLabelKeyPluginID, Value: "1"},
	}, got)
	assert.Equal(t, MetricLabelValueMetricCategoryPlugin, labels[0].Value)

	withoutCategory := []LabelPair{{Key: MetricLabelKeyPluginType, Value: "service_mysql"}}
	got = CloneLabelsWithCategory(withoutCategory, MetricLabelValueMetricCategoryPluginSource)
	assert.Equal(t, MetricLabelKeyMetricCategory, got[0].Key)
	assert.Equal(t, MetricLabelValueMetricCategoryPluginSource, got[0].Value)
	assert.Equal(t, "service_mysql", got[1].Value)
}

func TestExportKind(t *testing.T) {
	assert.Equal(t, MetricCounterPrefix, exportKind(CounterType))
	assert.Equal(t, MetricCounterPrefix, exportKind(CumulativeCounterType))
	assert.Equal(t, MetricGaugePrefix, exportKind(GaugeType))
	assert.Equal(t, MetricGaugePrefix, exportKind(AverageType))
	assert.Equal(t, MetricGaugePrefix, exportKind(MaxType))
	assert.Equal(t, MetricGaugePrefix, exportKind(LatencyType))
	assert.Equal(t, MetricLabelPrefix, exportKind(StringType))
	assert.Equal(t, MetricGaugePrefix, exportKind(RateType))
}

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

	result := metricRecord.ExportMetricRecords()
	assert.Equal(t, 3, len(result))
	assert.Equal(t, "{\"add_event_total\":\"1.0000\",\"delete_event_total\":\"3.0000\",\"http_request_total\":\"6.0000\",\"update_event_total\":\"2.0000\"}", result["counters"])
	assert.Equal(t, "{\"avg_delay_ms\":\"7.0000\",\"cache_size\":\"4.0000\",\"max_delay_ms\":\"8.0000\",\"queue_size\":\"5.0000\"}", result["gauges"])
	assert.Equal(t, "{\"cluster_id\":\"test-cluster-id\",\"metric_category\":\"runner\",\"project\":\"test-project\",\"runner_name\":\"k8s_meta\"}", result["labels"])
}

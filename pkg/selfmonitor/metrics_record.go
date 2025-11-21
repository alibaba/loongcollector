// Copyright 2021 iLogtail Authors
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
	"sync"
)

const SelfMetricNameKey = "__name__"
const MetricLabelPrefix = "labels"
const MetricCounterPrefix = "counters"
const MetricGaugePrefix = "gauges"

type MetricsRecord struct {
	Labels []LabelPair

	sync.RWMutex
	MetricCollectors []MetricCollector
}

// RegisterMetricCollector is used for registering metric collector (vector).
func (m *MetricsRecord) RegisterMetricCollector(collector MetricCollector) {
	m.Lock()
	defer m.Unlock()
	m.MetricCollectors = append(m.MetricCollectors, collector)
}

// ExportMetricRecords exports all metrics bound to this metric record.
// The results may be a list of map[string]string, each map[string]string is a set of measurements has the same labels.
// for example:
// []{
// {"counters":"{\"http_flusher_dropped_events\":\"3.0000\",\"http_flusher_flush_failure_count\":\"6.0000\",\"http_flusher_matched_events\":\"1.0000\",\"http_flusher_retry_count\":\"5.0000\",\"http_flusher_unmatched_events\":\"2.0000\"}","gauges":"{\"http_flusher_flush_latency_ns\":\"7.0000\"}","labels":"{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\"}"}
// {"counters":"{\"http_flusher_status_code_count\":\"8.0000\"}","gauges":"{}","labels":"{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\",\"status_code\":\"200\"}"}
// {"counters":"{\"http_flusher_status_code_count\":\"9.0000\"}","gauges":"{}","labels":"{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\",\"status_code\":\"400\"}"}
// {"counters":"{\"http_flusher_error_count\":\"10.0000\"}","gauges":"{}","labels":"{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\",\"level\":\"error\",\"reason\":\"timeout\"}"}
// {"counters":"{\"http_flusher_error_count\":\"11.0000\"}","gauges":"{}","labels":"{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\",\"level\":\"warn\",\"reason\":\"retry\"}"}
// {"counters":"{\"http_flusher_error_count\":\"12.0000\"}","gauges":"{}","labels":"{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\",\"level\":\"error\",\"reason\":\"dropped\"}"}
// }
// Note:
// A metric may have three levels of labels
// 1. MetricsRecord Level Const Labels, like PluginType=flusher_http, PluginId=1
// 2. Metric Level Const Labels, for example, flusher_http may have a const label: RemoteURL=http://aliyun.com/write
// 3. Metric Level Dynamic Labels, like status_code=200, status_code=204
func (m *MetricsRecord) ExportMetricRecords() []map[string]string {
	m.RLock()
	defer m.RUnlock()

	res := []map[string]string{}

	curCounters := map[string]string{}
	currGauges := map[string]string{}
	commonLabels := m.getConstLabels()
	currLabels := m.getConstLabels()
	currSeriesCount := 0

	for _, metricCollector := range m.MetricCollectors {
		metricVectors := metricCollector.Collect()

		for _, metric := range metricVectors {
			measurement := metric.Export()
			if len(measurement) == 0 {
				continue
			}

			if currSeriesCount > 0 && hasDifferentLabels(currLabels, commonLabels, measurement) {
				res = append(res, buildRecord(currLabels, currGauges, curCounters))
				currLabels = m.getConstLabels()
				curCounters = map[string]string{}
				currGauges = map[string]string{}
				currSeriesCount = 0
			}

			currName := getMetriName(measurement)
			if currName == "" {
				continue
			}

			currValue := getMetricValue(measurement)
			if currValue == "" {
				continue
			}

			currMeasurementLabels := getLabels(measurement)
			for key, value := range currMeasurementLabels {
				currLabels[key] = value
			}

			if metric.Type() == CounterType {
				curCounters[currName] = currValue
				currSeriesCount++
			}
			if metric.Type() == GaugeType {
				currGauges[currName] = currValue
				currSeriesCount++
			}

		}
	}

	if currSeriesCount > 0 {
		res = append(res, buildRecord(currLabels, currGauges, curCounters))
	}
	return res
}

func (m *MetricsRecord) getConstLabels() map[string]string {
	labels := map[string]string{}
	for _, label := range m.Labels {
		labels[label.Key] = label.Value
	}
	return labels
}

// hasDifferentLabels checks if the next measurement has different labels compared to current labels.
// 1. currLabels: Labels from the current metric instance
// 2. commonLabels: Shared labels that should be present in all metrics
// 3. nextMeasurement: The next measurement to check
func hasDifferentLabels(currLabels map[string]string, commonLabels map[string]string, nextMeasurement map[string]string) bool {
	// Check if next measurement's dynamic labels differ from current labels

	newLabelCount := 0
	for key, value := range nextMeasurement {
		if !isLabel(key, nextMeasurement) {
			continue
		}

		// If label exists but value differs, labels are different
		currValue, exists := currLabels[key]
		if !exists || currValue != value {
			return true
		}
		newLabelCount++
	}

	// Check if common labels differ from current labels.
	// This only happens if the current metrics override the common labels.
	for key, value := range commonLabels {
		if value != currLabels[key] {
			return true
		}
		if _, ok := nextMeasurement[key]; !ok {
			newLabelCount++
		}
	}

	return newLabelCount != len(currLabels)
}

// getMetriName returns the name of the metric.
// singleMeasurement is a single measurement of a metric.
// for example, a measurement of "flusher_http_flush_count" may looks like:
// {__name__="flusher_http_flush_count", flusher_http_flush_count=1024, remote_url=http://localhost:8080/write, status_code=200}
// and its name is "flusher_http_flush_count".
func getMetriName(singleMeasurement map[string]string) string {
	return singleMeasurement[SelfMetricNameKey]
}

// getMetricValue returns the value of the metric.
// for example, a measurement of "flusher_http_flush_count" may looks like:
// {__name__="flusher_http_flush_count", flusher_http_flush_count=1024, remote_url=http://localhost:8080/write, status_code=200}
// and its value is "1024".
func getMetricValue(singleMeasurement map[string]string) string {
	return singleMeasurement[getMetriName(singleMeasurement)]
}

// getLabels returns the labels of the metric.
// for example, a measurement of "flusher_http_flush_count" may looks like:
// {__name__="flusher_http_flush_count", flusher_http_flush_count=1024, remote_url=http://localhost:8080/write, status_code=200}
// and its labels is {"remote_url": "http://localhost:8080/write", "status_code": "200"}
func getLabels(singleMeasurement map[string]string) map[string]string {
	labels := map[string]string{}
	for key, value := range singleMeasurement {
		if isLabel(key, singleMeasurement) {
			labels[key] = value
		}
	}
	return labels
}

// isLabel returns true if the key is a label.
func isLabel(key string, singleMetric map[string]string) bool {
	return key != SelfMetricNameKey && key != getMetriName(singleMetric)
}

func buildRecord(labels, gauges, counters map[string]string) map[string]string {
	records := make(map[string]string, 3)
	labelsStr, _ := json.Marshal(labels)
	records[MetricLabelPrefix] = string(labelsStr)

	gaugesStr, _ := json.Marshal(gauges)
	records[MetricGaugePrefix] = string(gaugesStr)

	countersStr, _ := json.Marshal(counters)
	records[MetricCounterPrefix] = string(countersStr)
	return records
}

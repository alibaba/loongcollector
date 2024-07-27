// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package helper

import (
	"fmt"
	"strconv"
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

func Test_MetricVectorWithEmptyLabel(t *testing.T) {
	v := NewCumulativeCounterMetricVector("test", nil, nil)
	v.WithLabels(pipeline.Label{Key: "test_label", Value: "test_value"}).Add(1)

	v.WithLabels().Add(1)
	collector, ok := v.(pipeline.MetricCollector)
	assert.True(t, ok)
	collectedMetrics := collector.Collect()
	assert.Equal(t, 1, len(collectedMetrics))

	for _, v := range collectedMetrics {
		counter, ok := v.(*cumulativeCounterImp)
		assert.True(t, ok)
		assert.Equal(t, "test", counter.Name())
		assert.Equal(t, 1.0, counter.Collect().Value)
	}
}

func Test_MetricVectorWithConstLabel(t *testing.T) {
	v := NewCumulativeCounterMetricVector("test", map[string]string{"host": "host1", "plugin_id": "2"}, nil)
	v.WithLabels(pipeline.Label{Key: "test_label", Value: "test_value"}).Add(1)

	v.WithLabels().Add(2)
	collector, ok := v.(pipeline.MetricCollector)
	assert.True(t, ok)
	collectedMetrics := collector.Collect()
	assert.Equal(t, 1, len(collectedMetrics))

	expectedContents := []map[string]string{
		{"host": "host1", "plugin_id": "2", "test_label": "test_value", "__name__": "test", "test": "2.0000"},
	}
	for i, v := range collectedMetrics {
		counter, ok := v.(*cumulativeCounterImp)
		assert.True(t, ok)
		assert.Equal(t, "test", counter.Name())
		assert.Equal(t, 2.0, counter.Collect().Value)
		log := &protocol.Log{}
		v.Serialize(log)
		for _, v := range log.Contents {
			assert.Equal(t, expectedContents[i][v.Key], v.Value)
		}
	}
}

func Test_CounterMetricVectorWithDynamicLabel(t *testing.T) {
	metricName := "test_counter_vector"
	v := NewCumulativeCounterMetricVector(metricName,
		map[string]string{"host": "host1", "plugin_id": "3"},
		[]string{"label1", "label2", "label3", "label5"},
	)

	v.WithLabels(pipeline.Label{Key: "label4", Value: "value4"}).Add(1)
	v.WithLabels().Add(-1)
	seriesCount := 500
	for i := 0; i < seriesCount; i++ {
		v.WithLabels(pipeline.Label{Key: "label1", Value: fmt.Sprintf("value_%d", i)}).Add(int64(i))
	}

	collector, ok := v.(pipeline.MetricCollector)
	assert.True(t, ok)
	collectedMetrics := collector.Collect()
	assert.Equal(t, seriesCount+1, len(collectedMetrics))

	expectedContents := []map[string]string{}

	for i := 0; i < seriesCount; i++ {
		expectedContents = append(expectedContents, map[string]string{
			"host":                "host1",
			"plugin_id":           "3",
			"label1":              fmt.Sprintf("value_%d", i),
			"label2":              defaultTagValue,
			"label3":              defaultTagValue,
			"label5":              defaultTagValue,
			"__name__":            metricName,
			"test_counter_vector": fmt.Sprintf("%d.0000", i),
		})
	}

	for _, metric := range collectedMetrics {
		counter, ok := metric.(*cumulativeCounterImp)
		assert.True(t, ok)
		assert.Equal(t, metricName, counter.Name())
		valueAsIndex := int(counter.Collect().Value)
		log := &protocol.Log{}
		metric.Serialize(log)
		if valueAsIndex >= 0 {
			for _, v := range log.Contents {
				assert.Equal(t, expectedContents[valueAsIndex][v.Key], v.Value)
			}
		}
	}
}

func Test_AverageMetricVectorWithDynamicLabel(t *testing.T) {
	metricName := "test_average_vector"
	v := NewAverageMetricVector(metricName,
		map[string]string{"host": "host1", "plugin_id": "3"},
		[]string{"label1", "label2", "label3", "label5"},
	)

	v.WithLabels(pipeline.Label{Key: "label4", Value: "value4"}).Add(1)
	v.WithLabels().Add(-1)
	seriesCount := 500
	for i := 0; i < seriesCount; i++ {
		v.WithLabels(pipeline.Label{Key: "label1", Value: fmt.Sprintf("value_%d", i)}).Add(int64(i))
	}

	collector, ok := v.(pipeline.MetricCollector)
	assert.True(t, ok)
	collectedMetrics := collector.Collect()
	assert.Equal(t, seriesCount+1, len(collectedMetrics))

	expectedContents := []map[string]string{}

	for i := 0; i < seriesCount; i++ {
		expectedContents = append(expectedContents, map[string]string{
			"host":      "host1",
			"plugin_id": "3",
			"label1":    fmt.Sprintf("value_%d", i),
			"label2":    defaultTagValue,
			"label3":    defaultTagValue,
			"label5":    defaultTagValue,
			"__name__":  metricName,
			metricName:  fmt.Sprintf("%d.0000", i),
		})
	}

	for i, metric := range collectedMetrics {
		counter, ok := metric.(*averageImp)
		assert.True(t, ok)
		assert.Equal(t, metricName, counter.Name())
		valueAsIndex := int(counter.Collect().Value)
		log := &protocol.Log{}
		metric.Serialize(log)
		if valueAsIndex >= 0 {
			for _, v := range log.Contents {
				if expectedContents[valueAsIndex][v.Key] != v.Value {
					t.Errorf("index: %d, actual: %v vs expcted: %v", i, v.Value, expectedContents[valueAsIndex][v.Key])
				}
			}
		}
	}
}

func Test_LatencyMetricVectorWithDynamicLabel(t *testing.T) {
	metricName := "test_latency_vector"
	v := NewLatencyMetricVector(metricName,
		map[string]string{"host": "host1", "plugin_id": "3"},
		[]string{"label1", "label2", "label3", "label5"},
	)

	v.WithLabels(pipeline.Label{Key: "label4", Value: "value4"}).Observe(0)
	seriesCount := 500

	v.WithLabels().Observe(float64(seriesCount * 2 * 1000))
	for i := 0; i < seriesCount; i++ {
		v.WithLabels(pipeline.Label{Key: "label1", Value: fmt.Sprintf("value_%d", i)}).Observe(float64(i * 1000))
	}

	collector, ok := v.(pipeline.MetricCollector)
	assert.True(t, ok)
	collectedMetrics := collector.Collect()
	assert.Equal(t, seriesCount+1, len(collectedMetrics))

	expectedContents := []map[string]string{}

	for i := 0; i < seriesCount; i++ {
		expectedContents = append(expectedContents, map[string]string{
			"host":      "host1",
			"plugin_id": "3",
			"label1":    fmt.Sprintf("value_%d", i),
			"label2":    defaultTagValue,
			"label3":    defaultTagValue,
			"label5":    defaultTagValue,
			"__name__":  metricName,
			metricName:  fmt.Sprintf("%d.0000", i),
		})
	}

	for i, metric := range collectedMetrics {
		latency, ok := metric.(*latencyImp)
		assert.True(t, ok)
		assert.Equal(t, metricName, latency.Name())
		log := &protocol.Log{}
		metric.Serialize(log)
		valueAsIndex := 0 // int(latency.Collect().Value / 1000)
		metricName := GetMetricName(log)
		for _, v := range log.Contents {
			if v.Key == metricName {
				valueAsIndexF, _ := strconv.ParseFloat(v.Value, 64)
				valueAsIndex = int(valueAsIndexF)
				break
			}
		}
		if valueAsIndex >= 0 && valueAsIndex < len(expectedContents) {
			for _, v := range log.Contents {
				if expectedContents[valueAsIndex][v.Key] != v.Value {
					t.Errorf("index: %d, actual: %v vs expcted: %v", i, v.Value, expectedContents[valueAsIndex][v.Key])
				}
			}
		}
	}
}

func Test_GaugeMetricVectorWithDynamicLabel(t *testing.T) {
	metricName := "test_gauge_vector"
	v := NewGaugeMetricVector(metricName,
		map[string]string{"host": "host1", "plugin_id": "3"},
		[]string{"label1", "label2", "label3", "label5"},
	)

	v.WithLabels(pipeline.Label{Key: "label4", Value: "value4"}).Set(0)
	seriesCount := 500
	v.WithLabels().Set(float64(seriesCount * 2 * 1000))

	for i := 0; i < seriesCount; i++ {
		v.WithLabels(pipeline.Label{Key: "label1", Value: fmt.Sprintf("value_%d", i)}).Set(float64(i))
	}

	collector, ok := v.(pipeline.MetricCollector)
	assert.True(t, ok)
	collectedMetrics := collector.Collect()
	assert.Equal(t, seriesCount+1, len(collectedMetrics))

	expectedContents := []map[string]string{}

	for i := 0; i < seriesCount; i++ {
		expectedContents = append(expectedContents, map[string]string{
			"host":      "host1",
			"plugin_id": "3",
			"label1":    fmt.Sprintf("value_%d", i),
			"label2":    defaultTagValue,
			"label3":    defaultTagValue,
			"label5":    defaultTagValue,
			"__name__":  metricName,
			metricName:  fmt.Sprintf("%d.0000", i),
		})
	}

	for i, metric := range collectedMetrics {
		counter, ok := metric.(*gaugeImp)
		assert.True(t, ok)
		assert.Equal(t, metricName, counter.Name())
		valueAsIndex := int(counter.Collect().Value)
		log := &protocol.Log{}
		metric.Serialize(log)
		if valueAsIndex >= 0 && valueAsIndex < len(expectedContents) {
			for _, v := range log.Contents {
				if expectedContents[valueAsIndex][v.Key] != v.Value {
					t.Errorf("index: %d, actual: %v vs expcted: %v", i, v.Value, expectedContents[valueAsIndex][v.Key])
				}
			}
		}
	}
}

func Test_StrMetricVectorWithDynamicLabel(t *testing.T) {
	metricName := "test_str_vector"
	v := NewStringMetricVector(metricName,
		map[string]string{"host": "host1", "plugin_id": "3"},
		[]string{"label1", "label2", "label3", "label5"},
	)

	v.WithLabels(pipeline.Label{Key: "label4", Value: "value4"}).Set("string")
	seriesCount := 500

	v.WithLabels().Set(strconv.Itoa(seriesCount * 2 * 1000))

	for i := 0; i < seriesCount; i++ {
		v.WithLabels(pipeline.Label{Key: "label1", Value: fmt.Sprintf("value_%d", i)}).Set(strconv.Itoa(i))
	}

	collector, ok := v.(pipeline.MetricCollector)
	assert.True(t, ok)
	collectedMetrics := collector.Collect()
	assert.Equal(t, seriesCount+1, len(collectedMetrics))

	expectedContents := []map[string]string{}

	for i := 0; i < seriesCount; i++ {
		expectedContents = append(expectedContents, map[string]string{
			"host":      "host1",
			"plugin_id": "3",
			"label1":    fmt.Sprintf("value_%d", i),
			"label2":    defaultTagValue,
			"label3":    defaultTagValue,
			"label5":    defaultTagValue,
			"__name__":  metricName,
			metricName:  strconv.Itoa(i),
		})
	}

	for i, metric := range collectedMetrics {
		counter, ok := metric.(*strMetricImp)
		assert.True(t, ok)
		assert.Equal(t, metricName, counter.Name())
		valueAsIndex, err := strconv.Atoi(counter.Collect().Value)
		assert.NoError(t, err)
		log := &protocol.Log{}
		metric.Serialize(log)
		if valueAsIndex >= 0 && valueAsIndex < len(expectedContents) {
			for _, v := range log.Contents {
				if expectedContents[valueAsIndex][v.Key] != v.Value {
					t.Errorf("index: %d, actual: %v vs expcted: %v", i, v.Value, expectedContents[valueAsIndex][v.Key])
				}
			}
		}
	}
}

func Test_NewCounterMetricAndRegister(t *testing.T) {
	metricsRecord := &pipeline.MetricsRecord{Context: nil}
	counter := NewCumulativeCounterMetricAndRegister(metricsRecord, "test_counter")
	counter.Add(1)
	value := counter.Collect()
	assert.Equal(t, 1.0, value.Value)
}

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

package selfmonitor

import (
	"fmt"
	"strconv"
	"sync/atomic"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
)

func Test_MetricVectorWithEmptyLabel(t *testing.T) {
	v := NewCumulativeCounterMetricVector("test", nil, nil)
	v.WithLabels(LabelPair{Key: "test_label", Value: "test_value"}).Add(1)

	v.WithLabels().Add(1)
	collector, ok := v.(MetricCollector)
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
	v.WithLabels(LabelPair{Key: "test_label", Value: "test_value"}).Add(1)

	v.WithLabels().Add(2)
	collector, ok := v.(MetricCollector)
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
		records := v.Export()
		for k, v := range records {
			assert.Equal(t, expectedContents[i][k], v)
		}
	}
}

func Test_CounterMetricVectorWithDynamicLabel(t *testing.T) {
	metricName := "test_counter_vector"
	v := NewCumulativeCounterMetricVector(metricName,
		map[string]string{"host": "host1", "plugin_id": "3"},
		[]string{"label1", "label2", "label3", "label5"},
	)

	v.WithLabels(LabelPair{Key: "label4", Value: "value4"}).Add(1)
	v.WithLabels().Add(-1)
	seriesCount := 500
	for i := 0; i < seriesCount; i++ {
		v.WithLabels(LabelPair{Key: "label1", Value: fmt.Sprintf("value_%d", i)}).Add(int64(i))
	}

	collector, ok := v.(MetricCollector)
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
		records := metric.Export()
		if valueAsIndex >= 0 {
			for k, v := range records {
				assert.Equal(t, expectedContents[valueAsIndex][k], v)
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

	v.WithLabels(LabelPair{Key: "label4", Value: "value4"}).Add(1)
	v.WithLabels().Add(-1)
	seriesCount := 500
	for i := 0; i < seriesCount; i++ {
		v.WithLabels(LabelPair{Key: "label1", Value: fmt.Sprintf("value_%d", i)}).Add(int64(i))
	}

	collector, ok := v.(MetricCollector)
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
		records := metric.Export()
		if valueAsIndex >= 0 {
			for k, v := range records {
				if expectedContents[valueAsIndex][k] != v {
					t.Errorf("index: %d, actual: %v vs expcted: %v", i, v, expectedContents[valueAsIndex][k])
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

	v.WithLabels(LabelPair{Key: "label4", Value: "value4"}).Observe(0)
	seriesCount := 500

	v.WithLabels().Observe(float64(seriesCount * 2 * 1000))
	for i := 0; i < seriesCount; i++ {
		v.WithLabels(LabelPair{Key: "label1", Value: fmt.Sprintf("value_%d", i)}).Observe(float64(i * 1000))
	}

	collector, ok := v.(MetricCollector)
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
		records := metric.Export()
		valueAsIndex := 0 // int(latency.Collect().Value / 1000)
		metricName := func() string {
			for k, v := range records {
				if k == SelfMetricNameKey {
					return v
				}
			}
			return ""
		}()
		for k, v := range records {
			if k == metricName {
				valueAsIndexF, _ := strconv.ParseFloat(v, 64)
				valueAsIndex = int(valueAsIndexF)
				break
			}
		}
		if valueAsIndex >= 0 && valueAsIndex < len(expectedContents) {
			for k, v := range records {
				if expectedContents[valueAsIndex][k] != v {
					t.Errorf("index: %d, actual: %v vs expcted: %v", i, v, expectedContents[valueAsIndex][k])
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

	v.WithLabels(LabelPair{Key: "label4", Value: "value4"}).Set(0)
	seriesCount := 500
	v.WithLabels().Set(float64(seriesCount * 2 * 1000))

	for i := 0; i < seriesCount; i++ {
		v.WithLabels(LabelPair{Key: "label1", Value: fmt.Sprintf("value_%d", i)}).Set(float64(i))
	}

	collector, ok := v.(MetricCollector)
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
		records := metric.Export()
		if valueAsIndex >= 0 && valueAsIndex < len(expectedContents) {
			for k, v := range records {
				if expectedContents[valueAsIndex][k] != v {
					t.Errorf("index: %d, actual: %v vs expcted: %v", i, v, expectedContents[valueAsIndex][k])
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

	v.WithLabels(LabelPair{Key: "label4", Value: "value4"}).Set("string")
	seriesCount := 500

	v.WithLabels().Set(strconv.Itoa(seriesCount * 2 * 1000))

	for i := 0; i < seriesCount; i++ {
		v.WithLabels(LabelPair{Key: "label1", Value: fmt.Sprintf("value_%d", i)}).Set(strconv.Itoa(i))
	}

	collector, ok := v.(MetricCollector)
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
		records := metric.Export()
		if valueAsIndex >= 0 && valueAsIndex < len(expectedContents) {
			for k, v := range records {
				if expectedContents[valueAsIndex][k] != v {
					t.Errorf("index: %d, actual: %v vs expcted: %v", i, v, expectedContents[valueAsIndex][k])
				}
			}
		}
	}
}

func Test_NewCounterMetricAndRegister(t *testing.T) {
	metricsRecord := &MetricsRecord{}
	counter := NewCumulativeCounterMetricAndRegister(metricsRecord, "test_counter")
	counter.Add(1)
	value := counter.Collect()
	assert.Equal(t, 1.0, value.Value)
}

func withExpiration(expiration time.Duration) MetricOption {
	return func(option metricOption) metricOption {
		option.metricExpiration = expiration
		return option
	}
}

func TestMetricVectorDynamicLabelWithGC(t *testing.T) {
	constLabels := map[string]string{
		"constKey1": "constValue1",
	}

	counters := NewCounterMetricVector("test", constLabels, nil,
		WithDynamicLabel(),
		withExpiration(time.Second),
		WithGC(true),
	)
	cumulativeCounter := NewCumulativeCounterMetricVector("test", constLabels, nil,
		WithDynamicLabel(),
		withExpiration(time.Second),
		WithGC(true),
	)
	gauge := NewGaugeMetricVector("test", constLabels, nil,
		WithDynamicLabel(),
		withExpiration(time.Second),
		WithGC(true),
	)
	latency := NewLatencyMetricVector("test", constLabels, nil,
		WithDynamicLabel(),
		withExpiration(time.Second),
		WithGC(true),
	)
	avg := NewAverageMetricVector("test", constLabels, nil,
		WithDynamicLabel(),
		withExpiration(time.Second),
		WithGC(true),
	)
	histogram := NewHistogramMetricVector("test", constLabels, nil,
		WithDynamicLabel(),
		withExpiration(time.Second),
		WithGC(true),
	)
	cumulativeHistogram := NewCumulativeHistogramMetricVector("test", constLabels, nil,
		WithDynamicLabel(),
		withExpiration(time.Second),
		WithGC(true),
	)

	emitCount := 1000
	index := int64(0)
	allTags := genAllTags(3, 10)
	for i := 0; i < emitCount; i++ {
		counters.WithLabels(genNextTags(allTags, &index)...).Add(1)
	}
	ms := counters.(MetricCollector).Collect()
	assert.Len(t, ms, 1000)

	for i := 0; i < emitCount; i++ {
		cumulativeCounter.WithLabels(genNextTags(allTags, &index)...).Add(1)
	}
	ms = cumulativeCounter.(MetricCollector).Collect()
	assert.Len(t, ms, 1000)

	for i := 0; i < emitCount; i++ {
		gauge.WithLabels(genNextTags(allTags, &index)...).Set(1)
	}
	ms = gauge.(MetricCollector).Collect()
	assert.Len(t, ms, 1000)

	for i := 0; i < emitCount; i++ {
		latency.WithLabels(genNextTags(allTags, &index)...).Observe(1)
	}
	ms = latency.(MetricCollector).Collect()
	assert.Len(t, ms, 1000)

	for i := 0; i < emitCount; i++ {
		avg.WithLabels(genNextTags(allTags, &index)...).Add(1)
	}
	ms = avg.(MetricCollector).Collect()
	assert.Len(t, ms, 1000)

	for i := 0; i < emitCount; i++ {
		histogram.WithLabels(genNextTags(allTags, &index)...).Observe(1)
	}
	ms = histogram.(MetricCollector).Collect()
	assert.Len(t, ms, 1000)

	for i := 0; i < emitCount; i++ {
		cumulativeHistogram.WithLabels(genNextTags(allTags, &index)...).Observe(1)
	}
	ms = cumulativeHistogram.(MetricCollector).Collect()
	assert.Len(t, ms, 1000)

	time.Sleep(time.Second * 3)
	ms = counters.(MetricCollector).Collect()
	assert.Len(t, ms, 0)
	ms = cumulativeCounter.(MetricCollector).Collect()
	assert.Len(t, ms, 0)
	ms = gauge.(MetricCollector).Collect()
	assert.Len(t, ms, 0)
	ms = latency.(MetricCollector).Collect()
	assert.Len(t, ms, 0)
	ms = avg.(MetricCollector).Collect()
	assert.Len(t, ms, 0)
	ms = histogram.(MetricCollector).Collect()
	assert.Len(t, ms, 0)
	ms = cumulativeHistogram.(MetricCollector).Collect()
	assert.Len(t, ms, 0)
}

func genNextTags(allTagKVs [][]LabelPair, index *int64) []LabelPair {
	tagNumber := len(allTagKVs)
	tagDimension := len(allTagKVs[0])
	if tagNumber == 0 || tagDimension == 0 {
		return nil
	}
	tags := make([]LabelPair, tagNumber)
	valueTotalIndex := atomic.AddInt64(index, 1) - 1

	i := 0
	for i < tagNumber {
		currValueIdx := valueTotalIndex % int64(tagDimension)
		tags[tagNumber-1-i] = allTagKVs[tagNumber-1-i][currValueIdx]
		valueTotalIndex /= int64(len(allTagKVs[tagNumber-1-i]))
		i++
	}
	return tags
}

func genAllTags(tagNum, dimension int) [][]LabelPair {
	tags := make([][]LabelPair, tagNum)
	for i := 0; i < tagNum; i++ {
		for j := 0; j < dimension; j++ {
			tags[i] = append(tags[i], LabelPair{fmt.Sprintf("tag%d", i), fmt.Sprintf("value%d-%d", i, j)})
		}
	}
	return tags
}

func getTagNames(allTagKVs [][]LabelPair) (res []string) {
	for _, innerSlice := range allTagKVs {
		if len(innerSlice) > 0 {
			res = append(res, innerSlice[0].Key)
		}
	}
	return res
}

func TestMetricOverflow(t *testing.T) {
	constLabels := map[string]string{
		"constKey1": "constValue1",
	}

	allTags := genAllTags(2, 10) // 100

	cumulativeCounter := NewCumulativeCounterMetricVector("test", constLabels, getTagNames(allTags),
		WithExpiration(time.Second),
		WithCardinalityLimit(1000),
		WithGC(true),
	)

	index := int64(0)
	for i := 0; i < 200; i++ {
		cumulativeCounter.WithLabels(genNextTags(allTags, &index)...).Add(1)
	}

	ms := cumulativeCounter.(MetricCollector).Collect()
	assert.Len(t, ms, 100)

	cumulativeCounterWithCardinalityLimit50 := NewCumulativeCounterMetricVector("test", constLabels, getTagNames(allTags),
		WithExpiration(time.Second),
		WithCardinalityLimit(10),
		WithGC(true),
	)

	for i := 0; i < 200; i++ {
		cumulativeCounterWithCardinalityLimit50.WithLabels(genNextTags(allTags, &index)...).Add(1)
	}

	ms = cumulativeCounterWithCardinalityLimit50.(MetricCollector).Collect()
	assert.Len(t, ms, 11)
	last := ms[len(ms)-1]
	lastEvent := last.ExportEvent()
	assert.Equal(t, "test", lastEvent.GetName())
	assert.Equal(t, "constValue1", lastEvent.GetTags().Get("constKey1"))
	assert.Equal(t, "overflow", lastEvent.GetTags().Get("tag0"))

	cumulativeCounterWithCardinalityLimit50AndExpiration := NewCumulativeCounterMetricVector("test", constLabels, getTagNames(allTags),
		WithExpiration(time.Second),
		WithCardinalityLimit(10),
		WithOverflowKey("overflow2"),
	)

	for i := 0; i < 200; i++ {
		cumulativeCounterWithCardinalityLimit50AndExpiration.WithLabels(genNextTags(allTags, &index)...).Add(1)
	}

	ms2 := cumulativeCounterWithCardinalityLimit50AndExpiration.(MetricCollector).Collect()
	assert.Len(t, ms2, 11)
	last2 := ms2[len(ms2)-1]
	lastEvent2 := last2.ExportEvent()
	assert.Equal(t, "overflow2", lastEvent2.GetTags().Get("tag0"))

	time.Sleep(time.Second * 3)
	ms3 := cumulativeCounterWithCardinalityLimit50AndExpiration.(MetricCollector).Collect()
	assert.Len(t, ms3, 1)
	cardinality := cumulativeCounterWithCardinalityLimit50AndExpiration.(*MetricVectorImpl[CounterMetric]).cache.(*MapCache).currCardinality
	assert.Equal(t, int64(0), cardinality)
}

func TestMetricWithDynamicLabelOverflow(t *testing.T) {
	constLabels := map[string]string{
		"constKey1": "constValue1",
	}

	allTags := genAllTags(2, 10) // 100

	cumulativeCounter := NewCumulativeCounterMetricVector("test", constLabels, nil,
		WithExpiration(time.Second),
		WithCardinalityLimit(1000),
		WithDynamicLabel(),
	)

	index := int64(0)
	for i := 0; i < 200; i++ {
		cumulativeCounter.WithLabels(genNextTags(allTags, &index)...).Add(1)
	}

	ms := cumulativeCounter.(MetricCollector).Collect()
	assert.Len(t, ms, 100)

	cumulativeCounterWithCardinalityLimit50 := NewCumulativeCounterMetricVector("test", constLabels, getTagNames(allTags),
		WithExpiration(time.Second),
		WithCardinalityLimit(10),
		WithDynamicLabel(),
		WithGC(true),
	)

	for i := 0; i < 200; i++ {
		cumulativeCounterWithCardinalityLimit50.WithLabels(genNextTags(allTags, &index)...).Add(1)
	}

	ms = cumulativeCounterWithCardinalityLimit50.(MetricCollector).Collect()
	assert.Len(t, ms, 11)
	last := ms[len(ms)-1]
	lastEvent := last.ExportEvent()
	assert.Equal(t, "test", lastEvent.GetName())
	assert.Equal(t, "constValue1", lastEvent.GetTags().Get("constKey1"))
	assert.Equal(t, "true", lastEvent.GetTags().Get("overflow"))

	cumulativeCounterWithCardinalityLimit50AndExpiration := NewCumulativeCounterMetricVector("test", constLabels, getTagNames(allTags),
		WithExpiration(time.Second),
		WithCardinalityLimit(10),
		WithOverflowKey("overflow2"),
		WithDynamicLabel(),
		WithGC(true),
	)

	for i := 0; i < 200; i++ {
		cumulativeCounterWithCardinalityLimit50AndExpiration.WithLabels(genNextTags(allTags, &index)...).Add(1)
	}

	ms2 := cumulativeCounterWithCardinalityLimit50AndExpiration.(MetricCollector).Collect()
	assert.Len(t, ms2, 11)
	last2 := ms2[len(ms2)-1]
	lastEvent2 := last2.ExportEvent()
	assert.Equal(t, "true", lastEvent2.GetTags().Get("overflow2"))

	time.Sleep(time.Second * 3)
	ms3 := cumulativeCounterWithCardinalityLimit50AndExpiration.(MetricCollector).Collect()
	assert.Len(t, ms3, 1)
	cardinality := cumulativeCounterWithCardinalityLimit50AndExpiration.(*MetricVectorImpl[CounterMetric]).cache.(*MapCache).currCardinality
	assert.Equal(t, int64(0), cardinality)
}

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
	"errors"
	"strconv"
	"sync"
	"sync/atomic"
	"time"

	"github.com/alibaba/ilogtail/pkg/helper/math"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

var (
	_ CounterMetric = (*cumulativeCounterImp)(nil)
	_ CounterMetric = (*counterImp)(nil)
	_ CounterMetric = (*averageImp)(nil)
	_ GaugeMetric   = (*gaugeImp)(nil)
	_ LatencyMetric = (*latencyImp)(nil)
	_ StringMetric  = (*strMetricImp)(nil)

	_ CounterMetric = (*errorNumericMetric)(nil)
	_ GaugeMetric   = (*errorNumericMetric)(nil)
	_ LatencyMetric = (*errorNumericMetric)(nil)
	_ StringMetric  = (*errorStrMetric)(nil)
)

func newMetric(metricType SelfMetricType, metricSet MetricSet, labelValues []string) Metric {
	switch metricType {
	case CumulativeCounterType:
		return newCumulativeCounter(metricSet, labelValues)
	case AverageType:
		return newAverage(metricSet, labelValues)
	case MaxType:
		return newMax(metricSet, labelValues)
	case CounterType:
		return newDeltaCounter(metricSet, labelValues)
	case GaugeType:
		return newGauge(metricSet, labelValues)
	case StringType:
		return newStringMetric(metricSet, labelValues)
	case LatencyType:
		return newLatency(metricSet, labelValues)
	}
	return newErrorMetric(metricType, errors.New("invalid metric type"))
}

// ErrorMetrics always return error.
func newErrorMetric(metricType SelfMetricType, err error) Metric {
	switch metricType {
	case StringType:
		return newErrorStringMetric(err)
	default:
		return newErrorNumericMetric(err)
	}
}

// Deprecated: Use deltaImp instead.
// cumulativeCounterImp is a counter metric that can be incremented or decremented.
// It gets the cumulative value of the counter.
type cumulativeCounterImp struct {
	value int64
	Series
}

func newCumulativeCounter(ms MetricSet, labelValues []string) CounterMetric {
	c := &cumulativeCounterImp{
		Series: newSeries(ms, labelValues),
	}
	return c
}

func (c *cumulativeCounterImp) Add(delta int64) {
	atomic.AddInt64(&c.value, delta)
}

func (c *cumulativeCounterImp) Collect() MetricValue[float64] {
	value := atomic.LoadInt64(&c.value)
	return MetricValue[float64]{Name: c.Name(), Value: float64(value)}
}

func (c *cumulativeCounterImp) Clear() {
	atomic.StoreInt64(&c.value, 0)
}

func (c *cumulativeCounterImp) Serialize(log *protocol.Log) {
	metricValue := c.Collect()
	c.Series.SerializeWithStr(log, metricValue.Name, strconv.FormatFloat(metricValue.Value, 'f', 4, 64))
}

func (c *cumulativeCounterImp) Export() map[string]string {
	metricValue := c.Collect()
	return c.Series.Export(metricValue.Name, strconv.FormatFloat(metricValue.Value, 'f', 4, 64))
}

func (c *cumulativeCounterImp) Type() SelfMetricType {
	return CounterType
}

// delta is a counter metric that can be incremented or decremented.
// It gets the increased value in the last window.
type counterImp struct {
	value int64
	Series
}

func newDeltaCounter(ms MetricSet, labelValues []string) CounterMetric {
	c := &counterImp{
		Series: newSeries(ms, labelValues),
	}
	return c
}

func (c *counterImp) Add(delta int64) {
	atomic.AddInt64(&c.value, delta)
}

func (c *counterImp) Collect() MetricValue[float64] {
	value := atomic.SwapInt64(&c.value, 0)
	return MetricValue[float64]{Name: c.Name(), Value: float64(value)}
}

func (c *counterImp) Clear() {
	atomic.StoreInt64(&c.value, 0)
}

func (c *counterImp) Serialize(log *protocol.Log) {
	metricValue := c.Collect()
	c.Series.SerializeWithStr(log, metricValue.Name, strconv.FormatFloat(metricValue.Value, 'f', 4, 64))
}

func (c *counterImp) Export() map[string]string {
	metricValue := c.Collect()
	return c.Series.Export(metricValue.Name, strconv.FormatFloat(metricValue.Value, 'f', 4, 64))
}

func (c *counterImp) Type() SelfMetricType {
	return CounterType
}

// gauge is a metric that represents a single numerical value that can arbitrarily go up and down.
type gaugeImp struct {
	value float64
	Series
}

func newGauge(ms MetricSet, labelValues []string) GaugeMetric {
	g := &gaugeImp{
		Series: newSeries(ms, labelValues),
	}
	return g
}

func (g *gaugeImp) Set(f float64) {
	math.AtomicStoreFloat64(&g.value, f)
}

func (g *gaugeImp) Collect() MetricValue[float64] {
	return MetricValue[float64]{Name: g.Name(), Value: math.AtomicLoadFloat64(&g.value)}
}

func (g *gaugeImp) Clear() {
	math.AtomicStoreFloat64(&g.value, 0)
}

func (g *gaugeImp) Serialize(log *protocol.Log) {
	metricValue := g.Collect()
	g.Series.SerializeWithStr(log, metricValue.Name, strconv.FormatFloat(metricValue.Value, 'f', 4, 64))
}

func (g *gaugeImp) Export() map[string]string {
	metricValue := g.Collect()
	return g.Series.Export(metricValue.Name, strconv.FormatFloat(metricValue.Value, 'f', 4, 64))
}

func (g *gaugeImp) Type() SelfMetricType {
	return GaugeType
}

// averageImp is a metric to compute the average value of a series of values in the last window.
// if there is no value added in the last window, the previous average value will be returned.
type averageImp struct {
	sync.RWMutex
	value   int64
	count   int64
	prevAvg float64
	Series
}

func newAverage(ms MetricSet, labelValues []string) CounterMetric {
	a := &averageImp{
		Series: newSeries(ms, labelValues),
	}
	return a
}

func (a *averageImp) Add(f int64) {
	a.Lock()
	defer a.Unlock()
	a.value += f
	a.count++
}

func (a *averageImp) Collect() MetricValue[float64] {
	a.RLock()
	defer a.RUnlock()
	if a.count == 0 {
		return MetricValue[float64]{Name: a.Name(), Value: a.prevAvg}
	}
	avg := float64(a.value) / float64(a.count)
	a.prevAvg, a.value, a.count = avg, 0, 0
	return MetricValue[float64]{Name: a.Name(), Value: avg}
}

func (a *averageImp) Clear() {
	a.Lock()
	a.value = 0
	a.count = 0
	a.prevAvg = 0
	a.Unlock()
}

func (a *averageImp) Serialize(log *protocol.Log) {
	metricValue := a.Collect()
	a.Series.SerializeWithStr(log, metricValue.Name, strconv.FormatFloat(metricValue.Value, 'f', 4, 64))
}

func (a *averageImp) Export() map[string]string {
	metricValue := a.Collect()
	return a.Series.Export(metricValue.Name, strconv.FormatFloat(metricValue.Value, 'f', 4, 64))
}

func (a *averageImp) Type() SelfMetricType {
	return GaugeType
}

// maxImp is a metric to compute the max value of a series of values in the last window.
// if there is no value added in the last window, zero will be returned.
type maxImp struct {
	sync.RWMutex
	value float64
	Series
}

func newMax(ms MetricSet, labelValues []string) GaugeMetric {
	m := &maxImp{
		Series: newSeries(ms, labelValues),
	}
	return m
}

func (m *maxImp) Set(f float64) {
	m.Lock()
	defer m.Unlock()
	if f > m.value {
		m.value = f
	}
}

func (m *maxImp) Collect() MetricValue[float64] {
	m.RLock()
	defer m.RUnlock()
	metric := MetricValue[float64]{Name: m.Name(), Value: m.value}
	m.value = 0
	return metric
}

func (m *maxImp) Export() map[string]string {
	metricValue := m.Collect()
	return m.Series.Export(metricValue.Name, strconv.FormatFloat(metricValue.Value, 'f', 4, 64))
}

func (m *maxImp) Type() SelfMetricType {
	return GaugeType
}

// latencyImp is a metric to compute the average latency of a series of values in the last window.
type latencyImp struct {
	sync.Mutex
	count      int64
	latencySum float64
	Series
}

func newLatency(ms MetricSet, labelValues []string) LatencyMetric {
	l := &latencyImp{
		Series: newSeries(ms, labelValues),
	}
	return l
}

func (l *latencyImp) Observe(f float64) {
	l.Lock()
	defer l.Unlock()
	l.count++
	l.latencySum += f
}

func (l *latencyImp) Record(d time.Duration) {
	l.Observe(float64(d))
}

func (l *latencyImp) Collect() MetricValue[float64] {
	l.Lock()
	defer l.Unlock()
	if l.count == 0 {
		return MetricValue[float64]{Name: l.Name(), Value: 0}
	}
	avg := l.latencySum / float64(l.count)
	l.count, l.latencySum = 0, 0
	return MetricValue[float64]{Name: l.Name(), Value: avg}
}

func (l *latencyImp) Clear() {
	l.Lock()
	defer l.Unlock()
	l.count = 0
	l.latencySum = 0
}

func (l *latencyImp) Serialize(log *protocol.Log) {
	metricValue := l.Collect()
	l.Series.SerializeWithStr(log, metricValue.Name, strconv.FormatFloat(metricValue.Value/1000, 'f', 4, 64)) // ns to us
}

func (l *latencyImp) Export() map[string]string {
	metricValue := l.Collect()
	return l.Series.Export(metricValue.Name, strconv.FormatFloat(metricValue.Value/1000, 'f', 4, 64)) // ns to us
}

func (l *latencyImp) Type() SelfMetricType {
	return GaugeType
}

// strMetricImp is a metric that represents a single string value.
type strMetricImp struct {
	sync.RWMutex
	value string
	Series
}

func newStringMetric(ms MetricSet, labelValues []string) StringMetric {
	s := &strMetricImp{
		Series: newSeries(ms, labelValues),
	}
	return s
}

func (s *strMetricImp) Set(str string) {
	s.Lock()
	defer s.Unlock()
	s.value = str
}

func (s *strMetricImp) Collect() MetricValue[string] {
	s.RLock()
	defer s.RUnlock()
	return MetricValue[string]{Name: s.Name(), Value: s.value}
}

func (s *strMetricImp) Clear() {
	s.Lock()
	s.value = ""
	s.Unlock()
}

func (s *strMetricImp) Serialize(log *protocol.Log) {
	metricValue := s.Collect()
	s.Series.SerializeWithStr(log, metricValue.Name, metricValue.Value)
}

func (s *strMetricImp) Export() map[string]string {
	metricValue := s.Collect()
	return s.Series.Export(metricValue.Name, metricValue.Value)
}

func (s *strMetricImp) Type() SelfMetricType {
	return GaugeType
}

type Series struct {
	MetricSet
	labelValues []string
}

func newSeries(ms MetricSet, labelValues []string) Series {
	var indexToStore []string
	if labelValues != nil {
		indexToStore = make([]string, len(labelValues))
		copy(indexToStore, labelValues)
	}

	return Series{
		MetricSet:   ms,
		labelValues: indexToStore,
	}
}

func (s Series) SerializeWithStr(log *protocol.Log, metricName, metricValueStr string) {
	log.Contents = append(log.Contents,
		&protocol.Log_Content{Key: metricName, Value: metricValueStr},
		&protocol.Log_Content{Key: SelfMetricNameKey, Value: metricName})

	for _, v := range s.ConstLabels() {
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: v.Key, Value: v.Value})
	}

	labelNames := s.LabelKeys()
	for i, v := range s.labelValues {
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: labelNames[i], Value: v})
	}
}

func (s Series) Export(metricName, metricValue string) map[string]string {
	ret := make(map[string]string, len(s.ConstLabels())+len(s.labelValues)+2)
	ret[metricName] = metricValue
	ret[SelfMetricNameKey] = metricName

	for _, v := range s.ConstLabels() {
		ret[v.Key] = v.Value
	}

	for i, v := range s.labelValues {
		ret[s.LabelKeys()[i]] = v
	}

	return ret
}

/*
Following are the metrics returned when WithLabel encountered an error.
*/
type errorNumericMetric struct {
	err error
}

func (e *errorNumericMetric) Add(f int64) {
	// logger.Warning(context.Background(), "METRIC_WITH_LABEL_ALARM", "add", e.err)
}

func (e *errorNumericMetric) Set(f float64) {
	// logger.Warning(context.Background(), "METRIC_WITH_LABEL_ALARM", "set", e.err)
}

func (e *errorNumericMetric) Observe(f float64) {
	// logger.Warning(context.Background(), "METRIC_WITH_LABEL_ALARM", "observe", e.err)
}

func (e *errorNumericMetric) Serialize(log *protocol.Log) {}

func (e *errorNumericMetric) Export() map[string]string {
	return nil
}

func (e *errorNumericMetric) Type() SelfMetricType {
	return CounterType
}

func (e *errorNumericMetric) Collect() MetricValue[float64] {
	return MetricValue[float64]{Name: "", Value: 0}
}

func (e *errorNumericMetric) Clear() {}

func newErrorNumericMetric(err error) *errorNumericMetric {
	return &errorNumericMetric{err: err}
}

type errorStrMetric struct {
	errorNumericMetric
}

func (e errorStrMetric) Set(s string) {
	// logger.Warning(context.Background(), "METRIC_WITH_LABEL_ALARM", "set", e.err)
}

func (e errorStrMetric) Collect() MetricValue[string] {
	return MetricValue[string]{Name: "", Value: ""}
}

func newErrorStringMetric(err error) StringMetric {
	return &errorStrMetric{errorNumericMetric: errorNumericMetric{err: err}}
}

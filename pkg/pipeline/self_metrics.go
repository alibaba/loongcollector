package pipeline

import (
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type SelfMetricType int

const (
	_                     SelfMetricType = iota
	CounterType                          // counter in the last window.
	CumulativeCounterType                // cumulative counter.
	AverageType                          // average value in the last window.
	LatencyType                          // average latency in the last window.
	StringType                           // string value.
	GaugeType                            // gauge value in the last window.

	/*
		Following Type are not used and not implemented yet.
	*/
	RateType // qps in the last window.
	SummaryType
	HistogramType
)

type Label struct {
	Key   string
	Value string
}

type MetricValue[T string | float64] struct {
	Name  string // Value Name, e.g. "cpu_usage"
	Value T
}

// MetricSet is a Collector to bundle metrics of the same name that differ in  their label values.
// A MetricSet has three properties:
// 1. Metric Name.
// 2. Constant Labels: Labels has constant value, e.g., "hostname=localhost", "namespace=default"
// 3. Label Keys is label Keys that may have different values, e.g., "status_code=200", "status_code=404".
type MetricSet interface {
	Name() string
	Type() SelfMetricType
	ConstLabels() []Label
	LabelKeys() []string
}

// MetricCollector is the interface implemented by anything that can be used by iLogtail to collect metrics.
type MetricCollector interface {
	Collect() []Metric
}

// MetricVector is a Collector to bundle metrics of the same name that differ in  their label values.
// WithLabels will return a unique Metric that is bound to a set of label values.
// If the labels contain label names that are not in the MetricSet, the Metric will be invalid.
type MetricVector[T Metric] interface {
	WithLabels(labels ...Label) T
}

type Metric interface {
	Serialize(log *protocol.Log)
	Export() map[string]string // Export as a map[string]string, It will replace Serialize in the future.
}

// CounterMetric has three implementations:
// cumulativeCounter: a cumulative counter metric that represents a single monotonically increasing counter whose value can only increase or be reset to zero on restart.
// counter: the increased value in the last window.
// average: the cumulative average value.
type CounterMetric interface {
	Metric
	Add(int64)
	Collect() MetricValue[float64]
}

type GaugeMetric interface {
	Metric
	Set(float64)
	Collect() MetricValue[float64]
}

type LatencyMetric interface {
	Metric
	Observe(float64)
	Collect() MetricValue[float64]
}

// SummaryMetric is used to compute pctXX for a batch of data
type SummaryMetric interface {
	Metric
	Observe(float64)
	Get() []MetricValue[float64]
}

// HistogramMetric is used to compute distribution of a batch of data.
type HistogramMetric interface {
	Metric
	Observe(float64)
	Get() []MetricValue[float64]
}

type StringMetric interface {
	Metric
	Set(v string)
	Collect() MetricValue[string]
}

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
	"log"
	"sort"
	"sync"
	"sync/atomic"
	"time"
	"unsafe"

	"github.com/alibaba/ilogtail/pkg/helper/pool"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const (
	defaultTagValue = "-"
	minExpiration   = time.Minute * 5
)

var (
	DefaultCacheFactory = NewMapCache
)

// SetMetricVectorCacheFactory allows users to set the cache factory for the metric vector, like Prometheus SDK.
func SetMetricVectorCacheFactory(factory func(MetricSet) MetricVectorCache) {
	DefaultCacheFactory = factory
}

type (
	CumulativeCounterMetricVector   = MetricVector[CounterMetric]
	AverageMetricVector             = MetricVector[CounterMetric]
	MaxMetricVector                 = MetricVector[GaugeMetric]
	CounterMetricVector             = MetricVector[CounterMetric]
	GaugeMetricVector               = MetricVector[GaugeMetric]
	LatencyMetricVector             = MetricVector[LatencyMetric]
	StringMetricVector              = MetricVector[StringMetric]
	HistogramMetricVector           = MetricVector[HistogramMetric]
	CumulativeHistogramMetricVector = MetricVector[HistogramMetric]
)

// NewCumulativeCounterMetricVector creates a new CounterMetricVector.
// Note that MetricVector doesn't expose Collect API by default. Plugins Developers should be careful to collect metrics manually.
func NewCumulativeCounterMetricVector(metricName string, constLabels map[string]string, labelNames []string, opts ...MetricOption) CumulativeCounterMetricVector {
	return NewMetricVector[CounterMetric](metricName, CumulativeCounterType, constLabels, labelNames, opts...)
}

// NewCounterMetricVector creates a new DeltaMetricVector.
// Note that MetricVector doesn't expose Collect API by default. Plugins Developers should be careful to collect metrics manually.
func NewCounterMetricVector(metricName string, constLabels map[string]string, labelNames []string, opts ...MetricOption) CounterMetricVector {
	return NewMetricVector[CounterMetric](metricName, CounterType, constLabels, labelNames, opts...)
}

// NewAverageMetricVector creates a new AverageMetricVector.
// Note that MetricVector doesn't expose Collect API by default. Plugins Developers should be careful to collect metrics manually.
func NewAverageMetricVector(metricName string, constLabels map[string]string, labelNames []string, opts ...MetricOption) AverageMetricVector {
	return NewMetricVector[CounterMetric](metricName, AverageType, constLabels, labelNames, opts...)
}

// NewMaxMetricVector creates a new MaxMetricVector.
// Note that MetricVector doesn't expose Collect API by default. Plugins Developers should be careful to collect metrics manually.
func NewMaxMetricVector(metricName string, constLabels map[string]string, labelNames []string, opts ...MetricOption) MaxMetricVector {
	return NewMetricVector[GaugeMetric](metricName, MaxType, constLabels, labelNames, opts...)
}

// NewGaugeMetricVector creates a new GaugeMetricVector.
// Note that MetricVector doesn't expose Collect API by default. Plugins Developers should be careful to collect metrics manually.
func NewGaugeMetricVector(metricName string, constLabels map[string]string, labelNames []string, opts ...MetricOption) GaugeMetricVector {
	return NewMetricVector[GaugeMetric](metricName, GaugeType, constLabels, labelNames, opts...)
}

// NewStringMetricVector creates a new StringMetricVector.
// Note that MetricVector doesn't expose Collect API by default. Plugins Developers should be careful to collect metrics manually.
func NewStringMetricVector(metricName string, constLabels map[string]string, labelNames []string, opts ...MetricOption) StringMetricVector {
	return NewMetricVector[StringMetric](metricName, StringType, constLabels, labelNames, opts...)
}

// NewLatencyMetricVector creates a new LatencyMetricVector.
// Note that MetricVector doesn't expose Collect API by default. Plugins Developers should be careful to collect metrics manually.
func NewLatencyMetricVector(metricName string, constLabels map[string]string, labelNames []string, opts ...MetricOption) LatencyMetricVector {
	return NewMetricVector[LatencyMetric](metricName, LatencyType, constLabels, labelNames, opts...)
}

// NewHistogramMetricVector creates a new HistogramMetricVector.
// Note that MetricVector doesn't expose Collect API by default. Plugins Developers should be careful to collect metrics manually.
func NewHistogramMetricVector(metricName string, constLabels map[string]string, labelNames []string, opts ...MetricOption) HistogramMetricVector {
	return NewMetricVector[HistogramMetric](metricName, HistogramType, constLabels, labelNames, opts...)
}

// NewCumulativeHistogramMetricVector creates a new CumulativeHistogramMetricVector.
// Note that MetricVector doesn't expose Collect API by default. Plugins Developers should be careful to collect metrics manually.
func NewCumulativeHistogramMetricVector(metricName string, constLabels map[string]string, labelNames []string, opts ...MetricOption) CumulativeHistogramMetricVector {
	return NewMetricVector[HistogramMetric](metricName, CumulativeHistogramType, constLabels, labelNames, opts...)
}

// NewCumulativeCounterMetricVectorAndRegister creates a new CounterMetricVector and register it to the MetricsRecord.
func NewCumulativeCounterMetricVectorAndRegister(mr *MetricsRecord, metricName string, constLabels map[string]string, labelNames []string, opts ...MetricOption) CumulativeCounterMetricVector {
	v := NewMetricVector[CounterMetric](metricName, CumulativeCounterType, constLabels, labelNames, opts...)
	mr.RegisterMetricCollector(v)
	return v
}

// NewAverageMetricVectorAndRegister creates a new AverageMetricVector and register it to the MetricsRecord.
func NewAverageMetricVectorAndRegister(mr *MetricsRecord, metricName string, constLabels map[string]string, labelNames []string, opts ...MetricOption) AverageMetricVector {
	v := NewMetricVector[CounterMetric](metricName, AverageType, constLabels, labelNames, opts...)
	mr.RegisterMetricCollector(v)
	return v
}

// NewCounterMetricVectorAndRegister creates a new DeltaMetricVector and register it to the MetricsRecord.
func NewCounterMetricVectorAndRegister(mr *MetricsRecord, metricName string, constLabels map[string]string, labelNames []string, opts ...MetricOption) CounterMetricVector {
	v := NewMetricVector[CounterMetric](metricName, CounterType, constLabels, labelNames, opts...)
	mr.RegisterMetricCollector(v)
	return v
}

// NewGaugeMetricVectorAndRegister creates a new GaugeMetricVector and register it to the MetricsRecord.
func NewGaugeMetricVectorAndRegister(mr *MetricsRecord, metricName string, constLabels map[string]string, labelNames []string, opts ...MetricOption) GaugeMetricVector {
	v := NewMetricVector[GaugeMetric](metricName, GaugeType, constLabels, labelNames, opts...)
	mr.RegisterMetricCollector(v)
	return v
}

// NewLatencyMetricVectorAndRegister creates a new LatencyMetricVector and register it to the MetricsRecord.
func NewLatencyMetricVectorAndRegister(mr *MetricsRecord, metricName string, constLabels map[string]string, labelNames []string, opts ...MetricOption) LatencyMetricVector {
	v := NewMetricVector[LatencyMetric](metricName, LatencyType, constLabels, labelNames, opts...)
	mr.RegisterMetricCollector(v)
	return v
}

// NewStringMetricVectorAndRegister creates a new StringMetricVector and register it to the MetricsRecord.
func NewStringMetricVectorAndRegister(mr *MetricsRecord, metricName string, constLabels map[string]string, labelNames []string, opts ...MetricOption) StringMetricVector {
	v := NewMetricVector[StringMetric](metricName, StringType, constLabels, labelNames, opts...)
	mr.RegisterMetricCollector(v)
	return v
}

// NewCumulativeCounterMetric creates a new CounterMetric.
func NewCumulativeCounterMetric(n string, lables ...*protocol.Log_Content) CounterMetric {
	return NewCumulativeCounterMetricVector(n, convertLabels(lables), nil).WithLabels()
}

// NewAverageMetric creates a new AverageMetric.
func NewAverageMetric(n string, lables ...*protocol.Log_Content) CounterMetric {
	return NewAverageMetricVector(n, convertLabels(lables), nil).WithLabels()
}

// NewCounterMetric creates a new DeltaMetric.
func NewCounterMetric(n string, lables ...*protocol.Log_Content) CounterMetric {
	return NewCounterMetricVector(n, convertLabels(lables), nil).WithLabels()
}

// NewGaugeMetric creates a new GaugeMetric.
func NewGaugeMetric(n string, lables ...*protocol.Log_Content) GaugeMetric {
	return NewGaugeMetricVector(n, convertLabels(lables), nil).WithLabels()
}

// NewStringMetric creates a new StringMetric.
func NewStringMetric(n string, lables ...*protocol.Log_Content) StringMetric {
	return NewStringMetricVector(n, convertLabels(lables), nil).WithLabels()
}

// NewLatencyMetric creates a new LatencyMetric.
func NewLatencyMetric(n string, lables ...*protocol.Log_Content) LatencyMetric {
	return NewLatencyMetricVector(n, convertLabels(lables), nil).WithLabels()
}

// NewCumulativeCounterMetricAndRegister creates a new CounterMetric and register it's metricVector to the MetricsRecord.
func NewCumulativeCounterMetricAndRegister(c *MetricsRecord, n string, lables ...*protocol.Log_Content) CounterMetric {
	mv := NewCumulativeCounterMetricVector(n, convertLabels(lables), nil)
	c.RegisterMetricCollector(mv.(MetricCollector))
	return mv.WithLabels()
}

// NewCounterMetricAndRegister creates a new DeltaMetric and register it's metricVector to the MetricsRecord.
func NewCounterMetricAndRegister(c *MetricsRecord, n string, lables ...*protocol.Log_Content) CounterMetric {
	mv := NewCounterMetricVector(n, convertLabels(lables), nil)
	c.RegisterMetricCollector(mv.(MetricCollector))
	return mv.WithLabels()
}

// NewAverageMetricAndRegister creates a new AverageMetric and register it's metricVector to the MetricsRecord.
func NewAverageMetricAndRegister(c *MetricsRecord, n string, lables ...*protocol.Log_Content) CounterMetric {
	mv := NewAverageMetricVector(n, convertLabels(lables), nil)
	c.RegisterMetricCollector(mv.(MetricCollector))
	return mv.WithLabels()
}

// NewMaxMetricAndRegister creates a new MaxMetric and register it's metricVector to the MetricsRecord.
func NewMaxMetricAndRegister(c *MetricsRecord, n string, lables ...*protocol.Log_Content) GaugeMetric {
	mv := NewMaxMetricVector(n, convertLabels(lables), nil)
	c.RegisterMetricCollector(mv.(MetricCollector))
	return mv.WithLabels()
}

// NewGaugeMetricAndRegister creates a new GaugeMetric and register it's metricVector to the MetricsRecord.
func NewGaugeMetricAndRegister(c *MetricsRecord, n string, lables ...*protocol.Log_Content) GaugeMetric {
	mv := NewGaugeMetricVector(n, convertLabels(lables), nil)
	c.RegisterMetricCollector(mv.(MetricCollector))
	return mv.WithLabels()
}

// NewLatencyMetricAndRegister creates a new LatencyMetric and register it's metricVector to the MetricsRecord.
func NewLatencyMetricAndRegister(c *MetricsRecord, n string, lables ...*protocol.Log_Content) LatencyMetric {
	mv := NewLatencyMetricVector(n, convertLabels(lables), nil)
	c.RegisterMetricCollector(mv.(MetricCollector))
	return mv.WithLabels()
}

// NewStringMetricAndRegister creates a new StringMetric and register it's metricVector to the MetricsRecord.
func NewStringMetricAndRegister(c *MetricsRecord, n string, lables ...*protocol.Log_Content) StringMetric {
	mv := NewStringMetricVector(n, convertLabels(lables), nil)
	c.RegisterMetricCollector(mv.(MetricCollector))
	return mv.WithLabels()
}

var (
	_ MetricCollector             = (*MetricVectorImpl[CounterMetric])(nil)
	_ MetricSet                   = (*MetricVectorImpl[StringMetric])(nil)
	_ MetricVector[CounterMetric] = (*MetricVectorImpl[CounterMetric])(nil)
	_ MetricVector[GaugeMetric]   = (*MetricVectorImpl[GaugeMetric])(nil)
	_ MetricVector[LatencyMetric] = (*MetricVectorImpl[LatencyMetric])(nil)
	_ MetricVector[StringMetric]  = (*MetricVectorImpl[StringMetric])(nil)
)

type MetricVectorAndCollector[T Metric] interface {
	MetricVector[T]
	MetricCollector
}

type metricOption struct {
	name             string // metric name
	desc             string
	unit             string
	metricType       SelfMetricType
	constLabels      []LabelPair // constLabels is the labels that are not changed when the metric is created.
	labelKeys        []string    // labelNames is the names of the labels. The values of the labels can be changed.
	isDynamicLabel   bool
	metricExpiration time.Duration
	isCumulative     bool
	bucketBoundaries []float64
	cardinalityLimit int // 0 is unlimited
	overflowKey      string
	enableGC         bool
}

func (o metricOption) Desc() string {
	return o.desc
}

func (o metricOption) Unit() string {
	return o.unit
}

func (o metricOption) Name() string {
	return o.name
}

func (o metricOption) Type() SelfMetricType {
	return o.metricType
}

func (o metricOption) ConstLabels() []LabelPair {
	return o.constLabels
}

func (o metricOption) LabelKeys() []string {
	return o.labelKeys
}

func (o metricOption) IsLabelDynamic() bool {
	return o.isDynamicLabel
}

func (o metricOption) Expiration() time.Duration {
	return o.metricExpiration
}

func (o metricOption) IsCumulative() bool {
	return o.isCumulative
}

func (o metricOption) BucketBoundaries() []float64 {
	return o.bucketBoundaries
}

func (o metricOption) CardinalityLimit() int {
	return o.cardinalityLimit
}

func (o metricOption) OverflowKey() string {
	return o.overflowKey
}

func (o metricOption) EnableGC() bool {
	return o.enableGC
}

func defaultMetricOption() metricOption {
	return metricOption{
		overflowKey: "overflow",
		enableGC:    false,
	}
}

type MetricOption func(option metricOption) metricOption

func WithDynamicLabel() MetricOption {
	return func(option metricOption) metricOption {
		option.isDynamicLabel = true
		return option
	}
}

func WithExpiration(expiration time.Duration) MetricOption {
	return func(option metricOption) metricOption {
		option.metricExpiration = expiration
		return option
	}
}

func WithDesc(desc string) MetricOption {
	return func(option metricOption) metricOption {
		option.desc = desc
		return option
	}
}

func WithUnit(unit string) MetricOption {
	return func(option metricOption) metricOption {
		option.unit = unit
		return option
	}
}

func WithCumulative() MetricOption {
	return func(option metricOption) metricOption {
		option.isCumulative = true
		return option
	}
}

func WithBucketBoundaries(boundaries []float64) MetricOption {
	return func(option metricOption) metricOption {
		option.bucketBoundaries = boundaries
		return option
	}
}

func WithCardinalityLimit(limit int) MetricOption {
	return func(option metricOption) metricOption {
		if limit <= 0 {
			return option
		}
		option.cardinalityLimit = limit
		return option
	}
}

func WithOverflowKey(key string) MetricOption {
	return func(option metricOption) metricOption {
		option.overflowKey = key
		return option
	}
}

func WithGC(enable bool) MetricOption {
	return func(option metricOption) metricOption {
		option.enableGC = enable
		return option
	}
}

type MetricVectorImpl[T Metric] struct {
	*metricVector
}

// NewMetricVector creates a new MetricVector.
// It returns a MetricVectorAndCollector, which is a MetricVector and a MetricCollector.
// For plugin developers, they should use MetricVector APIs to create metrics.
// For agent itself, it uses MetricCollector APIs to collect metrics.
func NewMetricVector[T Metric](metricName string, metricType SelfMetricType, constLabels map[string]string, labelNames []string, opts ...MetricOption) MetricVectorAndCollector[T] {
	options := defaultMetricOption()
	options.name = metricName
	options.metricType = metricType
	options.labelKeys = labelNames
	for k, v := range constLabels {
		options.constLabels = append(options.constLabels, LabelPair{Key: k, Value: v})
	}

	for _, opt := range opts {
		options = opt(options)
	}

	return &MetricVectorImpl[T]{
		metricVector: newMetricVector(options),
	}
}

func (m *MetricVectorImpl[T]) WithLabels(labels ...LabelPair) T {
	return m.metricVector.WithLabels(labels...).(T)
}

func (m *MetricVectorImpl[T]) Start() error {
	return m.metricVector.Start()
}

func (m *MetricVectorImpl[T]) Close() error {
	return m.metricVector.Close()
}

var (
	_ MetricSet = (*metricVector)(nil)
)

type metricVector struct {
	metricOption

	indexPool pool.GenericPool[string] // index is []string, which is sorted according to labelNames.
	labelPool pool.GenericPool[LabelPair]
	cache     MetricVectorCache // collector is a map[string]Metric, key is the index of the metric.
	close     chan struct{}
	once      sync.Once
}

// MetricVectorCache is a cache for MetricVector.
type MetricVectorCache interface {
	// return a metric with the given label values.
	// Note that the label values are sorted according to the label keys in MetricSet.
	WithLabelValues([]string) Metric

	MetricCollector
}

func newMetricVector(option metricOption) *metricVector {
	mv := &metricVector{
		metricOption: option,
		indexPool:    pool.NewGenericPool(func() []string { return make([]string, 0, 10) }),
		labelPool:    pool.NewGenericPool(func() []LabelPair { return make([]LabelPair, 0, 10) }),
		close:        make(chan struct{}),
	}
	mv.cache = DefaultCacheFactory(mv)

	if mv.metricExpiration > 0 && option.EnableGC() {
		if err := mv.Start(); err != nil {
			log.Printf("start metric vector %v, failed: %v", option, err)
		}
	}
	return mv
}

func (v *metricVector) LabelKeys() []string {
	if v.isDynamicLabel {
		return nil
	}
	return v.metricOption.LabelKeys()
}

func (v *metricVector) WithLabels(labels ...LabelPair) Metric {
	labelValues, err := v.buildLabelValues(labels)
	if err != nil {
		return newErrorMetric(v.metricType, err)
	}
	defer v.indexPool.Put(labelValues)
	return v.cache.WithLabelValues(*labelValues)
}

func (v *metricVector) Start() error {
	if v.metricExpiration <= 0 {
		return fmt.Errorf("metric vector %v is not started gc since expiration is %v", v.metricOption, v.metricExpiration)
	}

	go v.gc(v.metricExpiration)
	return nil
}

func (v *metricVector) Collect() []Metric {
	return v.cache.Collect()
}

func (v *metricVector) Close() error {
	v.once.Do(func() {
		close(v.close)
	})
	return nil
}

func (v *metricVector) gc(metricExpiration time.Duration) {
	gcInterval := time.Duration(float64(metricExpiration) * 0.25)
	for {
		select {
		case <-v.close:
			return
		case <-time.After(gcInterval):
			if gcer, ok := v.cache.(GCer); ok {
				gcer.GC(metricExpiration)
			}
		}
	}
}

type GCer interface {
	GC(expiration time.Duration)
}

// buildLabelValues return the index
func (v *metricVector) buildLabelValues(labels []LabelPair) (*[]string, error) {
	if !v.isDynamicLabel && len(labels) > len(v.labelKeys) {
		return nil, fmt.Errorf("too many labels, expected %d, got %d. defined labels: %v",
			len(v.labelKeys), len(labels), v.labelKeys)
	}

	index := v.indexPool.Get()
	if !v.isDynamicLabel {
		for range v.labelKeys {
			*index = append(*index, defaultTagValue)
		}

		for d, tag := range labels {
			if v.labelKeys[d] == tag.Key { // fast path
				(*index)[d] = tag.Value
			} else {
				err := v.slowConstructIndex(index, tag)
				if err != nil {
					v.indexPool.Put(index)
					return nil, err
				}
			}
		}
	} else {
		tmpLabels := v.labelPool.Get()
		*tmpLabels = append(*tmpLabels, labels...)
		sort.Slice(*tmpLabels, func(i, j int) bool {
			return (*tmpLabels)[i].Key < (*tmpLabels)[j].Key
		})
		for _, tag := range *tmpLabels {
			value := tag.Value
			if value == "" {
				value = defaultTagValue
			}
			*index = append(*index, tag.Key, value)
		}
	}
	return index, nil
}

func (v *metricVector) slowConstructIndex(index *[]string, tag LabelPair) error {
	for i, tagName := range v.labelKeys {
		if tagName == tag.Key {
			(*index)[i] = tag.Value
			return nil
		}
	}
	return fmt.Errorf("undefined label: %s in %v", tag.Key, v.labelKeys)
}

var (
	_ GCer = (*MapCache)(nil)
)

type MapCache struct {
	currCardinality  int64
	overflowInitOnce sync.Once
	overflowMetric   Metric

	MetricSet
	bytesPool pool.GenericPool[byte]
	sync.Map
}

func NewMapCache(metricSet MetricSet) MetricVectorCache {
	return &MapCache{
		MetricSet: metricSet,
		bytesPool: pool.NewGenericPool(func() []byte { return make([]byte, 0, 128) }),
	}
}

func (v *MapCache) WithLabelValues(labelValues []string) Metric {
	buffer := v.bytesPool.Get()
	for _, tagValue := range labelValues {
		*buffer = append(*buffer, '|')
		*buffer = append(*buffer, tagValue...)
	}

	/* #nosec G103 */
	k := *(*string)(unsafe.Pointer(buffer))
	acV, loaded := v.Load(k)
	if loaded {
		metric := acV.(Metric)
		v.bytesPool.Put(buffer)
		return metric
	}

	if v.IsCardinalityLimitReached() {
		v.bytesPool.Put(buffer)

		v.overflowInitOnce.Do(func() {
			v.overflowMetric = newMetric(v.Type(), v, v.GetOverflowMetricLabels(), v.IsCumulative(), v.BucketBoundaries())
		})
		return v.overflowMetric
	}

	atomic.AddInt64(&v.currCardinality, 1)
	newMetric := newMetric(v.Type(), v, labelValues, v.IsCumulative(), v.BucketBoundaries())
	acV, loaded = v.LoadOrStore(k, newMetric)
	if loaded {
		v.bytesPool.Put(buffer)
	}
	return acV.(Metric)
}

func (v *MapCache) GetOverflowMetricLabels() []string {
	if v.IsLabelDynamic() {
		return []string{v.OverflowKey(), "true"}
	}
	index := make([]string, len(v.LabelKeys()))
	for i := range v.LabelKeys() {
		index[i] = v.OverflowKey()
	}
	return index
}

func (v *MapCache) IsCardinalityLimitReached() bool {
	return v.CardinalityLimit() > 0 && atomic.LoadInt64(&v.currCardinality) >= int64(v.CardinalityLimit())
}

func (v *MapCache) Collect() []Metric {
	res := make([]Metric, 0, 10)
	v.Range(func(key, value interface{}) bool {
		res = append(res, value.(Metric))
		return true
	})

	if v.overflowMetric != nil {
		res = append(res, v.overflowMetric)
	}
	return res
}

type Expirable interface {
	GetLastActiveTime() time.Time
}

func (v *MapCache) GC(expiration time.Duration) {
	now := time.Now()
	v.Range(func(key, value interface{}) bool {
		if metric, ok := value.(Expirable); ok {
			if metric.GetLastActiveTime().Add(expiration).Before(now) {
				v.Delete(key)
				atomic.AddInt64(&v.currCardinality, -1)
			}
		}
		return true
	})
}

func convertLabels(labels []*protocol.Log_Content) map[string]string {
	if len(labels) == 0 {
		return nil
	}

	l := make(map[string]string)
	for _, label := range labels {
		l[label.Key] = label.Value
	}

	return l
}

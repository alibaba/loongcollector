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

package pipeline

import (
	"context"
	"encoding/json"
	"sync"

	"github.com/alibaba/ilogtail/pkg/config"
)

type CommonContext struct {
	Project    string
	Logstore   string
	ConfigName string
}

type LabelPair = Label

const SelfMetricNameKey = "__name__"
const MetricLabelPrefix = "labels"
const MetricCounterPrefix = "counters"
const MetricGaugePrefix = "gauges"

type MetricsRecord struct {
	Context Context
	Labels  []LabelPair

	sync.RWMutex
	MetricCollectors []MetricCollector
}

func (m *MetricsRecord) insertLabels(record map[string]string) {
	labels := map[string]string{}
	for _, label := range m.Labels {
		labels[label.Key] = label.Value
	}
	labelsStr, _ := json.Marshal(labels)
	record[MetricLabelPrefix] = string(labelsStr)
}

func (m *MetricsRecord) RegisterMetricCollector(collector MetricCollector) {
	m.Lock()
	defer m.Unlock()
	m.MetricCollectors = append(m.MetricCollectors, collector)
}

// ExportMetricRecords is used for exporting metrics records.
// It will replace Serialize in the future.
func (m *MetricsRecord) ExportMetricRecords() map[string]string {
	m.RLock()
	defer m.RUnlock()

	record := map[string]string{}
	m.insertLabels(record)
	for _, metricCollector := range m.MetricCollectors {
		metrics := metricCollector.Collect()
		counters := map[string]string{}
		gauges := map[string]string{}
		for _, metric := range metrics {
			singleMetric := metric.Export()
			if len(singleMetric) == 0 {
				continue
			}
			valueName := singleMetric[SelfMetricNameKey]
			valueValue := singleMetric[valueName]
			if metric.Type() == CounterType {
				counters[valueName] = valueValue
			}
			if metric.Type() == GaugeType {
				gauges[valueName] = valueValue
			}
		}
		countersStr, _ := json.Marshal(counters)
		record[MetricCounterPrefix] = string(countersStr)
		gaugesStr, _ := json.Marshal(gauges)
		record[MetricGaugePrefix] = string(gaugesStr)
	}
	return record
}

// Context for plugin
type Context interface {
	InitContext(project, logstore, configName string)

	GetConfigName() string
	GetProject() string
	GetLogstore() string
	GetRuntimeContext() context.Context
	GetPipelineScopeConfig() *config.GlobalConfig
	GetExtension(name string, cfg any) (Extension, error)
	// RegisterCounterMetric(metric CounterMetric)
	// RegisterStringMetric(metric StringMetric)
	// RegisterLatencyMetric(metric LatencyMetric)

	SaveCheckPoint(key string, value []byte) error
	GetCheckPoint(key string) (value []byte, exist bool)
	SaveCheckPointObject(key string, obj interface{}) error
	GetCheckPointObject(key string, obj interface{}) (exist bool)

	// APIs for self monitor
	RegisterMetricRecord(labels []LabelPair) *MetricsRecord
	GetMetricRecord() *MetricsRecord
	ExportMetricRecords() []map[string]string
	RegisterLogstoreConfigMetricRecord(labels []LabelPair) *MetricsRecord
	GetLogstoreConfigMetricRecord() *MetricsRecord
}

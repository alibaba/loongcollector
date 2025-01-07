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
package pluginmanager

import (
	goruntimemetrics "runtime/metrics"
	"strconv"
	"strings"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
)

const (
	MetricExportTypeGo  = "direct"
	MetricExportTypeCpp = "cpp_provided"
)

func GetMetrics(metricType string) []map[string]string {
	if metricType == MetricExportTypeGo {
		return GetGoDirectMetrics()
	}
	if metricType == MetricExportTypeCpp {
		return GetGoCppProvidedMetrics()
	}
	return []map[string]string{}
}

// 直接输出的go指标，例如go插件指标
//
//	[]map[string]string{
//		{
//			"labels": "{\"category\": \"plugin\",\"plugin_type\":\"flusher_stdout\"}",
//			"counters": "{\"proc_in_records_total\": \"100\"}"
//			"gauges": "{}"
//		},
//		{
//			"labels": "{\"category\": \"runner\",\"runner_name\":\"k8s_meta\"}",
//			"counters": "{\"proc_in_records_total\": \"100\"}",
//			"gauges": "{\"cache_size\": \"100\"}"
//		}
//		},
//	}
func GetGoDirectMetrics() []map[string]string {
	metrics := make([]map[string]string, 0)
	// go plugin metrics
	metrics = append(metrics, GetGoPluginMetrics()...)
	// k8s meta metrics
	metrics = append(metrics, k8smeta.GetMetaManagerMetrics()...)
	return metrics
}

// 由C++定义的指标，go把值传过去，例如go的进程级指标
//
//	[]map[string]string{
//		{
//			"go_memory_used_mb": "100",
//			"go_routines_total": "20"
//		}
//	}
func GetGoCppProvidedMetrics() []map[string]string {
	metrics := make([]map[string]string, 0)
	// agent-level metrics
	metrics = append(metrics, GetAgentStat()...)
	return metrics
}

// go 插件指标，直接输出
func GetGoPluginMetrics() []map[string]string {
	metrics := make([]map[string]string, 0)
	LogtailConfigLock.RLock()
	for _, config := range LogtailConfig {
		metrics = append(metrics, config.Context.ExportMetricRecords()...)
	}
	LogtailConfigLock.RUnlock()
	return metrics
}

// go 进程级指标，由C++部分注册
func GetAgentStat() []map[string]string {
	metrics := []map[string]string{}
	metric := map[string]string{}
	// key is the metric key in runtime/metrics, value is agent's metric key
	metricNames := map[string]string{
		// mem. All memory mapped by the Go runtime into the current process as read-write. Note that this does not include memory mapped by code called via cgo or via the syscall package. Sum of all metrics in /memory/classes.
		"/memory/classes/total:bytes": helper.MetricAgentMemoryGo,
		// go routines cnt. Count of live goroutines.
		"/sched/goroutines:goroutines": helper.MetricAgentGoRoutinesTotal,
	}

	// metrics to read from runtime/metrics
	samples := make([]goruntimemetrics.Sample, 0)
	for name := range metricNames {
		samples = append(samples, goruntimemetrics.Sample{Name: name})
	}
	goruntimemetrics.Read(samples)

	// push results to recrods
	for _, sample := range samples {
		key := metricNames[sample.Name]
		value := sample.Value
		valueStr := ""
		switch value.Kind() {
		case goruntimemetrics.KindUint64:
			if strings.HasSuffix(key, "_mb") {
				valueStr = strconv.FormatUint(value.Uint64()/1024/1024, 10)
			} else {
				valueStr = strconv.FormatUint(value.Uint64(), 10)
			}
		case goruntimemetrics.KindFloat64:
			valueStr = strconv.FormatFloat(value.Float64(), 'g', -1, 64)
		}
		metric[key] = valueStr
	}

	metrics = append(metrics, metric)
	return metrics
}

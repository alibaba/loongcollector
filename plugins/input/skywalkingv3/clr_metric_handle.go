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

package skywalkingv3

import (
	"context"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	common "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	agent "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/agent/v3"
)

type CLRMetricHandler struct {
	context   pipeline.Context
	collector pipeline.Collector
	interval  int64
	lastTime  int64
}

func (c *CLRMetricHandler) Collect(ctx context.Context, req *agent.CLRMetricCollection) (*common.Commands, error) {
	defer panicRecover()
	for _, metric := range req.GetMetrics() {
		fixTime := metric.Time - metric.Time%1000
		// 原始数据上传过于频繁, 不需要
		if fixTime-c.lastTime >= c.interval {
			c.lastTime = fixTime
		} else {
			return &common.Commands{}, nil
		}
		logs := c.toMetricStoreFormat(metric, req.Service, req.ServiceInstance)
		for _, log := range logs {
			c.collector.AddRawLog(log)
		}
	}
	return &common.Commands{}, nil
}

func (c *CLRMetricHandler) toMetricStoreFormat(metric *agent.CLRMetric, service string, serviceInstance string) []*protocol.Log {
	var logs []*protocol.Log
	var labels helper.MetricLabels
	labels.Append("service", service)
	labels.Append("serviceInstance", serviceInstance)
	cpuUsage := helper.NewMetricLog("skywalking_clr_cpu_usage", metric.Time, metric.GetCpu().UsagePercent, &labels)
	logs = append(logs, cpuUsage)

	gc := metric.Gc
	gcLabels := labels.Clone()
	gcLabels.Append("gen", "gen0")
	gen0GcCount := helper.NewMetricLog("skywalking_clr_gc_count", metric.GetTime(), float64(gc.Gen0CollectCount), gcLabels)
	logs = append(logs, gen0GcCount)

	gcLabels.Replace("gen", "gen1")
	gen1GcCount := helper.NewMetricLog("skywalking_clr_gc_count", metric.GetTime(), float64(gc.Gen1CollectCount), gcLabels)
	logs = append(logs, gen1GcCount)

	gcLabels.Replace("gen", "gen2")
	gen2GcCount := helper.NewMetricLog("skywalking_clr_gc_count", metric.GetTime(), float64(gc.Gen2CollectCount), gcLabels)
	logs = append(logs, gen2GcCount)

	heapMemory := helper.NewMetricLog("skywalking_clr_heap_memory", metric.GetTime(), float64(gc.HeapMemory), &labels)
	logs = append(logs, heapMemory)

	thread := metric.Thread
	threadLabels := labels.CloneInto(gcLabels)
	threadLabels.Append("type", "availableCompletionPortThreads")
	availableCompletionPortThreads := helper.NewMetricLog("skywalking_clr_threads", metric.GetTime(), float64(thread.AvailableCompletionPortThreads), threadLabels)
	logs = append(logs, availableCompletionPortThreads)

	threadLabels.Replace("type", "availableWorkerThreads")
	availableWorkerThreads := helper.NewMetricLog("skywalking_clr_threads", metric.GetTime(), float64(thread.AvailableWorkerThreads), threadLabels)
	logs = append(logs, availableWorkerThreads)

	threadLabels.Replace("type", "maxCompletionPortThreads")
	maxCompletionPortThreads := helper.NewMetricLog("skywalking_clr_threads", metric.GetTime(), float64(thread.MaxCompletionPortThreads), threadLabels)
	logs = append(logs, maxCompletionPortThreads)

	threadLabels.Replace("type", "maxWorkerThreads")
	maxWorkerThreads := helper.NewMetricLog("skywalking_clr_threads", metric.GetTime(), float64(thread.MaxWorkerThreads), threadLabels)
	logs = append(logs, maxWorkerThreads)
	return logs
}

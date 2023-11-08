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
	v3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	skywalking "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/agent/v3"
)

type JVMMetricHandler struct {
	context   pipeline.Context
	collector pipeline.Collector
	interval  int64
	lastTime  int64
}

func (h *JVMMetricHandler) Collect(ctx context.Context, req *skywalking.JVMMetricCollection) (*v3.Commands, error) {
	defer panicRecover()
	for _, metric := range req.GetMetrics() {
		// 在充当Collector角色（一对多个实例）时，将会出现问题，因为每个实例的时间不一致，所以这里不做时间过滤
		// fixTime := metric.Time - metric.Time%1000
		// // 原始数据上传过于频繁, 不需要
		// if fixTime-h.lastTime >= h.interval {
		// 	h.lastTime = fixTime
		// } else {
		// 	logger.Warning(h.context.GetRuntimeContext(), "", "lastTime", h.lastTime, "fixTime", fixTime, "interval", h.interval)
		// 	return &v3.Commands{}, nil
		// }
		logs := h.toMetricStoreFormat(metric, req.Service, req.ServiceInstance)
		for _, log := range logs {
			h.collector.AddRawLog(log)
		}
	}
	return &v3.Commands{}, nil
}

func (h *JVMMetricHandler) toMetricStoreFormat(metric *skywalking.JVMMetric, service string, serviceInstance string) []*protocol.Log {
	var logs []*protocol.Log
	var labels helper.MetricLabels
	labels.Append("service", service)
	labels.Append("serviceInstance", serviceInstance)

	cpuUsage := helper.NewMetricLog("skywalking_jvm_cpu_usage", metric.Time, metric.GetCpu().UsagePercent, &labels)
	logs = append(logs, cpuUsage)

	memLabels := labels.Clone()
	for _, mem := range metric.Memory {
		var memType string
		if mem.IsHeap {
			memType = "heap"
		} else {
			memType = "nonheap"
		}
		memLabels.Replace("type", memType)
		memCommitted := helper.NewMetricLog("skywalking_jvm_memory_committed", metric.GetTime(), float64(mem.Committed), memLabels)
		logs = append(logs, memCommitted)

		memInit := helper.NewMetricLog("skywalking_jvm_memory_init", metric.GetTime(), float64(mem.Init), memLabels)
		logs = append(logs, memInit)

		memMax := helper.NewMetricLog("skywalking_jvm_memory_max", metric.GetTime(), float64(mem.Max), memLabels)
		logs = append(logs, memMax)

		memUsed := helper.NewMetricLog("skywalking_jvm_memory_used", metric.GetTime(), float64(mem.Used), memLabels)
		logs = append(logs, memUsed)
	}
	for _, memPool := range metric.MemoryPool {
		memLabels.Replace("type", memPool.Type.String())
		memPoolCommitted := helper.NewMetricLog("skywalking_jvm_memory_pool_committed", metric.GetTime(), float64(memPool.Committed), memLabels)
		logs = append(logs, memPoolCommitted)

		memPoolInit := helper.NewMetricLog("skywalking_jvm_memory_pool_init", metric.GetTime(), float64(memPool.Init), memLabels)
		logs = append(logs, memPoolInit)

		memPoolMax := helper.NewMetricLog("skywalking_jvm_memory_pool_max", metric.GetTime(), float64(memPool.Max), memLabels)
		logs = append(logs, memPoolMax)

		memPoolUsed := helper.NewMetricLog("skywalking_jvm_memory_pool_used", metric.GetTime(), float64(memPool.Used), memLabels)
		logs = append(logs, memPoolUsed)
	}

	gcLabels := labels.CloneInto(memLabels)
	for _, gc := range metric.Gc {
		gcLabels.Replace("phrase", gc.Phrase.String())
		gcTime := helper.NewMetricLog("skywalking_jvm_gc_time", metric.GetTime(), float64(gc.Time), gcLabels)
		logs = append(logs, gcTime)

		gcCount := helper.NewMetricLog("skywalking_jvm_gc_count", metric.GetTime(), float64(gc.Count), gcLabels)
		logs = append(logs, gcCount)
	}

	threadsLive := helper.NewMetricLog("skywalking_jvm_threads_live", metric.GetTime(), float64(metric.Thread.LiveCount), &labels)
	logs = append(logs, threadsLive)

	threadsDaemon := helper.NewMetricLog("skywalking_jvm_threads_daemon", metric.GetTime(), float64(metric.Thread.DaemonCount), &labels)
	logs = append(logs, threadsDaemon)

	threadsPeak := helper.NewMetricLog("skywalking_jvm_threads_peak", metric.GetTime(), float64(metric.Thread.PeakCount), &labels)
	logs = append(logs, threadsPeak)

	return logs
}

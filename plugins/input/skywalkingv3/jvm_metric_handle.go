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
	"strconv"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
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
		fixTime := metric.Time - metric.Time%1000
		// 原始数据上传过于频繁, 不需要
		if fixTime-h.lastTime >= h.interval {
			h.lastTime = fixTime
		} else {
			return &v3.Commands{}, nil
		}
		logs := h.toMetricStoreFormat(metric, req.Service, req.ServiceInstance)
		for _, log := range logs {
			h.collector.AddRawLog(log)
		}
	}
	return &v3.Commands{}, nil
}

func (h *JVMMetricHandler) toMetricStoreFormat(metric *skywalking.JVMMetric, service string, serviceInstance string) []*protocol.Log {
	var logs []*protocol.Log
	cpuUsage := util.NewMetricLog("skywalking_jvm_cpu_usage", metric.Time,
		strconv.FormatFloat(metric.GetCpu().UsagePercent, 'f', 6, 64), []util.Label{{Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}})
	logs = append(logs, cpuUsage)

	for _, mem := range metric.Memory {
		var memType string
		if mem.IsHeap {
			memType = "heap"
		} else {
			memType = "nonheap"
		}
		memCommitted := util.NewMetricLog("skywalking_jvm_memory_committed", metric.GetTime(),
			strconv.FormatInt(mem.Committed, 10),
			[]util.Label{{Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: memType}})
		logs = append(logs, memCommitted)

		memInit := util.NewMetricLog("skywalking_jvm_memory_init", metric.GetTime(),
			strconv.FormatInt(mem.Init, 10),
			[]util.Label{{Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: memType}})
		logs = append(logs, memInit)

		memMax := util.NewMetricLog("skywalking_jvm_memory_max", metric.GetTime(),
			strconv.FormatInt(mem.Max, 10),
			[]util.Label{{Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: memType}})
		logs = append(logs, memMax)

		memUsed := util.NewMetricLog("skywalking_jvm_memory_used", metric.GetTime(),
			strconv.FormatInt(mem.Used, 10),
			[]util.Label{{Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: memType}})
		logs = append(logs, memUsed)
	}
	for _, gc := range metric.Gc {
		gcTime := util.NewMetricLog("skywalking_jvm_gc_time", metric.GetTime(),
			strconv.FormatInt(gc.Time, 10),
			[]util.Label{{Name: "phrase", Value: gc.Phrase.String()}, {Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}})
		logs = append(logs, gcTime)

		gcCount := util.NewMetricLog("skywalking_jvm_gc_count", metric.GetTime(),
			strconv.FormatInt(gc.Count, 10),
			[]util.Label{{Name: "phrase", Value: gc.Phrase.String()}, {Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}})
		logs = append(logs, gcCount)
	}

	for _, memPool := range metric.MemoryPool {
		memPoolCommitted := util.NewMetricLog("skywalking_jvm_memory_pool_committed", metric.GetTime(),
			strconv.FormatInt(memPool.Committed, 10),
			[]util.Label{{Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: memPool.Type.String()}})
		logs = append(logs, memPoolCommitted)

		memPoolInit := util.NewMetricLog("skywalking_jvm_memory_pool_init", metric.GetTime(),
			strconv.FormatInt(memPool.Init, 10),
			[]util.Label{{Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: memPool.Type.String()}})
		logs = append(logs, memPoolInit)

		memPoolMax := util.NewMetricLog("skywalking_jvm_memory_pool_max", metric.GetTime(),
			strconv.FormatInt(memPool.Max, 10),
			[]util.Label{{Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: memPool.Type.String()}})
		logs = append(logs, memPoolMax)

		memPoolUsed := util.NewMetricLog("skywalking_jvm_memory_pool_used",
			metric.GetTime(), strconv.FormatInt(memPool.Used, 10),
			[]util.Label{{Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: memPool.Type.String()}})
		logs = append(logs, memPoolUsed)
	}

	threadsLive := util.NewMetricLog("skywalking_jvm_threads_live", metric.GetTime(),
		strconv.FormatInt(metric.Thread.LiveCount, 10),
		[]util.Label{{Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}})
	logs = append(logs, threadsLive)
	threadsDaemon := util.NewMetricLog("skywalking_jvm_threads_daemon", metric.GetTime(),
		strconv.FormatInt(metric.Thread.DaemonCount, 10),
		[]util.Label{{Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}})
	logs = append(logs, threadsDaemon)
	threadsPeak := util.NewMetricLog("skywalking_jvm_threads_peak", metric.GetTime(),
		strconv.FormatInt(metric.Thread.PeakCount, 10),
		[]util.Label{{Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}})
	logs = append(logs, threadsPeak)
	return logs
}

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

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
	common "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	agent "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/agent/v3"
)

type CLRMetricHandler struct {
	context   ilogtail.Context
	collector ilogtail.Collector
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
	cpuUsage := util.NewMetricLog("skywalking_clr_cpu_usage", metric.Time,
		strconv.FormatFloat(metric.GetCpu().UsagePercent, 'f', 6, 64), []util.Label{{Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}})
	logs = append(logs, cpuUsage)

	gc := metric.Gc
	gen0GcCount := util.NewMetricLog("skywalking_clr_gc_count", metric.GetTime(),
		strconv.FormatInt(gc.Gen0CollectCount, 10),
		[]util.Label{{Name: "gen", Value: "gen0"}, {Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}})
	logs = append(logs, gen0GcCount)

	gen1GcCount := util.NewMetricLog("skywalking_clr_gc_count", metric.GetTime(),
		strconv.FormatInt(gc.Gen1CollectCount, 10),
		[]util.Label{{Name: "gen", Value: "gen1"}, {Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}})
	logs = append(logs, gen1GcCount)

	gen2GcCount := util.NewMetricLog("skywalking_clr_gc_count", metric.GetTime(),
		strconv.FormatInt(gc.Gen2CollectCount, 10),
		[]util.Label{{Name: "gen", Value: "gen2"}, {Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}})
	logs = append(logs, gen2GcCount)

	heapMemory := util.NewMetricLog("skywalking_clr_heap_memory", metric.GetTime(),
		strconv.FormatInt(gc.HeapMemory, 10),
		[]util.Label{{Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}})
	logs = append(logs, heapMemory)

	thread := metric.Thread
	availableCompletionPortThreads := util.NewMetricLog("skywalking_clr_threads", metric.GetTime(),
		strconv.FormatInt(int64(thread.AvailableCompletionPortThreads), 10),
		[]util.Label{{Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: "availableCompletionPortThreads"}})
	logs = append(logs, availableCompletionPortThreads)

	availableWorkerThreads := util.NewMetricLog("skywalking_clr_threads", metric.GetTime(),
		strconv.FormatInt(int64(thread.AvailableWorkerThreads), 10),
		[]util.Label{{Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: "availableWorkerThreads"}})
	logs = append(logs, availableWorkerThreads)

	maxCompletionPortThreads := util.NewMetricLog("skywalking_clr_threads", metric.GetTime(),
		strconv.FormatInt(int64(thread.MaxCompletionPortThreads), 10),
		[]util.Label{{Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: "maxCompletionPortThreads"}})
	logs = append(logs, maxCompletionPortThreads)

	maxWorkerThreads := util.NewMetricLog("skywalking_clr_threads", metric.GetTime(),
		strconv.FormatInt(int64(thread.MaxWorkerThreads), 10),
		[]util.Label{{Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: "maxWorkerThreads"}})
	logs = append(logs, maxWorkerThreads)
	return logs
}

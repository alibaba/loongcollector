// Copyright 2021 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http:www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package skywalkingv2

import (
	"context"
	"strconv"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/input/skywalkingv2/skywalking/apm/network/language/agent"
)

type JVMMetricServiceHandle struct {
	RegistryInformationCache

	context   ilogtail.Context
	collector ilogtail.Collector
}

func (j *JVMMetricServiceHandle) Collect(ctx context.Context, metrics *agent.JVMMetrics) (*agent.Downstream, error) {
	defer panicRecover()
	applicationInstance, ok := j.RegistryInformationCache.findApplicationInstanceRegistryInfo(metrics.ApplicationInstanceId)

	if !ok {
		return &agent.Downstream{}, nil
	}

	for _, metric := range metrics.GetMetrics() {
		logs := toMetricStoreFormat(metric, applicationInstance.application.applicationName, applicationInstance.uuid, applicationInstance.host)
		for _, log := range logs {
			j.collector.AddRawLog(log)
		}
	}

	return &agent.Downstream{}, nil
}

func toMetricStoreFormat(metric *agent.JVMMetric, service string, serviceInstance string, host string) []*protocol.Log {
	var logs []*protocol.Log
	cpuUsage := util.NewMetricLog("skywalking_jvm_cpu_usage", metric.Time,
		strconv.FormatFloat(metric.GetCpu().UsagePercent, 'f', 6, 64), []util.Label{{Name: "host", Value: host}, {Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}})
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
			[]util.Label{{Name: "host", Value: host}, {Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: memType}})
		logs = append(logs, memCommitted)

		memInit := util.NewMetricLog("skywalking_jvm_memory_init", metric.GetTime(),
			strconv.FormatInt(mem.Init, 10),
			[]util.Label{{Name: "host", Value: host}, {Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: memType}})
		logs = append(logs, memInit)

		memMax := util.NewMetricLog("skywalking_jvm_memory_max", metric.GetTime(),
			strconv.FormatInt(mem.Max, 10),
			[]util.Label{{Name: "host", Value: host}, {Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: memType}})
		logs = append(logs, memMax)

		memUsed := util.NewMetricLog("skywalking_jvm_memory_used", metric.GetTime(),
			strconv.FormatInt(mem.Used, 10),
			[]util.Label{{Name: "host", Value: host}, {Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: memType}})
		logs = append(logs, memUsed)
	}
	for _, gc := range metric.Gc {
		gcTime := util.NewMetricLog("skywalking_jvm_gc_time", metric.GetTime(),
			strconv.FormatInt(gc.Time, 10),
			[]util.Label{{Name: "host", Value: host}, {Name: "phrase", Value: gc.Phrase.String()}, {Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}})
		logs = append(logs, gcTime)

		phrase := "Old"
		if gc.Phrase.Number() == agent.GCPhrase_NEW.Number() {
			phrase = "Young"
		}

		gcCount := util.NewMetricLog("skywalking_jvm_gc_count", metric.GetTime(),
			strconv.FormatInt(gc.Count, 10),
			[]util.Label{{Name: "host", Value: host}, {Name: "phrase", Value: phrase}, {Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}})
		logs = append(logs, gcCount)
	}

	for _, memPool := range metric.MemoryPool {
		memPoolCommitted := util.NewMetricLog("skywalking_jvm_memory_pool_committed", metric.GetTime(),
			strconv.FormatInt(memPool.Commited, 10),
			[]util.Label{{Name: "host", Value: host}, {Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: memPool.Type.String()}})
		logs = append(logs, memPoolCommitted)

		memPoolInit := util.NewMetricLog("skywalking_jvm_memory_pool_init", metric.GetTime(),
			strconv.FormatInt(memPool.Init, 10),
			[]util.Label{{Name: "host", Value: host}, {Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: memPool.Type.String()}})
		logs = append(logs, memPoolInit)

		memPoolMax := util.NewMetricLog("skywalking_jvm_memory_pool_max", metric.GetTime(),
			strconv.FormatInt(memPool.Max, 10),
			[]util.Label{{Name: "host", Value: host}, {Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: memPool.Type.String()}})
		logs = append(logs, memPoolMax)

		memPoolUsed := util.NewMetricLog("skywalking_jvm_memory_pool_used",
			metric.GetTime(), strconv.FormatInt(memPool.Used, 10),
			[]util.Label{{Name: "host", Value: host}, {Name: "service", Value: service}, {Name: "serviceInstance", Value: serviceInstance}, {Name: "type", Value: memPool.Type.String()}})
		logs = append(logs, memPoolUsed)
	}

	return logs
}

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

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/input/skywalkingv2/skywalking/apm/network/language/agent"
)

type JVMMetricServiceHandle struct {
	RegistryInformationCache

	context   pipeline.Context
	collector pipeline.Collector
}

func (j *JVMMetricServiceHandle) Collect(ctx context.Context, metrics *agent.JVMMetrics) (*agent.Downstream, error) {
	defer panicRecover()
	applicationInstance, ok := j.RegistryInformationCache.findApplicationInstanceRegistryInfo(metrics.ApplicationInstanceId)

	if !ok {
		logger.Warning(j.context.GetRuntimeContext(), "Failed to find application instance registry info", "applicationInstanceId", metrics.ApplicationInstanceId)
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
	var labels helper.MetricLabels
	labels.Append("service", service)
	labels.Append("serviceInstance", serviceInstance)
	labels.Append("host", host)

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
		memPoolCommitted := helper.NewMetricLog("skywalking_jvm_memory_pool_committed", metric.GetTime(), float64(memPool.Commited), memLabels)
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
		memLabels.Replace("phrase", gc.Phrase.String())
		gcTime := helper.NewMetricLog("skywalking_jvm_gc_time", metric.GetTime(), float64(gc.Time), gcLabels)
		logs = append(logs, gcTime)

		phrase := "Old"
		if gc.Phrase.Number() == agent.GCPhrase_NEW.Number() {
			phrase = "Young"
		}
		memLabels.Replace("phrase", phrase)

		gcCount := helper.NewMetricLog("skywalking_jvm_gc_count", metric.GetTime(), float64(gc.Count), gcLabels)
		logs = append(logs, gcCount)
	}

	return logs
}

/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "host_monitor/collector/CPUCollector.h"

#include <ctime>

#include <string>

#include "common/Flags.h"
#include "common/StringView.h"
#include "host_monitor/HostMonitorContext.h"
#include "host_monitor/SystemInterface.h"
#include "host_monitor/collector/CollectorConstants.h"
#include "logger/Logger.h"

DEFINE_FLAG_INT32(basic_host_monitor_cpu_collect_interval, "basic host monitor cpu collect interval, seconds", 1);
namespace logtail {

const std::string CPUCollector::sName = "cpu";

bool CPUCollector::Collect(HostMonitorContext& collectContext, PipelineEventGroup* groupPtr) {
    CPUInformation cpuInfo;
    CPUPercent totalCpuPercent{};
    if (!SystemInterface::GetInstance()->GetCPUInformation(collectContext.GetMetricTime(), cpuInfo)) {
        return false;
    }

    if (cpuInfo.stats.size() <= 1) {
        LOG_ERROR(sLogger, ("cpu count is negative", cpuInfo.stats.size()));
        return false;
    }

    for (const auto& cpu : cpuInfo.stats) {
        if (cpu.index != -1) {
            continue;
        }

        CPUStat cpuTotal = cpu;
        double cpuCores = cpuCount;
        if (!CalculateCPUPercent(totalCpuPercent, cpuTotal)) {
            return false;
        }
        // first time get cpu count and not calculate
        if (cpuCount == 0) {
            cpuCount = cpuInfo.stats.size() - 1;
            return true;
        }

        cpuCount = cpuInfo.stats.size() - 1;
        mCalculate.AddValue(totalCpuPercent);

        // If group is not provided, just collect data without generating metrics
        if (!groupPtr) {
            return true;
        }

        CPUPercent minCPU, maxCPU, avgCPU, lastCPU;
        mCalculate.Stat(maxCPU, minCPU, avgCPU, &lastCPU);

        mCalculate.Reset();
        struct MetricDef {
            StringView name;
            double* value;
        } metrics[] = {
            {KEY_CPU_SYSTEM_AVG, &avgCPU.sys},  {KEY_CPU_SYSTEM_MIN, &minCPU.sys},  {KEY_CPU_SYSTEM_MAX, &maxCPU.sys},
            {KEY_CPU_IDLE_AVG, &avgCPU.idle},   {KEY_CPU_IDLE_MIN, &minCPU.idle},   {KEY_CPU_IDLE_MAX, &maxCPU.idle},
            {KEY_CPU_USER_AVG, &avgCPU.user},   {KEY_CPU_USER_MIN, &minCPU.user},   {KEY_CPU_USER_MAX, &maxCPU.user},
            {KEY_CPU_WAIT_AVG, &avgCPU.wait},   {KEY_CPU_WAIT_MIN, &minCPU.wait},   {KEY_CPU_WAIT_MAX, &maxCPU.wait},
            {KEY_CPU_OTHER_AVG, &avgCPU.other}, {KEY_CPU_OTHER_MIN, &minCPU.other}, {KEY_CPU_OTHER_MAX, &maxCPU.other},
            {KEY_CPU_TOTAL_AVG, &avgCPU.total}, {KEY_CPU_TOTAL_MIN, &minCPU.total}, {KEY_CPU_TOTAL_MAX, &maxCPU.total},
            {KEY_CPU_CORES_VALUE, &cpuCores},
        };
        MetricEvent* metricEvent = groupPtr->AddMetricEvent(true);
        if (!metricEvent) {
            return false;
        }
        metricEvent->SetTimestamp(cpuInfo.collectTime, 0);
        metricEvent->SetValue<UntypedMultiDoubleValues>(metricEvent);
        metricEvent->SetTagNoCopy(TAG_KEY_M, METRIC_SYSTEM_CPU);
        auto* multiDoubleValues = metricEvent->MutableValue<UntypedMultiDoubleValues>();
        for (const auto& def : metrics) {
            multiDoubleValues->SetValue(def.name,
                                        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, *def.value});
        }
    }
    return true;
}

bool CPUCollector::CalculateCPUPercent(CPUPercent& cpuPercent, CPUStat& currentCpu) {
    if (cpuCount == 0) {
        lastCpu = currentCpu;
        cpuPercent.sys = cpuPercent.user = cpuPercent.wait = cpuPercent.idle = cpuPercent.other = cpuPercent.total
            = 0.0;
        LOG_DEBUG(sLogger, ("first time collect Cpu info", "empty"));
        return true;
    }

    double currentJiffies, lastJiffies, jiffiesDelta;
    currentJiffies = currentCpu.user + currentCpu.nice + currentCpu.system + currentCpu.idle + currentCpu.iowait
        + currentCpu.irq + currentCpu.softirq + currentCpu.steal;
    lastJiffies = lastCpu.user + lastCpu.nice + lastCpu.system + lastCpu.idle + lastCpu.iowait + lastCpu.irq
        + lastCpu.softirq + lastCpu.steal;
    jiffiesDelta = currentJiffies - lastJiffies;

    if (jiffiesDelta <= 0) {
        LOG_ERROR(sLogger, ("jiffies delta is negative", "skip"));
        return false;
    }

    cpuPercent.sys = (currentCpu.system - lastCpu.system) / jiffiesDelta * 100;
    cpuPercent.user = (currentCpu.user - lastCpu.user) / jiffiesDelta * 100;
    cpuPercent.wait = (currentCpu.iowait - lastCpu.iowait) / jiffiesDelta * 100;
    cpuPercent.idle = (currentCpu.idle - lastCpu.idle) / jiffiesDelta * 100;
    cpuPercent.other = (currentCpu.nice + currentCpu.irq + currentCpu.softirq + currentCpu.steal - lastCpu.nice
                        - lastCpu.irq - lastCpu.softirq - lastCpu.steal)
        / jiffiesDelta * 100;
    cpuPercent.total = 100 - cpuPercent.idle;
    lastCpu = currentCpu;
    return true;
}

const std::chrono::seconds CPUCollector::GetCollectInterval() const {
    return std::chrono::seconds(INT32_FLAG(basic_host_monitor_cpu_collect_interval));
}

} // namespace logtail

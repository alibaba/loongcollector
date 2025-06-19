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

#include <string>

#include "host_monitor/Constants.h"
#include "host_monitor/SystemInterface.h"

namespace logtail {

const std::string CPUCollector::sName = "cpu";

CPUCollector::CPUCollector() {
    Init();
}
int CPUCollector::Init(int totalCount) {
    mCountPerReport = totalCount;
    mCount = 0;
    return 0;
}
bool CPUCollector::Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) {
    if (group == nullptr) {
        return false;
    }
    CPUInformation cpuInfo;
    CPUPercent totalCpuPercent{};
    if (!SystemInterface::GetInstance()->GetCPUInformation(cpuInfo)) {
        return false;
    }
    const time_t now = time(nullptr);

    for (const auto& cpu : cpuInfo.stats) {
        if (cpu.index != -1) {
            continue;
        }

        CPUStat cpuTolal = cpu;
        double cpuCores = cpuCount;
        if (!CalculateCPUPercent(totalCpuPercent, cpuTolal)) {
            return false;
        }
        // first time get cpu count and not calculate mCount
        if (cpuCount == 0) {
            cpuCount = cpuInfo.stats.size() - 1;
            return true;
        }

        cpuCount = cpuInfo.stats.size() - 1;
        mCalculate.AddValue(totalCpuPercent);
        mCount++;

        if (mCount < mCountPerReport) {
            return true;
        }

        CPUPercent minCPU, maxCPU, avgCPU, lastCPU;
        mCalculate.Stat(maxCPU, minCPU, avgCPU, &lastCPU);

        mCount = 0;
        mCalculate.Reset();
        struct MetricDef {
            const char* name;
            double* value;
        } metrics[] = {
            {"cpu_system_avg", &avgCPU.sys},  {"cpu_system_min", &minCPU.sys},  {"cpu_system_max", &maxCPU.sys},
            {"cpu_idle_avg", &avgCPU.idle},   {"cpu_idle_min", &minCPU.idle},   {"cpu_idle_max", &maxCPU.idle},
            {"cpu_user_avg", &avgCPU.user},   {"cpu_user_min", &minCPU.user},   {"cpu_user_max", &maxCPU.user},
            {"cpu_wait_avg", &avgCPU.wait},   {"cpu_wait_min", &minCPU.wait},   {"cpu_wait_max", &maxCPU.wait},
            {"cpu_other_avg", &avgCPU.other}, {"cpu_other_min", &minCPU.other}, {"cpu_other_max", &maxCPU.other},
            {"cpu_total_avg", &avgCPU.total}, {"cpu_total_min", &minCPU.total}, {"cpu_total_max", &maxCPU.total},
            {"cpu_cores_value", &cpuCores},

        };
        MetricEvent* metricEvent = group->AddMetricEvent(true);
        if (!metricEvent) {
            return false;
        }
        metricEvent->SetTimestamp(now, 0);
        metricEvent->SetValue<UntypedMultiDoubleValues>(metricEvent);
        auto* multiDoubleValues = metricEvent->MutableValue<UntypedMultiDoubleValues>();
        for (const auto& def : metrics) {
            if (std::string(def.name) == "cpu_cores_value") {
                multiDoubleValues->SetValue(
                    std::string(def.name),
                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, *def.value});
            } else {
                multiDoubleValues->SetValue(
                    std::string(def.name),
                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, (*def.value) * 100});
            }
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
        LOG_DEBUG(sLogger, ("jiffies delta is negative", "skip"));
        return false;
    }

    cpuPercent.sys = (currentCpu.system - lastCpu.system) / jiffiesDelta;
    cpuPercent.user = (currentCpu.user - lastCpu.user) / jiffiesDelta;
    cpuPercent.wait = (currentCpu.iowait - lastCpu.iowait) / jiffiesDelta;
    cpuPercent.idle = (currentCpu.idle - lastCpu.idle) / jiffiesDelta;
    cpuPercent.other = (currentCpu.nice + currentCpu.irq + currentCpu.softirq + currentCpu.steal - lastCpu.nice
                        - lastCpu.irq - lastCpu.softirq - lastCpu.steal)
        / jiffiesDelta;
    cpuPercent.total = 1 - cpuPercent.idle;
    lastCpu = currentCpu;
    return true;
}

} // namespace logtail

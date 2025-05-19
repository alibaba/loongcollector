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
const std::string kMetricLabelCPU = "cpu";
const std::string kMetricLabelMode = "mode";

bool CPUCollector::Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) {
    if (group == nullptr) {
        return false;
    }
    CPUInformation cpuInfo;
    if (!SystemInterface::GetInstance()->GetCPUInformation(cpuInfo)) {
        return false;
    }
    const time_t now = time(nullptr);
    constexpr struct MetricDef {
        const char* name;
        const char* mode;
        double CPUStat::*value;
    } metrics[] = {
        {"node_cpu_seconds_total", "user", &CPUStat::user},
        {"node_cpu_seconds_total", "nice", &CPUStat::nice},
        {"node_cpu_seconds_total", "system", &CPUStat::system},
        {"node_cpu_seconds_total", "idle", &CPUStat::idle},
        {"node_cpu_seconds_total", "iowait", &CPUStat::iowait},
        {"node_cpu_seconds_total", "irq", &CPUStat::irq},
        {"node_cpu_seconds_total", "softirq", &CPUStat::softirq},
        {"node_cpu_seconds_total", "steal", &CPUStat::steal},
        {"node_cpu_guest_seconds_total", "user", &CPUStat::guest},
        {"node_cpu_guest_seconds_total", "nice", &CPUStat::guestNice},
    };
    for (const auto& cpu : cpuInfo.stats) {
        if (cpu.index == -1) {
            continue;
        }
        for (const auto& def : metrics) {
            auto* metricEvent = group->AddMetricEvent(true);
            if (!metricEvent) {
                continue;
            }
            metricEvent->SetName(def.name);
            metricEvent->SetTimestamp(now, 0);
            metricEvent->SetValue<UntypedSingleValue>(cpu.*(def.value) / SYSTEM_HERTZ);
            metricEvent->SetTag(kMetricLabelCPU, std::to_string(cpu.index));
            metricEvent->SetTagNoCopy(kMetricLabelMode, def.mode);
        }
    }
    return true;
}

} // namespace logtail

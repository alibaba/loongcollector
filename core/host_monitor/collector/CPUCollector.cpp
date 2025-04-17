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

#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/split.hpp"

#include "MetricValue.h"
#include "common/StringTools.h"
#include "host_monitor/Constants.h"
#include "host_monitor/SystemInformationTools.h"
#include "logger/Logger.h"

namespace logtail {

const std::string CPUCollector::sName = "cpu";
const std::string kMetricLabelCPU = "cpu";
const std::string kMetricLabelMode = "mode";

bool CPUCollector::Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) {
    if (group == nullptr) {
        return false;
    }
    std::vector<CPUStat> cpus;
    if (!GetHostSystemCPUStat(cpus)) {
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
    for (const auto& cpu : cpus) {
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

bool CPUCollector::GetHostSystemCPUStat(std::vector<CPUStat>& cpus) {
    std::vector<std::string> cpuLines;
    std::string errorMessage;
    if (!GetHostSystemStat(cpuLines, errorMessage)) {
        if (mValidState) {
            LOG_WARNING(sLogger, ("failed to get system cpu", "invalid CPU collector")("error msg", errorMessage));
            mValidState = false;
        }
        return false;
    }
    mValidState = true;
    // cpu  1195061569 1728645 418424132 203670447952 14723544 0 773400 0 0 0
    // cpu0 14708487 14216 4613031 2108180843 57199 0 424744 0 0 0
    // ...
    cpus.clear();
    cpus.reserve(cpuLines.size());
    for (auto const& line : cpuLines) {
        std::vector<std::string> cpuMetric;
        boost::split(cpuMetric, line, boost::is_any_of(" "), boost::token_compress_on);
        if (cpuMetric.size() > 0 && cpuMetric[0].substr(0, 3) == "cpu") {
            CPUStat cpuStat{};
            if (cpuMetric[0] == "cpu") {
                cpuStat.index = -1;
            } else {
                if (!StringTo(cpuMetric[0].substr(3), cpuStat.index)) {
                    LOG_ERROR(sLogger, ("failed to parse cpu index", "skip")("wrong cpu index", cpuMetric[0]));
                    continue;
                }
            }
            cpuStat.user = ParseMetric(cpuMetric, EnumCpuKey::user);
            cpuStat.nice = ParseMetric(cpuMetric, EnumCpuKey::nice);
            cpuStat.system = ParseMetric(cpuMetric, EnumCpuKey::system);
            cpuStat.idle = ParseMetric(cpuMetric, EnumCpuKey::idle);
            cpuStat.iowait = ParseMetric(cpuMetric, EnumCpuKey::iowait);
            cpuStat.irq = ParseMetric(cpuMetric, EnumCpuKey::irq);
            cpuStat.softirq = ParseMetric(cpuMetric, EnumCpuKey::softirq);
            cpuStat.steal = ParseMetric(cpuMetric, EnumCpuKey::steal);
            cpuStat.guest = ParseMetric(cpuMetric, EnumCpuKey::guest);
            cpuStat.guestNice = ParseMetric(cpuMetric, EnumCpuKey::guest_nice);
            cpus.push_back(cpuStat);
        }
    }
    return true;
}

double CPUCollector::ParseMetric(const std::vector<std::string>& cpuMetric, EnumCpuKey key) const {
    if (cpuMetric.size() <= static_cast<size_t>(key)) {
        return 0.0;
    }
    double value = 0.0;
    if (!StringTo(cpuMetric[static_cast<size_t>(key)], value)) {
        LOG_WARNING(
            sLogger,
            ("failed to parse cpu metric", static_cast<size_t>(key))("value", cpuMetric[static_cast<size_t>(key)]));
    }
    return value;
}

} // namespace logtail

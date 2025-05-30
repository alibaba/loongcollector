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

#include "host_monitor/collector/SystemCollector.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <chrono>
#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/split.hpp"

#include "MetricValue.h"
#include "common/StringTools.h"
// #include "host_monitor/Constants.h"
#include "host_monitor/SystemInformationTools.h"
#include "logger/Logger.h"

namespace logtail {

const std::string SystemCollector::sName = "system";
const std::string kMetricLabelLoad = "system";
const std::string kMetricLabelMode = "mode";

SystemCollector::SystemCollector(){
    Init();
}
int SystemCollector::Init(int totalCount) {
    mTotalCount = totalCount;
    mCount = 0;
    return 0;
}
bool SystemCollector::Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) {
    if (group == nullptr) {
        return false;
    }
    SystemStat load;
    if (!GetHostSystemLoadStat(load)) {
        return false;
    }

    mCalculate.AddValue(load);

    mCount++;
    if (mCount < mTotalCount) {
        return true;
    }

    SystemStat minSys, maxSys, avgSys, lastSys;
    mCalculate.Stat(maxSys, minSys, avgSys, &lastSys);

    mCount = 0;
    mCalculate.Reset();

    const time_t now = time(nullptr);

    // 数据整理
    struct MetricDef {
        const char* name;
        const char* mode;
        double* value;
    } metrics[] = {
        {"load_1m", "Minimum", &minSys.load1},
        {"load_1m", "Maximum", &maxSys.load1},
        {"load_1m", "Average", &avgSys.load1},
        {"load_5m", "Minimum", &minSys.load5},
        {"load_5m", "Maximum", &maxSys.load5},
        {"load_5m", "Average", &avgSys.load5},
        {"load_15m", "Minimum", &minSys.load15},
        {"load_15m", "Maximum", &maxSys.load15},
        {"load_15m", "Average", &avgSys.load15},
        {"load_per_core_1m", "Minimum", &minSys.load1_per_core},
        {"load_per_core_1m", "Maximum", &maxSys.load1_per_core},
        {"load_per_core_1m", "Average", &avgSys.load1_per_core},
        {"load_per_core_5m", "Minimum", &minSys.load5_per_core},
        {"load_per_core_5m", "Maximum", &maxSys.load5_per_core},
        {"load_per_core_5m", "Average", &avgSys.load5_per_core},
        {"load_per_core_15m", "Minimum", &minSys.load15_per_core},
        {"load_per_core_15m", "Maximum", &maxSys.load15_per_core},
        {"load_per_core_15m", "Average", &avgSys.load15_per_core},
    };

    for (const auto& def : metrics) {
        auto* metricEvent = group->AddMetricEvent(true);
        if (!metricEvent) {
            continue;
        }
        metricEvent->SetName(def.name);
        metricEvent->SetTimestamp(now, 0);
        metricEvent->SetValue<UntypedSingleValue>(*def.value);
        metricEvent->SetTag(kMetricLabelMode, def.mode);
    }

    return true;
}

bool SystemCollector::GetHostSystemLoadStat(SystemStat& systemload) {
    std::vector<std::string> loadLines;
    std::string errorMessage;
    if (!GetHostSystemStatWithPath(loadLines, errorMessage, "/proc/loadavg")) {
        if (mValidState) {
            LOG_WARNING(sLogger, ("failed to get system load", "invalid System collector")("error msg", errorMessage));
            mValidState = false;
        }
        return false;
    }

    mValidState = true;
    // cat /proc/loadavg
    // 0.10 0.07 0.03 1/561 78450
    std::vector<std::string> loadMetric;
    boost::split(loadMetric, loadLines[0], boost::is_any_of(" "), boost::token_compress_on);

    systemload.load1 = std::atof(loadMetric[0].c_str());
    systemload.load5 = std::atof(loadMetric[1].c_str());
    systemload.load15 = std::atof(loadMetric[2].c_str());

    auto cpuCoreCount = static_cast<double>(std::thread::hardware_concurrency());
    cpuCoreCount = cpuCoreCount < 1 ? 1 : cpuCoreCount;

    systemload.load1_per_core = systemload.load1 / cpuCoreCount;
    systemload.load5_per_core = systemload.load5 / cpuCoreCount;
    systemload.load15_per_core = systemload.load15 / cpuCoreCount;

    return true;
}

} // namespace logtail

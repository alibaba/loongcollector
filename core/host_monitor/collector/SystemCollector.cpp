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

#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/split.hpp"

#include "MetricValue.h"
#include "common/StringTools.h"
#include "host_monitor/SystemInformationTools.h"
#include "logger/Logger.h"

namespace logtail {

const std::string SystemCollector::sName = "system";
const std::string kMetricLabelMode = "valueTag";

SystemCollector::SystemCollector() {
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
    std::vector<double> values = {minSys.load1,
                                  maxSys.load1,
                                  avgSys.load1,
                                  minSys.load5,
                                  maxSys.load5,
                                  avgSys.load5,
                                  minSys.load15,
                                  maxSys.load15,
                                  avgSys.load15,
                                  minSys.load1PerCore,
                                  maxSys.load1PerCore,
                                  avgSys.load1PerCore,
                                  minSys.load5PerCore,
                                  maxSys.load5PerCore,
                                  avgSys.load5PerCore,
                                  minSys.load15PerCore,
                                  maxSys.load15PerCore,
                                  avgSys.load15PerCore};
    std::vector<std::string> names = {"load_1m_min",
                                      "load_1m_max",
                                      "load_1m_avg",
                                      "load_5m_min",
                                      "load_5m_max",
                                      "load_5m_avg",
                                      "load_15m_min",
                                      "load_15m_max",
                                      "load_15m_avg",
                                      "load_per_core_1m_min",
                                      "load_per_core_1m_max",
                                      "load_per_core_1m_avg",
                                      "load_per_core_5m_min",
                                      "load_per_core_5m_max",
                                      "load_per_core_5m_avg",
                                      "load_per_core_15m_min",
                                      "load_per_core_15m_max",
                                      "load_per_core_15m_avg"};


    MetricEvent* metricEvent = group->AddMetricEvent(true);
    if (!metricEvent) {
        return false;
    }
    metricEvent->SetTimestamp(now, 0);
    metricEvent->SetValue<UntypedMultiDoubleValues>(metricEvent);
    auto* multiDoubleValues = metricEvent->MutableValue<UntypedMultiDoubleValues>();
    for (size_t i = 0; i < names.size(); ++i) {
        multiDoubleValues->SetValue(std::string(names[i]),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, values[i]});
    }

    return true;
}

bool SystemCollector::GetHostSystemLoadStat(SystemStat& systemload) {
    std::vector<std::string> loadLines;
    std::string errorMessage;
    if (!GetHostSystemStatWithPath(loadLines, errorMessage, PROCESS_DIR / PROCESS_LOADAVG) || loadLines.empty()) {
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

    if (loadMetric.size() < 3) {
        LOG_WARNING(sLogger, ("failed to split load metric", "invalid System collector"));
        return false;
    }

    systemload.load1 = std::stod(loadMetric[0]);
    systemload.load5 = std::stod(loadMetric[1]);
    systemload.load15 = std::stod(loadMetric[2]);

    auto cpuCoreCount = static_cast<double>(std::thread::hardware_concurrency());
    cpuCoreCount = cpuCoreCount < 1 ? 1.0 : cpuCoreCount;

    systemload.load1PerCore = systemload.load1 / cpuCoreCount;
    systemload.load5PerCore = systemload.load5 / cpuCoreCount;
    systemload.load15PerCore = systemload.load15 / cpuCoreCount;

    return true;
}

} // namespace logtail

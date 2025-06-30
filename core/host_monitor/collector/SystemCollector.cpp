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
#include <string>

#include "MetricValue.h"
#include "host_monitor/Constants.h"
#include "host_monitor/SystemInterface.h"

namespace logtail {

const std::string SystemCollector::sName = "system";
const std::string kMetricLabelMode = "valueTag";

void SystemCollector::Init(const HostMonitorCollectConfig& collectConfig) {
    mCountPerReport = collectConfig.mInterval.count() / kHostMonitorMinInterval;
    mCount = 0;
}

bool SystemCollector::Collect(PipelineEventGroup* group) {
    if (group == nullptr) {
        return false;
    }
    SystemLoadInformation load;
    if (!SystemInterface::GetInstance()->GetSystemLoadInformation(load)) {
        return false;
    }

    mCalculate.AddValue(load.systemStat);

    mCount++;
    if (mCount < mCountPerReport) {
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


} // namespace logtail

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

#include "host_monitor/collector/MemCollector.h"

#include <boost/algorithm/string.hpp>
#include <filesystem>
#include <string>

#include "boost/algorithm/string/split.hpp"

#include "MetricValue.h"
#include "common/StringTools.h"
#include "host_monitor/Constants.h"
#include "host_monitor/LinuxSystemInterface.h"
#include "host_monitor/SystemInterface.h"
#include "logger/Logger.h"

namespace logtail {

const std::string MemCollector::sName = "memory";

MemCollector::MemCollector() {
    Init();
}

int MemCollector::Init(int totalCount) {
    mCountPerReport = totalCount;
    mCount = 0;
    return 0;
}

bool MemCollector::Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) {
    if (group == nullptr) {
        return false;
    }

    MemoryInformation meminfo;
    if (!GetHostMeminfoStat(meminfo)) {
        return false;
    }


    mCalculateMeminfo.AddValue(meminfo.memStat);
    mCount++;
    if (mCount < mCountPerReport) {
        return true;
    }
    MemoryStat minMem, maxMem, avgMem, lastMem;

    mCalculateMeminfo.Stat(maxMem, minMem, avgMem, &lastMem);

    mCount = 0;
    mCalculateMeminfo.Reset();

    const time_t now = time(nullptr);

    MetricEvent* metricEvent = group->AddMetricEvent(true);
    if (!metricEvent) {
        return false;
    }
    metricEvent->SetTimestamp(now, 0);
    metricEvent->SetValue<UntypedMultiDoubleValues>(metricEvent);
    auto* multiDoubleValues = metricEvent->MutableValue<UntypedMultiDoubleValues>();
    multiDoubleValues->SetValue(std::string("memory_usedutilization_min"),
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.usedPercent});
    multiDoubleValues->SetValue(std::string("memory_usedutilization_max"),
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.usedPercent});
    multiDoubleValues->SetValue(std::string("memory_usedutilization_avg"),
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.usedPercent});
    multiDoubleValues->SetValue(std::string("memory_freeutilization_min"),
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.freePercent});
    multiDoubleValues->SetValue(std::string("memory_freeutilization_max"),
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.freePercent});
    multiDoubleValues->SetValue(std::string("memory_freeutilization_avg"),
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.freePercent});
    multiDoubleValues->SetValue(std::string("memory_actualusedspace_min"),
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.actualUsed});
    multiDoubleValues->SetValue(std::string("memory_actualusedspace_max"),
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.actualUsed});
    multiDoubleValues->SetValue(std::string("memory_actualusedspace_avg"),
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.actualUsed});
    multiDoubleValues->SetValue(std::string("memory_freespace_min"),
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.free});
    multiDoubleValues->SetValue(std::string("memory_freespace_max"),
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.free});
    multiDoubleValues->SetValue(std::string("memory_freespace_avg"),
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.free});
    multiDoubleValues->SetValue(std::string("memory_usedspace_min"),
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.used});
    multiDoubleValues->SetValue(std::string("memory_usedspace_max"),
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.used});
    multiDoubleValues->SetValue(std::string("memory_usedspace_avg"),
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.used});
    multiDoubleValues->SetValue(std::string("memory_totalspace_min"),
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.total});
    multiDoubleValues->SetValue(std::string("memory_totalspace_max"),
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.total});
    multiDoubleValues->SetValue(std::string("memory_totalspace_avg"),
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.total});
    metricEvent->SetTag(std::string("m"), std::string("system.memory"));
    return true;
}

bool MemCollector::GetHostMeminfoStat(MemoryInformation& meminfo) {
    if (!SystemInterface::GetInstance()->GetHostMemInformationStat(meminfo)) {
        return false;
    }

    return true;
}

} // namespace logtail

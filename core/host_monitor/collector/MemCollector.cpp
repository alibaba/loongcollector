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
#include "common/StringView.h"
#include "host_monitor/Constants.h"
#include "host_monitor/HostMonitorContext.h"
#include "host_monitor/LinuxSystemInterface.h"
#include "host_monitor/SystemInterface.h"
#include "host_monitor/collector/CollectorConstants.h"
#include "logger/Logger.h"

DEFINE_FLAG_INT32(basic_host_monitor_mem_collect_interval, "basic host monitor mem collect interval, seconds", 5);

namespace logtail {

const std::string MemCollector::sName = "memory";

bool MemCollector::Collect(HostMonitorContext& collectContext, PipelineEventGroup* groupPtr) {
    MemoryInformation meminfo;
    if (!SystemInterface::GetInstance()->GetHostMemInformationStat(collectContext.GetMetricTime(), meminfo)) {
        return false;
    }

    mCalculateMeminfo.AddValue(meminfo.memStat);

    // If group is not provided, just collect data without generating metrics
    if (!groupPtr) {
        return true;
    }

    MemoryStat minMem, maxMem, avgMem, lastMem;
    mCalculateMeminfo.Stat(maxMem, minMem, avgMem, &lastMem);
    mCalculateMeminfo.Reset();

    MetricEvent* metricEvent = groupPtr->AddMetricEvent(true);
    if (!metricEvent) {
        return false;
    }
    metricEvent->SetTimestamp(meminfo.collectTime, 0);
    metricEvent->SetValue<UntypedMultiDoubleValues>(metricEvent);
    auto* multiDoubleValues = metricEvent->MutableValue<UntypedMultiDoubleValues>();
    multiDoubleValues->SetValue(kMemoryUsedutilizationMin,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.usedPercent});
    multiDoubleValues->SetValue(kMemoryUsedutilizationMax,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.usedPercent});
    multiDoubleValues->SetValue(kMemoryUsedutilizationAvg,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.usedPercent});
    multiDoubleValues->SetValue(kMemoryFreeutilizationMin,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.freePercent});
    multiDoubleValues->SetValue(kMemoryFreeutilizationMax,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.freePercent});
    multiDoubleValues->SetValue(kMemoryFreeutilizationAvg,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.freePercent});
    multiDoubleValues->SetValue(kMemoryActualusedspaceMin,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.actualUsed});
    multiDoubleValues->SetValue(kMemoryActualusedspaceMax,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.actualUsed});
    multiDoubleValues->SetValue(kMemoryActualusedspaceAvg,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.actualUsed});
    multiDoubleValues->SetValue(kMemoryFreespaceMin,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.free});
    multiDoubleValues->SetValue(kMemoryFreespaceMax,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.free});
    multiDoubleValues->SetValue(kMemoryFreespaceAvg,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.free});
    multiDoubleValues->SetValue(kMemoryUsedspaceMin,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.used});
    multiDoubleValues->SetValue(kMemoryUsedspaceMax,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.used});
    multiDoubleValues->SetValue(kMemoryUsedspaceAvg,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.used});
    multiDoubleValues->SetValue(kMemoryTotalspaceMin,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.total});
    multiDoubleValues->SetValue(kMemoryTotalspaceMax,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.total});
    multiDoubleValues->SetValue(kMemoryTotalspaceAvg,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.total});
    metricEvent->SetTagNoCopy(kTagKeyM, kMetricSystemMemory);
    return true;
}

const std::chrono::seconds MemCollector::GetCollectInterval() const {
    return std::chrono::seconds(INT32_FLAG(basic_host_monitor_mem_collect_interval));
}

} // namespace logtail

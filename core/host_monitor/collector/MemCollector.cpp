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
    multiDoubleValues->SetValue(KEY_MEMORY_USEDUTILIZATION_MIN,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.usedPercent});
    multiDoubleValues->SetValue(KEY_MEMORY_USEDUTILIZATION_MAX,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.usedPercent});
    multiDoubleValues->SetValue(KEY_MEMORY_USEDUTILIZATION_AVG,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.usedPercent});
    multiDoubleValues->SetValue(KEY_MEMORY_FREEUTILIZATION_MIN,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.freePercent});
    multiDoubleValues->SetValue(KEY_MEMORY_FREEUTILIZATION_MAX,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.freePercent});
    multiDoubleValues->SetValue(KEY_MEMORY_FREEUTILIZATION_AVG,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.freePercent});
    multiDoubleValues->SetValue(KEY_MEMORY_ACTUALUSEDSPACE_MIN,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.actualUsed});
    multiDoubleValues->SetValue(KEY_MEMORY_ACTUALUSEDSPACE_MAX,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.actualUsed});
    multiDoubleValues->SetValue(KEY_MEMORY_ACTUALUSEDSPACE_AVG,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.actualUsed});
    multiDoubleValues->SetValue(KEY_MEMORY_FREESPACE_MIN,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.free});
    multiDoubleValues->SetValue(KEY_MEMORY_FREESPACE_MAX,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.free});
    multiDoubleValues->SetValue(KEY_MEMORY_FREESPACE_AVG,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.free});
    multiDoubleValues->SetValue(KEY_MEMORY_USEDSPACE_MIN,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.used});
    multiDoubleValues->SetValue(KEY_MEMORY_USEDSPACE_MAX,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.used});
    multiDoubleValues->SetValue(KEY_MEMORY_USEDSPACE_AVG,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.used});
    multiDoubleValues->SetValue(KEY_MEMORY_TOTALSPACE_MIN,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.total});
    multiDoubleValues->SetValue(KEY_MEMORY_TOTALSPACE_MAX,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.total});
    multiDoubleValues->SetValue(KEY_MEMORY_TOTALSPACE_AVG,
                                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.total});
    metricEvent->SetTagNoCopy(TAG_KEY_M, METRIC_SYSTEM_MEMORY);
    return true;
}

const std::chrono::seconds MemCollector::GetCollectInterval() const {
    return std::chrono::seconds(INT32_FLAG(basic_host_monitor_mem_collect_interval));
}

} // namespace logtail

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
#include "common/StringView.h"
#include "host_monitor/HostMonitorContext.h"
#include "host_monitor/SystemInterface.h"
#include "host_monitor/collector/CollectorConstants.h"

DEFINE_FLAG_INT32(basic_host_monitor_system_collect_interval, "basic host monitor system collect interval, seconds", 1);

namespace logtail {

const std::string SystemCollector::sName = "system";
const std::string kMetricLabelMode = "valueTag";

bool SystemCollector::Collect(HostMonitorContext& collectContext, PipelineEventGroup* groupPtr) {
    SystemLoadInformation load;
    if (!SystemInterface::GetInstance()->GetSystemLoadInformation(collectContext.GetMetricTime(), load)) {
        return false;
    }

    mCalculate.AddValue(load.systemStat);

    // If group is not provided, just collect data without generating metrics
    if (!groupPtr) {
        return true;
    }

    SystemStat minSys, maxSys, avgSys;
    mCalculate.Stat(maxSys, minSys, avgSys);
    mCalculate.Reset();

    // 数据整理
    struct MetricDef {
        StringView name;
        double* value;
    } metrics[] = {
        {KEY_LOAD_1M_MIN, &minSys.load1},
        {KEY_LOAD_1M_MAX, &maxSys.load1},
        {KEY_LOAD_1M_AVG, &avgSys.load1},
        {KEY_LOAD_5M_MIN, &minSys.load5},
        {KEY_LOAD_5M_MAX, &maxSys.load5},
        {KEY_LOAD_5M_AVG, &avgSys.load5},
        {KEY_LOAD_15M_MIN, &minSys.load15},
        {KEY_LOAD_15M_MAX, &maxSys.load15},
        {KEY_LOAD_15M_AVG, &avgSys.load15},
        {KEY_LOAD_PER_CORE_1M_MIN, &minSys.load1PerCore},
        {KEY_LOAD_PER_CORE_1M_MAX, &maxSys.load1PerCore},
        {KEY_LOAD_PER_CORE_1M_AVG, &avgSys.load1PerCore},
        {KEY_LOAD_PER_CORE_5M_MIN, &minSys.load5PerCore},
        {KEY_LOAD_PER_CORE_5M_MAX, &maxSys.load5PerCore},
        {KEY_LOAD_PER_CORE_5M_AVG, &avgSys.load5PerCore},
        {KEY_LOAD_PER_CORE_15M_MIN, &minSys.load15PerCore},
        {KEY_LOAD_PER_CORE_15M_MAX, &maxSys.load15PerCore},
        {KEY_LOAD_PER_CORE_15M_AVG, &avgSys.load15PerCore},
    };

    MetricEvent* metricEvent = groupPtr->AddMetricEvent(true);
    if (!metricEvent) {
        return false;
    }
    metricEvent->SetTimestamp(load.collectTime, 0);
    metricEvent->SetValue<UntypedMultiDoubleValues>(metricEvent);
    metricEvent->SetTagNoCopy(TAG_KEY_M, METRIC_SYSTEM_LOAD);
    auto* multiDoubleValues = metricEvent->MutableValue<UntypedMultiDoubleValues>();
    for (const auto& def : metrics) {
        multiDoubleValues->SetValue(def.name,
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, *def.value});
    }

    return true;
}

const std::chrono::seconds SystemCollector::GetCollectInterval() const {
    return std::chrono::seconds(INT32_FLAG(basic_host_monitor_system_collect_interval));
}

} // namespace logtail

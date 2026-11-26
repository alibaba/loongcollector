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

#include "host_monitor/collector/DentryCollector.h"

#include "MetricValue.h"
#include "common/Flags.h"
#include "host_monitor/Constants.h"
#include "host_monitor/SystemInterface.h"
#include "logger/Logger.h"

DEFINE_FLAG_INT32(basic_host_monitor_dentry_collect_interval, "basic host monitor dentry collect interval, seconds", 5);

namespace logtail {

const std::string DentryCollector::sName = "dentry";

DentryCollector::DentryCollector() {
}

bool DentryCollector::Init(HostMonitorContext& collectContext) {
    return BaseCollector::Init(collectContext);
}

const std::chrono::seconds DentryCollector::GetCollectInterval() const {
    return std::chrono::seconds(INT32_FLAG(basic_host_monitor_dentry_collect_interval));
}

bool DentryCollector::Collect(HostMonitorContext& collectContext, PipelineEventGroup* group) {
    if (group == nullptr) {
        return true;
    }

    DentryStatInformation info;
    if (!SystemInterface::GetInstance()->GetDentryStatInformation(info)) {
        LOG_ERROR(sLogger, ("dentry collector", "GetDentryStatInformation failed"));
        return false;
    }

    const time_t now = time(nullptr);

    MetricEvent* metricEvent = group->AddMetricEvent(true);
    if (!metricEvent) {
        LOG_ERROR(sLogger, ("dentry collector", "metricEvent is nullptr"));
        return false;
    }
    metricEvent->SetTimestamp(now, 0);
    metricEvent->SetValue<UntypedMultiDoubleValues>(metricEvent);
    auto* multiDoubleValues = metricEvent->MutableValue<UntypedMultiDoubleValues>();
    multiDoubleValues->SetValue(
        std::string("dentry_nr"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.nrDentry)});
    multiDoubleValues->SetValue(
        std::string("dentry_unused"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.nrUnused)});
    multiDoubleValues->SetValue(
        std::string("dentry_age_limit"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.ageLimit)});
    multiDoubleValues->SetValue(
        std::string("dentry_want_pages"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.wantPages)});
    multiDoubleValues->SetValue(
        std::string("dentry_negative"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.nrNegative)});

    metricEvent->SetTag(std::string("m"), std::string("system.dentry"));
    return true;
}

} // namespace logtail

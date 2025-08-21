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

#include "host_monitor/collector/CgroupCollector.h"

#include "MetricValue.h"
#include "common/StringTools.h"
#include "host_monitor/Constants.h"
#include "host_monitor/LinuxSystemInterface.h"
#include "host_monitor/SystemInterface.h"


namespace logtail {

const std::string CgroupCollector::sName = "cgroup";

CgroupCollector::CgroupCollector() {
    Init();
}

int CgroupCollector::Init(int totalCount) {
    return 0;
}

bool CgroupCollector::Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) {
    if (group == nullptr) {
        std::cout << "group is nullptr" << std::endl;
        return false;
    }

    CgroupStatInformation info;
    if (!SystemInterface::GetInstance()->GetCgroupStatInformation(info)) {
        std::cout << "GetCgroupStatInformation failed" << std::endl;
        return false;
    }

    const time_t now = time(nullptr);

    MetricEvent* metricEvent = group->AddMetricEvent(true);
    if (!metricEvent) {
        std::cout << "metricEvent is nullptr" << std::endl;
        return false;
    }
    metricEvent->SetTimestamp(now, 0);
    metricEvent->SetValue<UntypedMultiDoubleValues>(metricEvent);
    auto* multiDoubleValues = metricEvent->MutableValue<UntypedMultiDoubleValues>();
    multiDoubleValues->SetValue(
        std::string("cgroup_cpuset"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.cpuset)});
    multiDoubleValues->SetValue(
        std::string("cgroup_cpu"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.cpu)});
    multiDoubleValues->SetValue(
        std::string("cgroup_cpuacct"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.cpuacct)});
    multiDoubleValues->SetValue(
        std::string("cgroup_blkio"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.blkio)});
    multiDoubleValues->SetValue(
        std::string("cgroup_memory"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.memory)});
    multiDoubleValues->SetValue(
        std::string("cgroup_devices"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.devices)});
    multiDoubleValues->SetValue(
        std::string("cgroup_freezer"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.freezer)});
    multiDoubleValues->SetValue(
        std::string("cgroup_net_cls"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.net_cls)});
    multiDoubleValues->SetValue(
        std::string("cgroup_perf_event"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.perf_event)});
    multiDoubleValues->SetValue(
        std::string("cgroup_net_prio"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.net_prio)});
    multiDoubleValues->SetValue(
        std::string("cgroup_hugetlb"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.hugetlb)});
    multiDoubleValues->SetValue(
        std::string("cgroup_pids"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.pids)});
    multiDoubleValues->SetValue(
        std::string("cgroup_ioasids"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.ioasids)});
    multiDoubleValues->SetValue(
        std::string("cgroup_rdma"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.stat.rdma)});

    metricEvent->SetTag(std::string("m"), std::string("system.cgroup"));
    return true;
}

} // namespace logtail

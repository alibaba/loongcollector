/*
 * Copyright 2024 iLogtail Authors
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

#include "host_monitor/collector/CollectorMetrics.h"

#include "host_monitor/collector/CPUCollector.h"
#include "host_monitor/collector/DiskCollector.h"
#include "host_monitor/collector/MemCollector.h"
#include "host_monitor/collector/NetCollector.h"
#include "host_monitor/collector/ProcessCollector.h"
#include "host_monitor/collector/SystemCollector.h"
#include "monitor/MetricManager.h"
#include "monitor/metric_constants/MetricConstants.h"

namespace logtail {

void CollectorMetrics::Init() {
    MetricLabels labels;
    WriteMetrics::GetInstance()->CreateMetricsRecordRef(
        mMetricsRecordRef, MetricCategory::METRIC_CATEGORY_PLUGIN_SOURCE, std::move(labels));

    // Initialize fail counters for each collector type
    mFailCounters[CPUCollector::sName] = mMetricsRecordRef.CreateCounter(METRIC_PLUGIN_CPU_FAIL_TOTAL);
    mFailCounters[SystemCollector::sName] = mMetricsRecordRef.CreateCounter(METRIC_PLUGIN_SYSTEM_FAIL_TOTAL);
    mFailCounters[MemCollector::sName] = mMetricsRecordRef.CreateCounter(METRIC_PLUGIN_MEM_FAIL_TOTAL);
    mFailCounters[NetCollector::sName] = mMetricsRecordRef.CreateCounter(METRIC_PLUGIN_NET_FAIL_TOTAL);
    mFailCounters[ProcessCollector::sName] = mMetricsRecordRef.CreateCounter(METRIC_PLUGIN_PROCESS_FAIL_TOTAL);
    mFailCounters[DiskCollector::sName] = mMetricsRecordRef.CreateCounter(METRIC_PLUGIN_DISK_FAIL_TOTAL);

    WriteMetrics::GetInstance()->CommitMetricsRecordRef(mMetricsRecordRef);
}

void CollectorMetrics::UpdateFailMetrics(const std::string& collectorType) {
    auto it = mFailCounters.find(collectorType);
    if (it != mFailCounters.end() && it->second) {
        it->second->Add(1);
    }
}

} // namespace logtail

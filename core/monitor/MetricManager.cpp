// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "MetricManager.h"

#include <cmath>

#include <limits>
#include <set>

#include "Monitor.h"
#include "app_config/AppConfig.h"
#include "go_pipeline/LogtailPlugin.h"
#include "google/protobuf/arena.h"
#include "logger/Logger.h"
#include "protobuf/models/pipeline_event_group.pb.h"
#include "provider/Provider.h"


using namespace sls_logs;
using namespace std;

namespace logtail {

const string METRIC_EXPORT_TYPE_GO = "direct";
const string METRIC_EXPORT_TYPE_CPP = "cpp_provided";

WriteMetrics::~WriteMetrics() {
    Clear();
}

void WriteMetrics::CreateMetricsRecordRef(MetricsRecordRef& ref,
                                          const std::string& category,
                                          MetricLabels&& labels,
                                          DynamicMetricLabels&& dynamicLabels) {
    MetricsRecord* cur = new MetricsRecord(
        category, std::make_shared<MetricLabels>(labels), std::make_shared<DynamicMetricLabels>(dynamicLabels));
    ref.SetMetricsRecord(cur);
}

void WriteMetrics::CommitMetricsRecordRef(MetricsRecordRef& ref) {
    std::lock_guard<std::mutex> lock(mMutex);
    ref.mMetrics->SetNext(mHead);
    mHead = ref.mMetrics;
    ref.mMetrics->MarkCommitted();
}

MetricsRecord* WriteMetrics::GetHead() {
    std::lock_guard<std::mutex> lock(mMutex);
    return mHead;
}

void WriteMetrics::Clear() {
    std::lock_guard<std::mutex> lock(mMutex);
    while (mHead) {
        MetricsRecord* toDeleted = mHead;
        mHead = mHead->GetNext();
        delete toDeleted;
    }
}

MetricsRecord* WriteMetrics::DoSnapshot() {
    // new read head
    MetricsRecord* snapshot = nullptr;
    MetricsRecord* toDeleteHead = nullptr;
    MetricsRecord* tmp = nullptr;

    MetricsRecord emptyHead;
    MetricsRecord* preTmp = nullptr;

    int writeMetricsTotal = 0;
    int writeMetricsDeleteTotal = 0;
    int metricsSnapshotTotal = 0;

    // find the first undeleted node and set as new mHead
    {
        std::lock_guard<std::mutex> lock(mMutex);
        emptyHead.SetNext(mHead);
        preTmp = &emptyHead;
        tmp = preTmp->GetNext();
        bool findHead = false;
        while (tmp) {
            if (tmp->IsDeleted()) {
                preTmp->SetNext(tmp->GetNext());
                tmp->SetNext(toDeleteHead);
                toDeleteHead = tmp;
                tmp = preTmp->GetNext();
                writeMetricsTotal++;
            } else {
                // find head
                mHead = tmp;
                preTmp = mHead;
                tmp = tmp->GetNext();
                findHead = true;
                break;
            }
        }
        // if no undeleted node, set null to mHead
        if (!findHead) {
            mHead = nullptr;
            preTmp = mHead;
        }
    }

    // copy head
    if (preTmp) {
        MetricsRecord* newMetrics = preTmp->Collect();
        newMetrics->SetNext(snapshot);
        snapshot = newMetrics;
        metricsSnapshotTotal++;
        writeMetricsTotal++;
    }

    while (tmp) {
        writeMetricsTotal++;
        if (tmp->IsDeleted()) {
            preTmp->SetNext(tmp->GetNext());
            tmp->SetNext(toDeleteHead);
            toDeleteHead = tmp;
            tmp = preTmp->GetNext();
        } else {
            MetricsRecord* newMetrics = tmp->Collect();
            newMetrics->SetNext(snapshot);
            snapshot = newMetrics;
            preTmp = tmp;
            tmp = tmp->GetNext();
            metricsSnapshotTotal++;
        }
    }

    while (toDeleteHead) {
        MetricsRecord* toDelete = toDeleteHead;
        toDeleteHead = toDeleteHead->GetNext();
        delete toDelete;
        writeMetricsDeleteTotal++;
    }
    LOG_INFO(sLogger,
             ("writeMetricsTotal", writeMetricsTotal)("writeMetricsDeleteTotal", writeMetricsDeleteTotal)(
                 "metricsSnapshotTotal", metricsSnapshotTotal));
    return snapshot;
}

ReadMetrics::~ReadMetrics() {
    Clear();
}

void ReadMetrics::ReadAsSelfMonitorMetricEvents(std::vector<SelfMonitorMetricEvent>& metricEventList) const {
    ReadLock lock(mReadWriteLock);
    // c++ metrics
    MetricsRecord* tmp = mHead;
    while (tmp) {
        metricEventList.emplace_back(tmp);
        tmp = tmp->GetNext();
    }
    // go metrics
    // Readers need an independent snapshot, but no longer reconstruct it from JSON.
    metricEventList.insert(metricEventList.end(), mGoMetrics.begin(), mGoMetrics.end());
}

void ReadMetrics::UpdateMetrics() {
    // go指标在Cpp指标前获取，是为了在 Cpp 部分指标做 SnapShot
    // 前（即调用 ReadMetrics::GetInstance()->UpdateMetrics() 函数），把go部分的进程级指标填写到 Cpp
    // 的进程级指标中去，随Cpp的进程级指标一起输出
    if (LogtailPlugin::GetInstance()->IsPluginOpened()) {
        vector<SelfMonitorMetricEvent> goCppProvidedMetircsList;
        LogtailPlugin::GetInstance()->GetGoSelfMonitorMetrics(goCppProvidedMetircsList, METRIC_EXPORT_TYPE_CPP);
        UpdateGoCppProvidedMetrics(goCppProvidedMetircsList);

        vector<SelfMonitorMetricEvent> goMetrics;
        LogtailPlugin::GetInstance()->GetGoSelfMonitorMetrics(goMetrics, METRIC_EXPORT_TYPE_GO);
        {
            WriteLock lock(mReadWriteLock);
            mGoMetrics.swap(goMetrics);
        }
        // Release the old snapshot outside the reader lock.
    }
    // 获取c++指标
    MetricsRecord* snapshot = WriteMetrics::GetInstance()->DoSnapshot();
    MetricsRecord* toDelete;
    {
        // Only lock when change head
        WriteLock lock(mReadWriteLock);
        toDelete = mHead;
        mHead = snapshot;
    }
    // delete old linklist
    while (toDelete) {
        MetricsRecord* obj = toDelete;
        toDelete = toDelete->GetNext();
        delete obj;
    }
}

MetricsRecord* ReadMetrics::GetHead() {
    WriteLock lock(mReadWriteLock);
    return mHead;
}

void ReadMetrics::Clear() {
    WriteLock lock(mReadWriteLock);
    while (mHead) {
        MetricsRecord* toDelete = mHead;
        mHead = mHead->GetNext();
        delete toDelete;
    }
}

// Process gauges already have numeric PB values; update the C++ owner directly.
void ReadMetrics::UpdateGoCppProvidedMetrics(const vector<SelfMonitorMetricEvent>& metricsList) {
    for (const auto& metrics : metricsList) {
        for (const auto& metric : metrics.GetGauges()) {
            if (!std::isfinite(metric.second) || metric.second < 0
                || metric.second >= static_cast<double>(std::numeric_limits<uint64_t>::max())) {
                continue;
            }
            if (metric.first == METRIC_AGENT_MEMORY_GO) {
                LoongCollectorMonitor::GetInstance()->SetAgentGoMemory(static_cast<uint64_t>(metric.second));
            } else if (metric.first == METRIC_AGENT_GO_ROUTINES_TOTAL) {
                LoongCollectorMonitor::GetInstance()->SetAgentGoRoutinesTotal(static_cast<uint64_t>(metric.second));
            }
        }
    }
}

bool ReadMetrics::ParseGoMetricsPB(std::string_view pbData, std::vector<SelfMonitorMetricEvent>& metricsList) {
    if (pbData.empty()) {
        return true;
    }
    if (pbData.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    // PB allocations share one arena lifetime; only the final snapshot owns copies.
    google::protobuf::Arena arena;
    auto* pbGroup = models::PipelineEventGroup::default_instance().New(&arena);
    if (!pbGroup->ParseFromArray(pbData.data(), static_cast<int>(pbData.size()))) {
        LOG_WARNING(sLogger, ("receive go metrics pb", "failed to parse protobuf")("size", pbData.size()));
        return false;
    }
    if (!pbGroup->has_metrics()) {
        // Non-metric groups were ignored by the old self-monitor conversion.
        return pbGroup->logs().events_size() > 0 || pbGroup->spans().events_size() > 0;
    }
    if (pbGroup->metrics().events_size() == 0) {
        return false;
    }
    // Validate the whole batch before appending, retaining failure atomicity.
    for (const auto& metric : pbGroup->metrics().events()) {
        if (!metric.has_untypedsinglevalue() && !metric.has_untypedmultidoublevalues()) {
            return false;
        }
    }
    metricsList.reserve(metricsList.size() + pbGroup->metrics().events_size());
    for (const auto& metric : pbGroup->metrics().events()) {
        metricsList.emplace_back(metric);
    }
    return true;
}

} // namespace logtail

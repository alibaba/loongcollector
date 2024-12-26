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

#include <set>

#include "Monitor.h"
#include "app_config/AppConfig.h"
#include "go_pipeline/LogtailPlugin.h"
#include "logger/Logger.h"
#include "provider/Provider.h"


using namespace sls_logs;
using namespace std;

namespace logtail {

const string METRIC_EXPORT_TYPE_GO = "direct";
const string METRIC_EXPORT_TYPE_CPP = "cpp_provided";

WriteMetrics::~WriteMetrics() {
    Clear();
}

void WriteMetrics::PrepareMetricsRecordRef(MetricsRecordRef& ref,
                                           const std::string& category,
                                           MetricLabels&& labels,
                                           DynamicMetricLabels&& dynamicLabels) {
    CreateMetricsRecordRef(ref, category, std::move(labels), std::move(dynamicLabels));
    CommitMetricsRecordRef(ref);
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
        metricEventList.emplace_back(SelfMonitorMetricEvent(tmp));
        tmp = tmp->GetNext();
    }
    // go metrics
    for (auto metrics : mGoMetrics) {
        metricEventList.emplace_back(SelfMonitorMetricEvent(move(metrics)));
    }
}

void ReadMetrics::UpdateMetrics() {
    // go指标在Cpp指标前获取，是为了在 Cpp 部分指标做 SnapShot
    // 前（即调用 ReadMetrics::GetInstance()->UpdateMetrics() 函数），把go部分的进程级指标填写到 Cpp
    // 的进程级指标中去，随Cpp的进程级指标一起输出
    if (LogtailPlugin::GetInstance()->IsPluginOpened()) {
        vector<map<string, string>> goCppProvidedMetircsList;
        LogtailPlugin::GetInstance()->GetGoMetrics(goCppProvidedMetircsList, METRIC_EXPORT_TYPE_CPP);
        UpdateGoCppProvidedMetrics(goCppProvidedMetircsList);

        {
            WriteLock lock(mReadWriteLock);
            mGoMetrics.clear();
            LogtailPlugin::GetInstance()->GetGoMetrics(mGoMetrics, METRIC_EXPORT_TYPE_GO);
        }
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

// metrics from Go that are provided by cpp
void ReadMetrics::UpdateGoCppProvidedMetrics(vector<map<string, string>>& metricsList) {
    if (metricsList.size() == 0) {
        return;
    }

    for (auto metrics : metricsList) {
        for (auto metric : metrics) {
            if (metric.first == METRIC_AGENT_MEMORY_GO) {
                LoongCollectorMonitor::GetInstance()->SetAgentGoMemory(stoi(metric.second));
            }
            if (metric.first == METRIC_AGENT_GO_ROUTINES_TOTAL) {
                LoongCollectorMonitor::GetInstance()->SetAgentGoRoutinesTotal(stoi(metric.second));
            }
        }
    }
}

} // namespace logtail

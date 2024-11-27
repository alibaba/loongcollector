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
#include "common/HashUtil.h"
#include "common/JsonUtil.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "go_pipeline/LogtailPlugin.h"
#include "logger/Logger.h"
#include "provider/Provider.h"


using namespace sls_logs;
using namespace std;

namespace logtail {

const string METRIC_KEY_CATEGORY = "category";
const string METRIC_KEY_LABEL = "label";
const string METRIC_TOPIC_TYPE = "loongcollector_metric";
const string METRIC_EXPORT_TYPE_GO = "direct";
const string METRIC_EXPORT_TYPE_CPP = "cpp_provided";
const string METRIC_GO_KEY_LABELS = "labels";
const string METRIC_GO_KEY_COUNTERS = "counters";
const string METRIC_GO_KEY_GAUGES = "gauges";

SelfMonitorMetricEvent::SelfMonitorMetricEvent() {
}

SelfMonitorMetricEvent::SelfMonitorMetricEvent(MetricsRecord* metricRecord) {
    // category
    mCategory = metricRecord->GetCategory();
    // labels
    for (auto item = metricRecord->GetLabels()->begin(); item != metricRecord->GetLabels()->end(); ++item) {
        pair<string, string> pair = *item;
        mLabels[pair.first] = pair.second;
    }
    for (auto item = metricRecord->GetDynamicLabels()->begin(); item != metricRecord->GetDynamicLabels()->end();
         ++item) {
        pair<string, function<string()>> pair = *item;
        string value = pair.second();
        mLabels[pair.first] = value;
    }
    // counters
    for (auto& item : metricRecord->GetCounters()) {
        mCounters[item->GetName()] = item->GetValue();
    }
    for (auto& item : metricRecord->GetTimeCounters()) {
        mCounters[item->GetName()] = item->GetValue();
    }
    // gauges
    for (auto& item : metricRecord->GetIntGauges()) {
        mGauges[item->GetName()] = item->GetValue();
    }
    for (auto& item : metricRecord->GetDoubleGauges()) {
        mGauges[item->GetName()] = item->GetValue();
    }
    CreateKey();
}

SelfMonitorMetricEvent::SelfMonitorMetricEvent(const std::map<std::string, std::string>& metricRecord) {
    Json::Value labels, counters, gauges;
    string errMsg;
    ParseJsonTable(metricRecord.at(METRIC_GO_KEY_LABELS), labels, errMsg);
    ParseJsonTable(metricRecord.at(METRIC_GO_KEY_COUNTERS), counters, errMsg);
    ParseJsonTable(metricRecord.at(METRIC_GO_KEY_GAUGES), gauges, errMsg);
    // category
    if (labels.isMember("metric_category")) {
        mCategory = labels["metric_category"].asString();
        labels.removeMember("metric_category");
    } else {
        mCategory = MetricCategory::METRIC_CATEGORY_UNKNOWN;
        LOG_ERROR(sLogger, ("parse go metric", "labels")("err", "metric_category not found"));
    }
    // labels
    for (Json::Value::const_iterator itr = labels.begin(); itr != labels.end(); ++itr) {
        if (itr->isString()) {
            mLabels[itr.key().asString()] = itr->asString();
        }
    }
    // counters
    for (Json::Value::const_iterator itr = counters.begin(); itr != counters.end(); ++itr) {
        if (itr->isUInt64()) {
            mCounters[itr.key().asString()] = itr->asUInt64();
        }
        if (itr->isDouble()) {
            mCounters[itr.key().asString()] = static_cast<uint64_t>(itr->asDouble());
        }
        if (itr->isString()) {
            try {
                mCounters[itr.key().asString()] = static_cast<uint64_t>(std::stod(itr->asString()));
            } catch (...) {
                mCounters[itr.key().asString()] = 0;
            }
        }
    }
    // gauges
    for (Json::Value::const_iterator itr = gauges.begin(); itr != gauges.end(); ++itr) {
        if (itr->isDouble()) {
            mGauges[itr.key().asString()] = itr->asDouble();
        }
        if (itr->isString()) {
            try {
                double value = std::stod(itr->asString());
                mGauges[itr.key().asString()] = value;
            } catch (...) {
                mGauges[itr.key().asString()] = 0;
            }
        }
    }
    CreateKey();
}

void SelfMonitorMetricEvent::CreateKey() {
    string key = "category:" + mCategory;
    for (auto label : mLabels) {
        key += (";" + label.first + ":" + label.second);
    }
    mKey = HashString(key);
    mUpdatedFlag = true;
}

void SelfMonitorMetricEvent::SetInterval(size_t interval) {
    mLastSendInterval = 0;
    mSendInterval = interval;
}

void SelfMonitorMetricEvent::Merge(SelfMonitorMetricEvent& event) {
    if (mSendInterval != event.mSendInterval) {
        mSendInterval = event.mSendInterval;
        mLastSendInterval = 0;
    }
    for (auto counter = event.mCounters.begin(); counter != event.mCounters.end(); counter++) {
        if (mCounters.find(counter->first) != mCounters.end())
            mCounters[counter->first] += counter->second;
        else
            mCounters[counter->first] = counter->second;
    }
    for (auto gauge = event.mGauges.begin(); gauge != event.mGauges.end(); gauge++) {
        mGauges[gauge->first] = gauge->second;
    }
    mUpdatedFlag = true;
}

bool SelfMonitorMetricEvent::ShouldSend() {
    mLastSendInterval++;
    return (mLastSendInterval >= mSendInterval) && mUpdatedFlag;
}

bool SelfMonitorMetricEvent::ShouldDelete() {
    return (mLastSendInterval >= mSendInterval) && !mUpdatedFlag;
}

void SelfMonitorMetricEvent::ReadAsMetricEvent(MetricEvent* metricEventPtr) {
    // time
    metricEventPtr->SetTimestamp(GetCurrentLogtailTime().tv_sec);
    // __tag__
    for (auto label = mLabels.begin(); label != mLabels.end(); label++) {
        metricEventPtr->SetTag(label->first, label->second);
    }
    // name
    metricEventPtr->SetName(mCategory);
    // values
    metricEventPtr->SetValue({});
    for (auto counter = mCounters.begin(); counter != mCounters.end(); counter++) {
        metricEventPtr->MutableValue<UntypedMultiDoubleValues>()->SetValue(counter->first, counter->second);
        counter->second = 0;
    }
    for (auto gauge = mGauges.begin(); gauge != mGauges.end(); gauge++) {
        metricEventPtr->MutableValue<UntypedMultiDoubleValues>()->SetValue(gauge->first, gauge->second);
    }
    // set flags
    mLastSendInterval = 0;
    mUpdatedFlag = false;
}

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
            LogtailMonitor::GetInstance()->UpdateMetric(metric.first, metric.second);
        }
    }
}

} // namespace logtail

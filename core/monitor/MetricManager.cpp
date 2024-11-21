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

set<string> GetRegionsByProjects(string projects);
string GenJsonString(Json::Value jsonValue);

SelfMonitorMetricEvent::SelfMonitorMetricEvent() {
}

SelfMonitorMetricEvent::SelfMonitorMetricEvent(MetricsRecord* metricRecord) {
    // category
    mCategory = metricRecord->GetCategory();
    // labels
    string projects;
    for (auto item = metricRecord->GetLabels()->begin(); item != metricRecord->GetLabels()->end(); ++item) {
        pair<string, string> pair = *item;
        mLabels[pair.first] = pair.second;
        if (pair.first == METRIC_LABEL_KEY_PROJECT) {
            projects = pair.second;
        }
    }
    for (auto item = metricRecord->GetDynamicLabels()->begin(); item != metricRecord->GetDynamicLabels()->end();
         ++item) {
        pair<string, function<string()>> pair = *item;
        string value = pair.second();
        mLabels[pair.first] = value;
        if (pair.first == METRIC_LABEL_KEY_PROJECT) {
            projects = value;
        }
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
    // others
    mRegions = GetRegionsByProjects(projects);
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
    // regions
    if (labels.isMember(METRIC_LABEL_KEY_PROJECT)) {
        mRegions = GetRegionsByProjects(labels[METRIC_LABEL_KEY_PROJECT].asString());
    } else {
        mRegions = GetRegionsByProjects("");
        LOG_ERROR(sLogger, ("parse go metric", "labels")("err", "project not found"));
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

void SelfMonitorMetricEvent::SetTarget(SelfMonitorMetricRule::SelfMonitorMetricRuleTarget target) {
    mTarget = target;
}

void SelfMonitorMetricEvent::Merge(SelfMonitorMetricEvent& event) {
    mTarget = event.mTarget;
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

void SelfMonitorMetricEvent::Collect() {
    auto now = GetCurrentLogtailTime();
    mTmpCollectTime = AppConfig::GetInstance()->EnableLogTimeAutoAdjust() ? now.tv_sec + GetTimeDelta() : now.tv_sec;

    mTmpCollectContents.clear();
    mTmpCollectContents[METRIC_KEY_CATEGORY] = mCategory;
    Json::Value metricsRecordLabel;
    for (auto label = mLabels.begin(); label != mLabels.end(); label++) {
        metricsRecordLabel[label->first] = label->second;
    }
    mTmpCollectContents[METRIC_KEY_LABEL] = GenJsonString(metricsRecordLabel);
    for (auto counter = mCounters.begin(); counter != mCounters.end(); counter++) {
        mTmpCollectContents[counter->first] = ToString(counter->second);
        counter->second = 0;
    }
    for (auto gauge = mGauges.begin(); gauge != mGauges.end(); gauge++) {
        mTmpCollectContents[gauge->first] = ToString(gauge->second);
    }
    mLastSendInterval = 0;
    mUpdatedFlag = false;
}

std::set<std::string> SelfMonitorMetricEvent::GetTargets() {
    set<string> targets;
    if (mTarget == SelfMonitorMetricRule::SelfMonitorMetricRuleTarget::LOCAL_FILE) {
        targets.emplace("local_file");
    }
    if (mTarget == SelfMonitorMetricRule::SelfMonitorMetricRuleTarget::SLS_SHENNONG) {
        for (const auto& region : mRegions) {
            targets.emplace("sls_shennong_" + region);
        }
    }
    if (mTarget == SelfMonitorMetricRule::SelfMonitorMetricRuleTarget::SLS_STATUS) {
        for (const auto& region : mRegions) {
            targets.emplace("sls_status_" + region);
        }
    }
    return targets;
}

void SelfMonitorMetricEvent::ReadAsLogEvent(LogEvent* logEventPtr) {
    logEventPtr->SetTimestamp(mTmpCollectTime);
    for (auto it = mTmpCollectContents.begin(); it != mTmpCollectContents.end(); it++) {
        logEventPtr->SetContent(it->first, it->second);
    }
}

WriteMetrics::~WriteMetrics() {
    Clear();
}

void WriteMetrics::PrepareMetricsRecordRef(MetricsRecordRef& ref,
                                           const string& category,
                                           MetricLabels&& labels,
                                           DynamicMetricLabels&& dynamicLabels) {
    CreateMetricsRecordRef(ref, category, move(labels), move(dynamicLabels));
    CommitMetricsRecordRef(ref);
}

void WriteMetrics::CreateMetricsRecordRef(MetricsRecordRef& ref,
                                          const string& category,
                                          MetricLabels&& labels,
                                          DynamicMetricLabels&& dynamicLabels) {
    MetricsRecord* cur = new MetricsRecord(
        category, make_shared<MetricLabels>(labels), make_shared<DynamicMetricLabels>(dynamicLabels));
    ref.SetMetricsRecord(cur);
}

void WriteMetrics::CommitMetricsRecordRef(MetricsRecordRef& ref) {
    lock_guard<mutex> lock(mMutex);
    ref.mMetrics->SetNext(mHead);
    mHead = ref.mMetrics;
}

MetricsRecord* WriteMetrics::GetHead() {
    lock_guard<mutex> lock(mMutex);
    return mHead;
}

void WriteMetrics::Clear() {
    lock_guard<mutex> lock(mMutex);
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
        lock_guard<mutex> lock(mMutex);
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

void ReadMetrics::ReadAsMetricEvents(std::vector<SelfMonitorMetricEvent>& metricEventList) const {
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

set<string> GetRegionsByProjects(string projects) {
    stringstream ss(projects);
    set<string> regions;
    string project;
    while (getline(ss, project, ' ')) {
        regions.insert(FlusherSLS::GetProjectRegion(project));
    }
    if (regions.empty()) {
        regions.insert(GetProfileSender()->GetDefaultProfileRegion());
    }
    return regions;
}

string GenJsonString(Json::Value jsonValue) {
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    return Json::writeString(writer, jsonValue);
}

} // namespace logtail

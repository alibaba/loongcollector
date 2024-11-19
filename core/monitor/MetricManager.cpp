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
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "go_pipeline/LogtailPlugin.h"
#include "logger/Logger.h"
#include "provider/Provider.h"


using namespace sls_logs;
using namespace std;

namespace logtail {

const string METRIC_SLS_LOGSTORE_NAME = "shennong_log_profile";
const string METRIC_TOPIC_TYPE = "loongcollector_metric";
const string METRIC_EXPORT_TYPE_GO = "direct";
const string METRIC_EXPORT_TYPE_CPP = "cpp_provided";

Log* CreateLogPtr(map<string, sls_logs::LogGroup*>& logGroupMap, const string& region);
string FindProjectFromMetricRecord(const MetricsRecord* tmp);
string FindProjectFromGoMetricRecord(const map<string, string>& metric);
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
    Json::Value metricsRecordLabel;
    for (const auto& metric : metricRecord) {
        // category
        if (metric.first.compare("label.metric_category") == 0) {
            mCategory = metric.second;
            continue;
        }
        // label
        if (metric.first.compare(0, METRIC_KEY_LABEL.length(), METRIC_KEY_LABEL) == 0) {
            mLabels[metric.first.substr(METRIC_KEY_LABEL.length() + 1)] = metric.second;
            if (metric.first == METRIC_KEY_LABEL + "." + METRIC_LABEL_KEY_PROJECT) {
                mRegions = GetRegionsByProjects(metric.second);
            }
            continue;
        }
        // value
        // go指标目前无法区分是Counter还是Gauge，这里统计数据无法操作，按gauge来计算，且更新间隔恒为1min
        if (metric.first.compare(0, METRIC_KEY_VALUE.length(), METRIC_KEY_VALUE) == 0) {
            try {
                double value = std::stod(metric.second);
                mGauges[metric.first.substr(METRIC_KEY_VALUE.length() + 1)] = value;
            } catch (...) {
                mGauges[metric.first.substr(METRIC_KEY_VALUE.length() + 1)] = 0;
            }

            continue;
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
    mSendInterval = event.mSendInterval;
    for (auto counter = event.mCounters.begin(); counter != event.mCounters.end(); counter++) {
        if (mCounters.find(counter->first) != mCounters.end())
            mCounters[counter->first] += counter->second;
        else
            mCounters[counter->first] = counter->second;
    }
    for (auto gauge = event.mGauges.begin(); gauge != event.mGauges.end(); gauge++) {
        mGauges[gauge->first] = gauge->second;
    }
}

bool SelfMonitorMetricEvent::ShouldSend() {
    mLastSendInterval++;
    return mLastSendInterval >= mSendInterval;
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
    for (const auto& metrics : mGoMetrics) {
        metricEventList.emplace_back(SelfMonitorMetricEvent(metrics));
    }
}

void ReadMetrics::ReadAsLogGroup(map<string, sls_logs::LogGroup*>& logGroupMap) const {
    ReadLock lock(mReadWriteLock);

    // c++ metrics
    MetricsRecord* tmp = mHead;
    while (tmp) {
        // get region by project
        set<string> regions = GetRegionsByProjects(FindProjectFromMetricRecord(tmp));
        if (tmp->GetCategory() == MetricCategory::METRIC_CATEGORY_AGENT) {
            regions.insert(GetProfileSender()->GetDefaultProfileRegion());
        }

        Log* logBasePtr = nullptr;
        for (const auto& region : regions) {
            // create logPtr
            Log* logPtr = CreateLogPtr(logGroupMap, region);
            if (logBasePtr == nullptr) {
                // set time
                auto now = GetCurrentLogtailTime();
                SetLogTime(logPtr,
                           AppConfig::GetInstance()->EnableLogTimeAutoAdjust() ? now.tv_sec + GetTimeDelta()
                                                                               : now.tv_sec);
                // set content
                { // category
                    Log_Content* contentPtr = logPtr->add_contents();
                    contentPtr->set_key(METRIC_KEY_CATEGORY);
                    contentPtr->set_value(tmp->GetCategory());
                }
                { // label
                    Json::Value metricsRecordLabel;
                    for (auto item = tmp->GetLabels()->begin(); item != tmp->GetLabels()->end(); ++item) {
                        pair<string, string> pair = *item;
                        metricsRecordLabel[pair.first] = pair.second;
                    }
                    for (auto item = tmp->GetDynamicLabels()->begin(); item != tmp->GetDynamicLabels()->end(); ++item) {
                        pair<string, function<string()>> pair = *item;
                        metricsRecordLabel[pair.first] = pair.second();
                    }
                    Log_Content* contentPtr = logPtr->add_contents();
                    contentPtr->set_key(METRIC_KEY_LABEL);
                    contentPtr->set_value(GenJsonString(metricsRecordLabel));
                }
                { // value
                    for (auto& item : tmp->GetCounters()) {
                        CounterPtr counter = item;
                        Log_Content* contentPtr = logPtr->add_contents();
                        contentPtr->set_key(counter->GetName());
                        contentPtr->set_value(ToString(counter->GetValue()));
                    }
                    for (auto& item : tmp->GetTimeCounters()) {
                        TimeCounterPtr counter = item;
                        Log_Content* contentPtr = logPtr->add_contents();
                        contentPtr->set_key(counter->GetName());
                        contentPtr->set_value(ToString(counter->GetValue()));
                    }
                    for (auto& item : tmp->GetIntGauges()) {
                        IntGaugePtr gauge = item;
                        Log_Content* contentPtr = logPtr->add_contents();
                        contentPtr->set_key(gauge->GetName());
                        contentPtr->set_value(ToString(gauge->GetValue()));
                    }
                    for (auto& item : tmp->GetDoubleGauges()) {
                        DoubleGaugePtr gauge = item;
                        Log_Content* contentPtr = logPtr->add_contents();
                        contentPtr->set_key(gauge->GetName());
                        contentPtr->set_value(ToString(gauge->GetValue()));
                    }
                }
                logBasePtr = logPtr;
            } else {
                logPtr->CopyFrom(*logBasePtr);
            }
        }

        tmp = tmp->GetNext();
    }

    // go metrics
    for (auto& metrics : mGoMetrics) {
        // get region by project
        set<string> regions = GetRegionsByProjects(FindProjectFromGoMetricRecord(metrics));

        Log* logBasePtr = nullptr;
        for (const auto& region : regions) {
            // create logPtr
            Log* logPtr = CreateLogPtr(logGroupMap, region);
            if (logBasePtr == nullptr) {
                // set time
                auto now = GetCurrentLogtailTime();
                SetLogTime(logPtr,
                           AppConfig::GetInstance()->EnableLogTimeAutoAdjust() ? now.tv_sec + GetTimeDelta()
                                                                               : now.tv_sec);

                // set content
                Json::Value metricsRecordLabel;
                for (const auto& metric : metrics) {
                    // category
                    if (metric.first.compare("label.metric_category") == 0) {
                        Log_Content* contentPtr = logPtr->add_contents();
                        contentPtr->set_key(METRIC_KEY_CATEGORY);
                        contentPtr->set_value(metric.second);
                        continue;
                    }
                    // label
                    if (metric.first.compare(0, METRIC_KEY_LABEL.length(), METRIC_KEY_LABEL) == 0) {
                        metricsRecordLabel[metric.first.substr(METRIC_KEY_LABEL.length() + 1)] = metric.second;
                        continue;
                    }
                    // value
                    if (metric.first.compare(0, METRIC_KEY_VALUE.length(), METRIC_KEY_VALUE) == 0) {
                        Log_Content* contentPtr = logPtr->add_contents();
                        contentPtr->set_key(metric.first.substr(METRIC_KEY_VALUE.length() + 1));
                        contentPtr->set_value(metric.second);
                        continue;
                    }
                }
                Log_Content* contentPtr = logPtr->add_contents();
                contentPtr->set_key(METRIC_KEY_LABEL);
                contentPtr->set_value(GenJsonString(metricsRecordLabel));
                logBasePtr = logPtr;
            } else {
                logPtr->CopyFrom(*logBasePtr);
            }
        }
    }
}

void ReadMetrics::ReadAsFileBuffer(string& metricsContent) const {
    ReadLock lock(mReadWriteLock);

    ostringstream oss;

    // c++ metrics
    MetricsRecord* tmp = mHead;
    while (tmp) {
        Json::Value metricsRecordJson, metricsRecordLabel;
        auto now = GetCurrentLogtailTime();
        metricsRecordJson["time"]
            = AppConfig::GetInstance()->EnableLogTimeAutoAdjust() ? now.tv_sec + GetTimeDelta() : now.tv_sec;

        metricsRecordJson[METRIC_KEY_CATEGORY] = tmp->GetCategory();

        for (auto item = tmp->GetLabels()->begin(); item != tmp->GetLabels()->end(); ++item) {
            pair<string, string> pair = *item;
            metricsRecordLabel[pair.first] = pair.second;
        }
        for (auto item = tmp->GetDynamicLabels()->begin(); item != tmp->GetDynamicLabels()->end(); ++item) {
            pair<string, function<string()>> pair = *item;
            metricsRecordLabel[pair.first] = pair.second();
        }
        metricsRecordJson[METRIC_KEY_LABEL] = metricsRecordLabel;

        for (auto& item : tmp->GetCounters()) {
            CounterPtr counter = item;
            metricsRecordJson[counter->GetName()] = ToString(counter->GetValue());
        }
        for (auto& item : tmp->GetTimeCounters()) {
            TimeCounterPtr counter = item;
            metricsRecordJson[counter->GetName()] = ToString(counter->GetValue());
        }
        for (auto& item : tmp->GetIntGauges()) {
            IntGaugePtr gauge = item;
            metricsRecordJson[gauge->GetName()] = ToString(gauge->GetValue());
        }
        for (auto& item : tmp->GetDoubleGauges()) {
            DoubleGaugePtr gauge = item;
            metricsRecordJson[gauge->GetName()] = ToString(gauge->GetValue());
        }

        oss << GenJsonString(metricsRecordJson) << '\n';

        tmp = tmp->GetNext();
    }

    // go metrics
    for (auto& metrics : mGoMetrics) {
        Json::Value metricsRecordJson, metricsRecordLabel;
        auto now = GetCurrentLogtailTime();
        metricsRecordJson["time"]
            = AppConfig::GetInstance()->EnableLogTimeAutoAdjust() ? now.tv_sec + GetTimeDelta() : now.tv_sec;
        for (const auto& metric : metrics) {
            if (metric.first.compare("label.metric_category") == 0) {
                metricsRecordJson[METRIC_KEY_CATEGORY] = metric.second;
                continue;
            }
            if (metric.first.compare(0, METRIC_KEY_LABEL.length(), METRIC_KEY_LABEL) == 0) {
                metricsRecordLabel[metric.first.substr(METRIC_KEY_LABEL.length() + 1)] = metric.second;
                continue;
            }
            metricsRecordJson[metric.first.substr(METRIC_KEY_VALUE.length() + 1)] = metric.second;
        }
        metricsRecordJson[METRIC_KEY_LABEL] = metricsRecordLabel;
        oss << GenJsonString(metricsRecordJson) << '\n';
    }

    metricsContent = oss.str();
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
            if (metric.first == METRIC_KEY_VALUE + "." + METRIC_AGENT_MEMORY_GO) {
                LoongCollectorMonitor::GetInstance()->SetAgentGoMemory(stoi(metric.second));
            }
            if (metric.first == METRIC_KEY_VALUE + "." + METRIC_AGENT_GO_ROUTINES_TOTAL) {
                LoongCollectorMonitor::GetInstance()->SetAgentGoRoutinesTotal(stoi(metric.second));
            }
            LogtailMonitor::GetInstance()->UpdateMetric(metric.first, metric.second);
        }
    }
}

Log* CreateLogPtr(map<string, sls_logs::LogGroup*>& logGroupMap, const string& region) {
    Log* logPtr = nullptr;
    map<string, sls_logs::LogGroup*>::iterator iter = logGroupMap.find(region);
    if (iter != logGroupMap.end()) {
        sls_logs::LogGroup* logGroup = iter->second;
        logPtr = logGroup->add_logs();
    } else {
        sls_logs::LogGroup* logGroup = new sls_logs::LogGroup();
        logGroup->set_category(METRIC_SLS_LOGSTORE_NAME);
        logGroup->set_source(LoongCollectorMonitor::mIpAddr);
        logGroup->set_topic(METRIC_TOPIC_TYPE);
        logPtr = logGroup->add_logs();
        logGroupMap.insert(pair<string, sls_logs::LogGroup*>(region, logGroup));
    }
    return logPtr;
}

string FindProjectFromMetricRecord(const MetricsRecord* tmp) {
    for (auto item = tmp->GetLabels()->begin(); item != tmp->GetLabels()->end(); ++item) {
        pair<string, string> pair = *item;
        if (METRIC_LABEL_KEY_PROJECT == pair.first) {
            return pair.second;
        }
    }
    for (auto item = tmp->GetDynamicLabels()->begin(); item != tmp->GetDynamicLabels()->end(); ++item) {
        pair<string, function<string()>> pair = *item;
        if (METRIC_LABEL_KEY_PROJECT == pair.first) {
            return pair.second();
        }
    }
    return "";
}

string FindProjectFromGoMetricRecord(const map<string, string>& metrics) {
    for (const auto& metric : metrics) {
        if (metric.first == METRIC_KEY_LABEL + "." + METRIC_LABEL_KEY_PROJECT) {
            return metric.second;
        }
    }
    return "";
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

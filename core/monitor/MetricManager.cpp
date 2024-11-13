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

#include "app_config/AppConfig.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "logger/Logger.h"


using namespace sls_logs;

namespace logtail {

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

void ReadMetrics::ReadAsLogGroup(const std::string& regionFieldName,
                                 const std::string& defaultRegion,
                                 std::map<std::string, sls_logs::LogGroup*>& logGroupMap) const {
    ReadLock lock(mReadWriteLock);
    MetricsRecord* tmp = mHead;
    while (tmp) {
        Log* logPtr = nullptr;

        for (auto item = tmp->GetLabels()->begin(); item != tmp->GetLabels()->end(); ++item) {
            std::pair<std::string, std::string> pair = *item;
            if (regionFieldName == pair.first) {
                std::map<std::string, sls_logs::LogGroup*>::iterator iter;
                std::string region = pair.second;
                iter = logGroupMap.find(region);
                if (iter != logGroupMap.end()) {
                    sls_logs::LogGroup* logGroup = iter->second;
                    logPtr = logGroup->add_logs();
                } else {
                    sls_logs::LogGroup* logGroup = new sls_logs::LogGroup();
                    logPtr = logGroup->add_logs();
                    logGroupMap.insert(std::pair<std::string, sls_logs::LogGroup*>(region, logGroup));
                }
            }
        }
        if (!logPtr) {
            std::map<std::string, sls_logs::LogGroup*>::iterator iter;
            iter = logGroupMap.find(defaultRegion);
            if (iter != logGroupMap.end()) {
                sls_logs::LogGroup* logGroup = iter->second;
                logPtr = logGroup->add_logs();
            } else {
                sls_logs::LogGroup* logGroup = new sls_logs::LogGroup();
                logPtr = logGroup->add_logs();
                logGroupMap.insert(std::pair<std::string, sls_logs::LogGroup*>(defaultRegion, logGroup));
            }
        }
        auto now = GetCurrentLogtailTime();
        SetLogTime(logPtr,
                   AppConfig::GetInstance()->EnableLogTimeAutoAdjust() ? now.tv_sec + GetTimeDelta() : now.tv_sec);
        { // category
            Log_Content* contentPtr = logPtr->add_contents();
            contentPtr->set_key(METRIC_KEY_CATEGORY);
            contentPtr->set_value(tmp->GetCategory());
        }
        { // label
            Json::Value metricsRecordLabel;
            for (auto item = tmp->GetLabels()->begin(); item != tmp->GetLabels()->end(); ++item) {
                std::pair<std::string, std::string> pair = *item;
                metricsRecordLabel[pair.first] = pair.second;
            }
            for (auto item = tmp->GetDynamicLabels()->begin(); item != tmp->GetDynamicLabels()->end(); ++item) {
                std::pair<std::string, std::function<std::string()>> pair = *item;
                metricsRecordLabel[pair.first] = pair.second();
            }
            Json::StreamWriterBuilder writer;
            writer["indentation"] = "";
            std::string jsonString = Json::writeString(writer, metricsRecordLabel);
            Log_Content* contentPtr = logPtr->add_contents();
            contentPtr->set_key(METRIC_KEY_LABEL);
            contentPtr->set_value(jsonString);
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
        tmp = tmp->GetNext();
    }
}

void ReadMetrics::ReadAsFileBuffer(std::string& metricsContent) const {
    ReadLock lock(mReadWriteLock);

    std::ostringstream oss;

    MetricsRecord* tmp = mHead;
    while (tmp) {
        Json::Value metricsRecordJson, metricsRecordLabel;
        auto now = GetCurrentLogtailTime();
        metricsRecordJson["time"]
            = AppConfig::GetInstance()->EnableLogTimeAutoAdjust() ? now.tv_sec + GetTimeDelta() : now.tv_sec;

        metricsRecordJson[METRIC_KEY_CATEGORY] = tmp->GetCategory();

        for (auto item = tmp->GetLabels()->begin(); item != tmp->GetLabels()->end(); ++item) {
            std::pair<std::string, std::string> pair = *item;
            metricsRecordLabel[pair.first] = pair.second;
        }
        for (auto item = tmp->GetDynamicLabels()->begin(); item != tmp->GetDynamicLabels()->end(); ++item) {
            std::pair<std::string, std::function<std::string()>> pair = *item;
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

        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        std::string jsonString = Json::writeString(writer, metricsRecordJson);
        oss << jsonString << '\n';

        tmp = tmp->GetNext();
    }
    metricsContent = oss.str();
}

void ReadMetrics::UpdateMetrics() {
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

} // namespace logtail

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

#include "LogtailMetric.h"
#include "common/StringTools.h"
#include "MetricConstants.h"
#include "logger/Logger.h"
#include "common/TimeUtil.h"
#include "app_config/AppConfig.h"


using namespace sls_logs;

namespace logtail {

Counter::Counter(const std::string& name, uint64_t val = 0) : mName(name), mVal(val) {
}

uint64_t Counter::GetValue() const {
    return mVal;
}

const std::string& Counter::GetName() const {
    return mName;
}

Counter* Counter::CopyAndReset() {
    return new Counter(mName, mVal.exchange(0));
}

void Counter::Add(uint64_t value) {
    mVal += value;
}

Gauge::Gauge(const std::string& name, uint64_t val = 0) : mName(name), mVal(val) {
}

uint64_t Gauge::GetValue() const {
    return mVal;
}

const std::string& Gauge::GetName() const {
    return mName;
}

Gauge* Gauge::CopyAndReset() {
    return new Gauge(mName, mVal.exchange(0));
}

void Gauge::Set(uint64_t value) {
    mVal = value;
}

MetricsRecord::MetricsRecord(LabelsPtr labels) : mLabels(labels), mDeleted(false) {
}

CounterPtr MetricsRecord::CreateCounter(const std::string& name) {
    CounterPtr counterPtr = std::make_shared<Counter>(name);
    mCounters.emplace_back(counterPtr);
    return counterPtr;
}

GaugePtr MetricsRecord::CreateGauge(const std::string& name) {
    GaugePtr gaugePtr = std::make_shared<Gauge>(name);
    mGauges.emplace_back(gaugePtr);
    return gaugePtr;
}

void MetricsRecord::MarkDeleted() {
    mDeleted = true;
}

bool MetricsRecord::IsDeleted() const {
    return mDeleted;
}

const LabelsPtr& MetricsRecord::GetLabels() const {
    return mLabels;
}

const std::vector<CounterPtr>& MetricsRecord::GetCounters() const {
    return mCounters;
}

const std::vector<GaugePtr>& MetricsRecord::GetGauges() const {
    return mGauges;
}

MetricsRecord* MetricsRecord::CopyAndReset() {
    MetricsRecord* metrics = new MetricsRecord(mLabels);
    for (auto& item : mCounters) {
        CounterPtr newPtr(item->CopyAndReset());
        metrics->mCounters.emplace_back(newPtr);
    }
    for (auto& item : mGauges) {
        GaugePtr newPtr(item->CopyAndReset());
        metrics->mGauges.emplace_back(newPtr);
    }
    return metrics;
}

MetricsRecord* MetricsRecord::GetNext() const {
    return mNext;
}

void MetricsRecord::SetNext(MetricsRecord* next) {
    mNext = next;
}

MetricsRecordRef::~MetricsRecordRef() {
    if (mMetrics) {
        mMetrics->MarkDeleted();
    }
}


void MetricsRecordRef::SetMetricsRecord(MetricsRecord* metricRecord) {
    mMetrics = metricRecord;
}

CounterPtr MetricsRecordRef::CreateCounter(const std::string& name) {
    return mMetrics->CreateCounter(name);
}
GaugePtr MetricsRecordRef::CreateGauge(const std::string& name) {
    return mMetrics->CreateGauge(name);
}

const MetricsRecord* MetricsRecordRef::operator->() const {
    return mMetrics;
}

WriteMetrics::~WriteMetrics() {
    Clear();
}

void WriteMetrics::PreparePluginCommonLabels(const std::string& projectName,
                                             const std::string& logstoreName,
                                             const std::string& region,
                                             const std::string& configName,
                                             const std::string& pluginName,
                                             const std::string& pluginID,
                                             MetricLabels& labels) {
    labels.emplace_back(std::make_pair("project", projectName));
    labels.emplace_back(std::make_pair("logstore", logstoreName));
    labels.emplace_back(std::make_pair("region", region));
    labels.emplace_back(std::make_pair("config_name", configName));
    labels.emplace_back(std::make_pair("plugin_name", pluginName));
    labels.emplace_back(std::make_pair("plugin_id", pluginID));
}

void WriteMetrics::PrepareMetricsRecordRef(MetricsRecordRef& ref, MetricLabels&& labels) {
    MetricsRecord* cur = new MetricsRecord(std::make_shared<MetricLabels>(labels));
    ref.SetMetricsRecord(cur);
    std::lock_guard<std::mutex> lock(mMutex);
    cur->SetNext(mHead);
    mHead = cur;
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
                writeMetricsTotal ++;
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
        MetricsRecord* newMetrics = preTmp->CopyAndReset();
        newMetrics->SetNext(snapshot);
        snapshot = newMetrics;
        metricsSnapshotTotal++;
        writeMetricsTotal ++;
    }

    while (tmp) {
        writeMetricsTotal ++;
        if (tmp->IsDeleted()) {
            preTmp->SetNext(tmp->GetNext());
            tmp->SetNext(toDeleteHead);
            toDeleteHead = tmp;
            tmp = preTmp->GetNext();
        } else {
            MetricsRecord* newMetrics = tmp->CopyAndReset();
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
        writeMetricsDeleteTotal ++;
    }
    LOG_INFO(sLogger, ("writeMetricsTotal", writeMetricsTotal)("writeMetricsDeleteTotal", writeMetricsDeleteTotal)("metricsSnapshotTotal", metricsSnapshotTotal));
    return snapshot;
}

ReadMetrics::~ReadMetrics() {
    Clear();
}

void ReadMetrics::ReadAsLogGroup(std::map<std::string, sls_logs::LogGroup*>& logGroupMap) const {
    ReadLock lock(mReadWriteLock);
    MetricsRecord* tmp = mHead;
    while (tmp) {
        Log* logPtr = nullptr;
        // for (auto &item: tmp->GetLabels()) {
        for (auto item = tmp->GetLabels()->begin(); item != tmp->GetLabels()->end(); ++item) {
            std::pair<std::string, std::string> pair = *item;
            if (METRIC_FIELD_REGION == pair.first) {
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
            iter = logGroupMap.find(METRIC_REGION_DEFAULT);
            if (iter != logGroupMap.end()) {
                sls_logs::LogGroup* logGroup = iter->second;
                logPtr = logGroup->add_logs();
            } else {
                sls_logs::LogGroup* logGroup = new sls_logs::LogGroup();
                logPtr = logGroup->add_logs();
                logGroupMap.insert(std::pair<std::string, sls_logs::LogGroup*>(METRIC_REGION_DEFAULT, logGroup));
            }
        }
        auto now = GetCurrentLogtailTime();
        SetLogTime(logPtr,
                   AppConfig::GetInstance()->EnableLogTimeAutoAdjust() ? now.tv_sec + GetTimeDelta() : now.tv_sec);
        for (auto item = tmp->GetLabels()->begin(); item != tmp->GetLabels()->end(); ++item) {
            std::pair<std::string, std::string> pair = *item;
            Log_Content* contentPtr = logPtr->add_contents();
            contentPtr->set_key(LABEL_PREFIX + pair.first);
            contentPtr->set_value(pair.second);
        }

        for (auto& item : tmp->GetCounters()) {
            CounterPtr counter = item;
            Log_Content* contentPtr = logPtr->add_contents();
            contentPtr->set_key(VALUE_PREFIX + counter->GetName());
            contentPtr->set_value(ToString(counter->GetValue()));
        }
        for (auto& item : tmp->GetGauges()) {
            GaugePtr gauge = item;
            Log_Content* contentPtr = logPtr->add_contents();
            contentPtr->set_key(VALUE_PREFIX + gauge->GetName());
            contentPtr->set_value(ToString(gauge->GetValue()));
        }
        tmp = tmp->GetNext();
    }
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

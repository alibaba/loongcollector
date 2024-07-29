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

#include "MetricConstants.h"
#include "app_config/AppConfig.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "logger/Logger.h"


using namespace sls_logs;

namespace logtail {

MetricsRecord::MetricsRecord(MetricLabelsPtr labels, DynamicMetricLabelsPtr dynamicLabels)
    : mLabels(labels), mDynamicLabels(dynamicLabels), mDeleted(false) {
}

CounterPtr MetricsRecord::CreateCounter(const std::string& name) {
    CounterPtr counterPtr = std::make_shared<Counter>(name);
    mCounters.emplace_back(counterPtr);
    return counterPtr;
}

IntGaugePtr MetricsRecord::CreateIntGauge(const std::string& name) {
    IntGaugePtr gaugePtr = std::make_shared<Gauge<uint64_t>>(name);
    mIntGauges.emplace_back(gaugePtr);
    return gaugePtr;
}

DoubleGaugePtr MetricsRecord::CreateDoubleGauge(const std::string& name) {
    DoubleGaugePtr gaugePtr = std::make_shared<Gauge<double>>(name);
    mDoubleGauges.emplace_back(gaugePtr);
    return gaugePtr;
}

void MetricsRecord::MarkDeleted() {
    mDeleted = true;
}

bool MetricsRecord::IsDeleted() const {
    return mDeleted;
}

const MetricLabelsPtr& MetricsRecord::GetLabels() const {
    return mLabels;
}

const DynamicMetricLabelsPtr& MetricsRecord::GetDynamicLabels() const {
    return mDynamicLabels;
}

const std::vector<CounterPtr>& MetricsRecord::GetCounters() const {
    return mCounters;
}

const std::vector<IntGaugePtr>& MetricsRecord::GetIntGauges() const {
    return mIntGauges;
}

const std::vector<DoubleGaugePtr>& MetricsRecord::GetDoubleGauges() const {
    return mDoubleGauges;
}

MetricsRecord* MetricsRecord::Collect() {
    MetricsRecord* metrics = new MetricsRecord(mLabels, mDynamicLabels);
    for (auto& item : mCounters) {
        CounterPtr newPtr(item->Collect());
        metrics->mCounters.emplace_back(newPtr);
    }
    for (auto& item : mIntGauges) {
        IntGaugePtr newPtr(item->Collect());
        metrics->mIntGauges.emplace_back(newPtr);
    }
    for (auto& item : mDoubleGauges) {
        DoubleGaugePtr newPtr(item->Collect());
        metrics->mDoubleGauges.emplace_back(newPtr);
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

const MetricLabelsPtr& MetricsRecordRef::GetLabels() const {
    return mMetrics->GetLabels();
}

const DynamicMetricLabelsPtr& MetricsRecordRef::GetDynamicLabels() const {
    return mMetrics->GetDynamicLabels();
}

CounterPtr MetricsRecordRef::CreateCounter(const std::string& name) {
    return mMetrics->CreateCounter(name);
}

IntGaugePtr MetricsRecordRef::CreateIntGauge(const std::string& name) {
    return mMetrics->CreateIntGauge(name);
}

DoubleGaugePtr MetricsRecordRef::CreateDoubleGauge(const std::string& name) {
    return mMetrics->CreateDoubleGauge(name);
}

const MetricsRecord* MetricsRecordRef::operator->() const {
    return mMetrics;
}

// ReentrantMetricsRecord相关操作可以无锁，因为mCounters、mGauges只在初始化时会添加内容，后续只允许Get操作
void ReentrantMetricsRecord::Init(MetricLabels& labels, std::unordered_map<std::string, MetricType>& metricKeys) {
    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(mMetricsRecordRef, std::move(labels));
    for (auto metric : metricKeys) {
        switch (metric.second) {
            case MetricType::METRIC_TYPE_COUNTER:
                mCounters[metric.first] = mMetricsRecordRef.CreateCounter(metric.first);
                break;
            case MetricType::METRIC_TYPE_INT_GAUGE:
                mIntGauges[metric.first] = mMetricsRecordRef.CreateIntGauge(metric.first);
                break;
            case MetricType::METRIC_TYPE_DOUBLE_GAUGE:
                mDoubleGauges[metric.first] = mMetricsRecordRef.CreateDoubleGauge(metric.first);
                break;
            default:
                break;
        }
    }
}

const MetricLabelsPtr& ReentrantMetricsRecord::GetLabels() const {
    return mMetricsRecordRef->GetLabels();
}

const DynamicMetricLabelsPtr& ReentrantMetricsRecord::GetDynamicLabels() const {
    return mMetricsRecordRef->GetDynamicLabels();
}

CounterPtr ReentrantMetricsRecord::GetCounter(const std::string& name) {
    auto it = mCounters.find(name);
    if (it != mCounters.end()) {
        return it->second;
    }
    return nullptr;
}

IntGaugePtr ReentrantMetricsRecord::GetIntGauge(const std::string& name) {
    auto it = mIntGauges.find(name);
    if (it != mIntGauges.end()) {
        return it->second;
    }
    return nullptr;
}

DoubleGaugePtr ReentrantMetricsRecord::GetDoubleGauge(const std::string& name) {
    auto it = mDoubleGauges.find(name);
    if (it != mDoubleGauges.end()) {
        return it->second;
    }
    return nullptr;
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
    labels.emplace_back(std::make_pair(METRIC_LABEL_PROJECT, projectName));
    labels.emplace_back(std::make_pair(METRIC_LABEL_LOGSTORE, logstoreName));
    labels.emplace_back(std::make_pair(METRIC_LABEL_REGION, region));
    labels.emplace_back(std::make_pair(METRIC_LABEL_CONFIG_NAME, configName));
    labels.emplace_back(std::make_pair(METRIC_LABEL_PLUGIN_NAME, pluginName));
    labels.emplace_back(std::make_pair(METRIC_LABEL_PLUGIN_ID, pluginID));
}

void WriteMetrics::PrepareMetricsRecordRef(MetricsRecordRef& ref,
                                           MetricLabels&& labels,
                                           DynamicMetricLabels&& dynamicLabels) {
    MetricsRecord* cur = new MetricsRecord(std::make_shared<MetricLabels>(labels),
                                           std::make_shared<DynamicMetricLabels>(dynamicLabels));
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
        for (auto item = tmp->GetDynamicLabels()->begin(); item != tmp->GetDynamicLabels()->end(); ++item) {
            std::pair<std::string, std::function<std::string()>> pair = *item;
            Log_Content* contentPtr = logPtr->add_contents();
            contentPtr->set_key(LABEL_PREFIX + pair.first);
            contentPtr->set_value(pair.second());
        }

        for (auto& item : tmp->GetCounters()) {
            CounterPtr counter = item;
            Log_Content* contentPtr = logPtr->add_contents();
            contentPtr->set_key(VALUE_PREFIX + counter->GetName());
            contentPtr->set_value(ToString(counter->GetValue()));
        }
        for (auto& item : tmp->GetIntGauges()) {
            IntGaugePtr gauge = item;
            Log_Content* contentPtr = logPtr->add_contents();
            contentPtr->set_key(VALUE_PREFIX + gauge->GetName());
            contentPtr->set_value(ToString(gauge->GetValue()));
        }
        for (auto& item : tmp->GetDoubleGauges()) {
            DoubleGaugePtr gauge = item;
            Log_Content* contentPtr = logPtr->add_contents();
            contentPtr->set_key(VALUE_PREFIX + gauge->GetName());
            contentPtr->set_value(ToString(gauge->GetValue()));
        }
        tmp = tmp->GetNext();
    }
}

void ReadMetrics::ReadAsFileBuffer(std::string& metricsContent) const {
    ReadLock lock(mReadWriteLock);

    std::ostringstream oss;

    MetricsRecord* tmp = mHead;
    while (tmp) {
        Json::Value metricsRecordValue;
        auto now = GetCurrentLogtailTime();
        metricsRecordValue["time"]
            = AppConfig::GetInstance()->EnableLogTimeAutoAdjust() ? now.tv_sec + GetTimeDelta() : now.tv_sec;

        for (auto item = tmp->GetLabels()->begin(); item != tmp->GetLabels()->end(); ++item) {
            std::pair<std::string, std::string> pair = *item;
            metricsRecordValue[LABEL_PREFIX + pair.first] = pair.second;
        }
        for (auto item = tmp->GetDynamicLabels()->begin(); item != tmp->GetDynamicLabels()->end(); ++item) {
            std::pair<std::string, std::function<std::string()>> pair = *item;
            metricsRecordValue[LABEL_PREFIX + pair.first] = pair.second();
        }

        for (auto& item : tmp->GetCounters()) {
            CounterPtr counter = item;
            metricsRecordValue[VALUE_PREFIX + counter->GetName()] = ToString(counter->GetValue());
        }

        for (auto& item : tmp->GetIntGauges()) {
            IntGaugePtr gauge = item;
            metricsRecordValue[VALUE_PREFIX + gauge->GetName()] = ToString(gauge->GetValue());
        }

        for (auto& item : tmp->GetDoubleGauges()) {
            DoubleGaugePtr gauge = item;
            metricsRecordValue[VALUE_PREFIX + gauge->GetName()] = ToString(gauge->GetValue());
        }

        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        std::string jsonString = Json::writeString(writer, metricsRecordValue);
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

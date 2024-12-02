/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "MetricRecord.h"

namespace logtail {

const std::string MetricCategory::METRIC_CATEGORY_UNKNOWN = "unknown";
const std::string MetricCategory::METRIC_CATEGORY_AGENT = "agent";
const std::string MetricCategory::METRIC_CATEGORY_RUNNER = "runner";
const std::string MetricCategory::METRIC_CATEGORY_PIPELINE = "pipeline";
const std::string MetricCategory::METRIC_CATEGORY_COMPONENT = "component";
const std::string MetricCategory::METRIC_CATEGORY_PLUGIN = "plugin";
const std::string MetricCategory::METRIC_CATEGORY_PLUGIN_SOURCE = "plugin_source";

MetricsRecord::MetricsRecord(const std::string& category, MetricLabelsPtr labels, DynamicMetricLabelsPtr dynamicLabels)
    : mCategory(category), mLabels(labels), mDynamicLabels(dynamicLabels), mDeleted(false) {
}

CounterPtr MetricsRecord::CreateCounter(const std::string& name) {
    CounterPtr counterPtr = std::make_shared<Counter>(name);
    mCounters.emplace_back(counterPtr);
    return counterPtr;
}

TimeCounterPtr MetricsRecord::CreateTimeCounter(const std::string& name) {
    TimeCounterPtr counterPtr = std::make_shared<TimeCounter>(name);
    mTimeCounters.emplace_back(counterPtr);
    return counterPtr;
}

IntGaugePtr MetricsRecord::CreateIntGauge(const std::string& name) {
    IntGaugePtr gaugePtr = std::make_shared<IntGauge>(name);
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

const std::string& MetricsRecord::GetCategory() const {
    return mCategory;
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

const std::vector<TimeCounterPtr>& MetricsRecord::GetTimeCounters() const {
    return mTimeCounters;
}

const std::vector<IntGaugePtr>& MetricsRecord::GetIntGauges() const {
    return mIntGauges;
}

const std::vector<DoubleGaugePtr>& MetricsRecord::GetDoubleGauges() const {
    return mDoubleGauges;
}

MetricsRecord* MetricsRecord::Collect() {
    MetricsRecord* metrics = new MetricsRecord(mCategory, mLabels, mDynamicLabels);
    for (auto& item : mCounters) {
        CounterPtr newPtr(item->Collect());
        metrics->mCounters.emplace_back(newPtr);
    }
    for (auto& item : mTimeCounters) {
        TimeCounterPtr newPtr(item->Collect());
        metrics->mTimeCounters.emplace_back(newPtr);
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

const std::string& MetricsRecordRef::GetCategory() const {
    return mMetrics->GetCategory();
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

TimeCounterPtr MetricsRecordRef::CreateTimeCounter(const std::string& name) {
    return mMetrics->CreateTimeCounter(name);
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

void MetricsRecordRef::AddLabels(MetricLabels&& labels) {
    mMetrics->GetLabels()->insert(mMetrics->GetLabels()->end(), labels.begin(), labels.end());
}

#ifdef APSARA_UNIT_TEST_MAIN
bool MetricsRecordRef::HasLabel(const std::string& key, const std::string& value) const {
    for (auto item : *(mMetrics->GetLabels())) {
        if (item.first == key && item.second == value) {
            return true;
        }
    }
    return false;
}
#endif

}
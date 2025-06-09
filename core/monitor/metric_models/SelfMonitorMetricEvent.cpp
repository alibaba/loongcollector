/*
 * Copyright 2023 iLogtail Authors
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

#include "SelfMonitorMetricEvent.h"

#include "common/HashUtil.h"
#include "common/JsonUtil.h"
#include "common/TimeUtil.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail {

const string METRIC_GO_KEY_LABELS = "labels";
const string METRIC_GO_KEY_COUNTERS = "counters";
const string METRIC_GO_KEY_GAUGES = "gauges";

SelfMonitorMetricEvent::SelfMonitorMetricEvent(MetricsRecord* metricRecord) : mCategory(metricRecord->GetCategory()) {
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
    for (const auto& item : metricRecord->GetCounters()) {
        mCounters[item->GetName()] = item->GetValue();
    }
    for (const auto& item : metricRecord->GetTimeCounters()) {
        mCounters[item->GetName()] = item->GetValue();
    }
    // gauges
    for (const auto& item : metricRecord->GetIntGauges()) {
        mGauges[item->GetName()] = item->GetValue();
    }
    for (const auto& item : metricRecord->GetDoubleGauges()) {
        mGauges[item->GetName()] = item->GetValue();
    }
    CreateKey();
}

SelfMonitorMetricEvent::SelfMonitorMetricEvent(const std::map<std::string, std::string>& metricRecord) {
    Json::Value labels;
    Json::Value counters;
    Json::Value gauges;
    string errMsg;
    ParseJsonTable(metricRecord.at(METRIC_GO_KEY_LABELS), labels, errMsg);
    if (!errMsg.empty()) {
        mCategory = MetricCategory::METRIC_CATEGORY_UNKNOWN;
        LOG_ERROR(sLogger, ("parse go metric", "labels")("err", errMsg));
        return;
    }
    ParseJsonTable(metricRecord.at(METRIC_GO_KEY_COUNTERS), counters, errMsg);
    if (!errMsg.empty()) {
        LOG_ERROR(sLogger, ("parse go metric", "counters")("err", errMsg));
    }
    ParseJsonTable(metricRecord.at(METRIC_GO_KEY_GAUGES), gauges, errMsg);
    if (!errMsg.empty()) {
        LOG_ERROR(sLogger, ("parse go metric", "gauges")("err", errMsg));
    }
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
        if (itr->isString()) {
            try {
                mCounters[itr.key().asString()] = static_cast<uint64_t>(std::stod(itr->asString()));
            } catch (...) { // catch std::invalid_argument & std::out_of_range
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
    for (const auto& label : mLabels) {
        key += (";" + label.first + ":" + label.second);
    }
    mKey = HashString(key);
    mUpdatedFlag = true;
}

void SelfMonitorMetricEvent::SetInterval(size_t interval) {
    mIntervalsSinceLastSend = 0;
    mSendInterval = interval;
}

void SelfMonitorMetricEvent::Merge(const SelfMonitorMetricEvent& event) {
    if (mSendInterval != event.mSendInterval) {
        mSendInterval = event.mSendInterval;
        mIntervalsSinceLastSend = 0;
    }
    for (auto counter = event.mCounters.begin(); counter != event.mCounters.end(); counter++) {
        if (mCounters.find(counter->first) != mCounters.end()) {
            mCounters[counter->first] += counter->second;
        } else {
            mCounters[counter->first] = counter->second;
        }
    }
    for (auto gauge = event.mGauges.begin(); gauge != event.mGauges.end(); gauge++) {
        mGauges[gauge->first] = gauge->second;
    }
    mUpdatedFlag = true;
}

bool SelfMonitorMetricEvent::ShouldSend() {
    mIntervalsSinceLastSend++;
    return (mIntervalsSinceLastSend >= mSendInterval) && mUpdatedFlag;
}

bool SelfMonitorMetricEvent::ShouldDelete() {
    return (mIntervalsSinceLastSend >= mSendInterval) && !mUpdatedFlag;
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
    metricEventPtr->SetValue(UntypedMultiDoubleValues{{}, nullptr});
    for (auto counter = mCounters.begin(); counter != mCounters.end(); counter++) {
        metricEventPtr->MutableValue<UntypedMultiDoubleValues>()->SetValue(
            counter->first, {UntypedValueMetricType::MetricTypeCounter, double(counter->second)});
        counter->second = 0;
    }
    for (auto gauge = mGauges.begin(); gauge != mGauges.end(); gauge++) {
        metricEventPtr->MutableValue<UntypedMultiDoubleValues>()->SetValue(
            gauge->first, {UntypedValueMetricType::MetricTypeGauge, gauge->second});
    }
    // set flags
    mIntervalsSinceLastSend = 0;
    mUpdatedFlag = false;
}

std::string SelfMonitorMetricEvent::GetLabel(const std::string& labelKey) {
    if (mLabels.find(labelKey) != mLabels.end()) {
        return mLabels.at(labelKey);
    }
    return "";
}

uint64_t SelfMonitorMetricEvent::GetCounter(const std::string& counterName) {
    if (mCounters.find(counterName) != mCounters.end()) {
        return mCounters.at(counterName);
    }
    return 0;
}

double SelfMonitorMetricEvent::GetGauge(const std::string& gaugeName) {
    if (mGauges.find(gaugeName) != mGauges.end()) {
        return mGauges.at(gaugeName);
    }
    return 0;
}

} // namespace logtail

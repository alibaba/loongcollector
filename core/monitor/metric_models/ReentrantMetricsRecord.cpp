/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "ReentrantMetricsRecord.h"

namespace logtail {

// ReentrantMetricsRecord相关操作可以无锁，因为mCounters、mGauges只在初始化时会添加内容，后续只允许Get操作
void ReentrantMetricsRecord::Init(const std::string& category,
                                  MetricLabels& labels,
                                  DynamicMetricLabels& dynamicLabels,
                                  std::unordered_map<std::string, MetricType>& metricKeys) {
    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
        mMetricsRecordRef, category, std::move(labels), std::move(dynamicLabels));
    for (auto metric : metricKeys) {
        switch (metric.second) {
            case MetricType::METRIC_TYPE_COUNTER:
                mCounters[metric.first] = mMetricsRecordRef.CreateCounter(metric.first);
                break;
            case MetricType::METRIC_TYPE_TIME_COUNTER:
                mTimeCounters[metric.first] = mMetricsRecordRef.CreateTimeCounter(metric.first);
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

TimeCounterPtr ReentrantMetricsRecord::GetTimeCounter(const std::string& name) {
    auto it = mTimeCounters.find(name);
    if (it != mTimeCounters.end()) {
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

ReentrantMetricsRecordRef PluginMetricManager::GetOrCreateReentrantMetricsRecordRef(MetricLabels labels, DynamicMetricLabels dynamicLabels) {
    std::lock_guard<std::mutex> lock(mutex);

    std::string key = GenerateKey(labels);
    // try get
    auto it = mReentrantMetricsRecordRefsMap.find(key);
    if (it != mReentrantMetricsRecordRefsMap.end()) {
        return it->second;
    }
    // create
    MetricLabels newLabels = mDefaultLabels;
    newLabels.insert(newLabels.end(), labels.begin(), labels.end());

    ReentrantMetricsRecordRef ptr = std::make_shared<ReentrantMetricsRecord>();
    ptr->Init(mDefaultCategory, newLabels, dynamicLabels, mMetricKeys);

    mReentrantMetricsRecordRefsMap.emplace(key, ptr);
    if (mSizeGauge != nullptr) {
        mSizeGauge->Set(mReentrantMetricsRecordRefsMap.size());
    }
    return ptr;
}

void PluginMetricManager::ReleaseReentrantMetricsRecordRef(MetricLabels labels) {
    std::lock_guard<std::mutex> lock(mutex);

    std::string key = GenerateKey(labels);
    // try get
    auto it = mReentrantMetricsRecordRefsMap.find(key);
    if (it == mReentrantMetricsRecordRefsMap.end()) {
        return;
    } else if (it->second.use_count() > 2) {
        // mMetricsRecordRefMaps中一次引用，待删除的实例中有一次引用
        // 如果引用数大于二，说明还有其他地方在使用这个MetricsRecordRef，mMetricsRecordRefMaps中就不将它删除
        return;
    }
    // delete
    mReentrantMetricsRecordRefsMap.erase(key);
    if (mSizeGauge != nullptr) {
        mSizeGauge->Set(mReentrantMetricsRecordRefsMap.size());
    }
}

std::string PluginMetricManager::GenerateKey(MetricLabels& labels) {
    std::sort(labels.begin(), labels.end());
    std::ostringstream oss;
    for (const auto& pair : labels) {
        oss << pair.first << "=" << pair.second << ";";
    }
    return oss.str();
}

} // namespace logtail
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
#include "PluginMetricManager.h"

namespace logtail {

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
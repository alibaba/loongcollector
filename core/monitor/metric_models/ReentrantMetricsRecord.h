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
#pragma once
#include <mutex>
#include <unordered_map>

#include "MetricManager.h"

namespace logtail {

class ReentrantMetricsRecord {
private:
    MetricsRecordRef mMetricsRecordRef;
    std::unordered_map<std::string, CounterPtr> mCounters;
    std::unordered_map<std::string, TimeCounterPtr> mTimeCounters;
    std::unordered_map<std::string, IntGaugePtr> mIntGauges;
    std::unordered_map<std::string, DoubleGaugePtr> mDoubleGauges;

public:
    void Init(const std::string& category,
              MetricLabels& labels,
              DynamicMetricLabels& dynamicLabels,
              std::unordered_map<std::string, MetricType>& metricKeys);
    const MetricLabelsPtr& GetLabels() const;
    const DynamicMetricLabelsPtr& GetDynamicLabels() const;
    CounterPtr GetCounter(const std::string& name);
    TimeCounterPtr GetTimeCounter(const std::string& name);
    IntGaugePtr GetIntGauge(const std::string& name);
    DoubleGaugePtr GetDoubleGauge(const std::string& name);
};
using ReentrantMetricsRecordRef = std::shared_ptr<ReentrantMetricsRecord>;


class PluginMetricManager {
public:
    PluginMetricManager(const MetricLabelsPtr defaultLabels,
                        std::unordered_map<std::string, MetricType> metricKeys,
                        std::string category = MetricCategory::METRIC_CATEGORY_UNKNOWN)
        : mDefaultLabels(defaultLabels->begin(), defaultLabels->end()),
          mMetricKeys(metricKeys),
          mDefaultCategory(category) {}

    ReentrantMetricsRecordRef GetOrCreateReentrantMetricsRecordRef(MetricLabels labels,
                                                                   DynamicMetricLabels dynamicLabels = {});
    void ReleaseReentrantMetricsRecordRef(MetricLabels labels);

    void RegisterSizeGauge(IntGaugePtr ptr) { mSizeGauge = ptr; }

private:
    std::string GenerateKey(MetricLabels& labels);

    MetricLabels mDefaultLabels;
    std::unordered_map<std::string, MetricType> mMetricKeys;
    std::string mDefaultCategory;
    std::unordered_map<std::string, ReentrantMetricsRecordRef> mReentrantMetricsRecordRefsMap;
    mutable std::mutex mutex;

    IntGaugePtr mSizeGauge;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class PluginMetricManagerUnittest;
#endif
};
using PluginMetricManagerPtr = std::shared_ptr<PluginMetricManager>;

} // namespace logtail
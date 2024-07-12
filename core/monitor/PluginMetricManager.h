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
#include <shared_mutex>
#include <unordered_map>

#include "LogtailMetric.h"

namespace logtail {

class PluginMetricManager {
public:
    PluginMetricManager(const LabelsPtr defaultLabels) : mDefaultLabels(defaultLabels->begin(), defaultLabels->end()) {}
    MetricsRecordRefPtr GetOrCreateMetricsRecordRefPtr(MetricLabels labels);
    void ReleaseMetricsRecordRefPtr(MetricLabels labels);

    void RegisterSizeGauge(GaugePtr ptr) { mSizeGauge = ptr; }

private:
    std::string GenerateKey(MetricLabels& labels);
    MetricLabels mDefaultLabels;
    std::unordered_map<std::string, MetricsRecordRefPtr> mMetricsRecordRefsMap;
    mutable std::shared_mutex mutex;

    GaugePtr mSizeGauge;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class PluginMetricManagerUnittest;
#endif
};
using PluginMetricManagerPtr = std::shared_ptr<PluginMetricManager>;

} // namespace logtail
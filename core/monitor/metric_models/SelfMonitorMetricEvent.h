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

#pragma once
#include <map>

#include "models/MetricEvent.h"
#include "monitor/metric_models/MetricRecord.h"

namespace logtail {

struct SelfMonitorMetricRule {
    bool mEnable;
    size_t mInterval;
};

struct SelfMonitorMetricRules {
    SelfMonitorMetricRule mAgentMetricsRule;
    SelfMonitorMetricRule mRunnerMetricsRule;
    SelfMonitorMetricRule mPipelineMetricsRule;
    SelfMonitorMetricRule mPluginSourceMetricsRule;
    SelfMonitorMetricRule mPluginMetricsRule;
    SelfMonitorMetricRule mComponentMetricsRule;
};

using SelfMonitorMetricEventKey = int64_t;
class SelfMonitorMetricEvent {
public:
    SelfMonitorMetricEvent() = default;
    SelfMonitorMetricEvent(const SelfMonitorMetricEvent& event) = default;

    explicit SelfMonitorMetricEvent(MetricsRecord* metricRecord);
    explicit SelfMonitorMetricEvent(const std::map<std::string, std::string>& metricRecord);

    void SetInterval(size_t interval);
    void Merge(const SelfMonitorMetricEvent& event);

    bool ShouldSend();
    bool ShouldDelete();
    void ReadAsMetricEvent(MetricEvent* metricEventPtr);

    // 调用的对象应是不再修改的只读对象，不用加锁
    std::string GetLabel(const std::string& labelKey);
    uint64_t GetCounter(const std::string& counterName);
    double GetGauge(const std::string& gaugeName);

    SelfMonitorMetricEventKey mKey = 0L; // labels + category
    std::string mCategory; // category
private:
    void CreateKey();

    std::unordered_map<std::string, std::string> mLabels;
    std::unordered_map<std::string, uint64_t> mCounters;
    std::unordered_map<std::string, double> mGauges;
    int32_t mSendInterval = 0;
    int32_t mIntervalsSinceLastSend = 0;
    bool mUpdatedFlag = false;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SelfMonitorMetricEventUnittest;
#endif
};
using SelfMonitorMetricEventMap = std::unordered_map<SelfMonitorMetricEventKey, SelfMonitorMetricEvent>;

} // namespace logtail

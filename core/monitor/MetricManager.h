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

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "MetricRecord.h"
#include "common/Lock.h"
#include "models/PipelineEventGroup.h"
#include "protobuf/sls/sls_logs.pb.h"

namespace logtail {

extern const std::string METRIC_TOPIC_TYPE;

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
    SelfMonitorMetricEvent();
    SelfMonitorMetricEvent(MetricsRecord* metricRecord);
    SelfMonitorMetricEvent(const std::map<std::string, std::string>& metricRecord);

    void SetInterval(size_t interval);
    void Merge(SelfMonitorMetricEvent& event);

    bool ShouldSend();
    bool ShouldDelete();
    void ReadAsMetricEvent(MetricEvent* metricEventPtr);

    SelfMonitorMetricEventKey mKey; // labels + category
    std::string mCategory; // category
private:
    void CreateKey();

    std::unordered_map<std::string, std::string> mLabels;
    std::unordered_map<std::string, uint64_t> mCounters;
    std::unordered_map<std::string, double> mGauges;
    int32_t mSendInterval;
    int32_t mLastSendInterval;
    bool mUpdatedFlag;
};
using SelfMonitorMetricEventMap = std::unordered_map<SelfMonitorMetricEventKey, SelfMonitorMetricEvent>;

class WriteMetrics {
private:
    WriteMetrics() = default;
    std::mutex mMutex;
    MetricsRecord* mHead = nullptr;

    void Clear();
    MetricsRecord* GetHead();

public:
    ~WriteMetrics();
    static WriteMetrics* GetInstance() {
        static WriteMetrics* ptr = new WriteMetrics();
        return ptr;
    }

    void PrepareMetricsRecordRef(MetricsRecordRef& ref,
                                 const std::string& category,
                                 MetricLabels&& labels,
                                 DynamicMetricLabels&& dynamicLabels = {});
    void CreateMetricsRecordRef(MetricsRecordRef& ref,
                                const std::string& category,
                                MetricLabels&& labels,
                                DynamicMetricLabels&& dynamicLabels = {});
    void CommitMetricsRecordRef(MetricsRecordRef& ref);
    MetricsRecord* DoSnapshot();


#ifdef APSARA_UNIT_TEST_MAIN
    friend class MetricManagerUnittest;
#endif
};

class ReadMetrics {
private:
    ReadMetrics() = default;
    mutable ReadWriteLock mReadWriteLock;
    MetricsRecord* mHead = nullptr;
    std::vector<std::map<std::string, std::string>> mGoMetrics;
    void Clear();
    MetricsRecord* GetHead();
    void UpdateGoCppProvidedMetrics(std::vector<std::map<std::string, std::string>>& metricsList);

public:
    ~ReadMetrics();
    static ReadMetrics* GetInstance() {
        static ReadMetrics* ptr = new ReadMetrics();
        return ptr;
    }
    void ReadAsSelfMonitorMetricEvents(std::vector<SelfMonitorMetricEvent>& metricEventList) const;
    void UpdateMetrics();

#ifdef APSARA_UNIT_TEST_MAIN
    friend class MetricManagerUnittest;
#endif
};

} // namespace logtail

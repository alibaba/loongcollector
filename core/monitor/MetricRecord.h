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

#pragma once
#include "MetricTypes.h"

namespace logtail {

extern const std::string METRIC_KEY_LABEL;
extern const std::string METRIC_KEY_VALUE;
extern const std::string METRIC_KEY_CATEGORY;
class MetricCategory {
public:
    static const std::string METRIC_CATEGORY_UNKNOWN;
    static const std::string METRIC_CATEGORY_AGENT;
    static const std::string METRIC_CATEGORY_RUNNER;
    static const std::string METRIC_CATEGORY_PIPELINE;
    static const std::string METRIC_CATEGORY_COMPONENT;
    static const std::string METRIC_CATEGORY_PLUGIN;
    static const std::string METRIC_CATEGORY_PLUGIN_SOURCE;
};

class MetricsRecord {
private:
    std::string mCategory;
    MetricLabelsPtr mLabels;
    DynamicMetricLabelsPtr mDynamicLabels;
    std::vector<CounterPtr> mCounters;
    std::vector<TimeCounterPtr> mTimeCounters;
    std::vector<IntGaugePtr> mIntGauges;
    std::vector<DoubleGaugePtr> mDoubleGauges;

    std::atomic_bool mDeleted;
    MetricsRecord* mNext = nullptr;

public:
    MetricsRecord(const std::string& category, MetricLabelsPtr labels, DynamicMetricLabelsPtr dynamicLabels = nullptr);
    MetricsRecord() = default;
    void MarkDeleted();
    bool IsDeleted() const;
    const std::string& GetCategory() const;
    const MetricLabelsPtr& GetLabels() const;
    const DynamicMetricLabelsPtr& GetDynamicLabels() const;
    const std::vector<CounterPtr>& GetCounters() const;
    const std::vector<TimeCounterPtr>& GetTimeCounters() const;
    const std::vector<IntGaugePtr>& GetIntGauges() const;
    const std::vector<DoubleGaugePtr>& GetDoubleGauges() const;
    CounterPtr CreateCounter(const std::string& name);
    TimeCounterPtr CreateTimeCounter(const std::string& name);
    IntGaugePtr CreateIntGauge(const std::string& name);
    DoubleGaugePtr CreateDoubleGauge(const std::string& name);
    MetricsRecord* Collect();
    void SetNext(MetricsRecord* next);
    MetricsRecord* GetNext() const;
};

class MetricsRecordRef {
    friend class WriteMetrics;
    friend bool operator==(const MetricsRecordRef& lhs, std::nullptr_t rhs);
    friend bool operator==(std::nullptr_t rhs, const MetricsRecordRef& lhs);

private:
    MetricsRecord* mMetrics = nullptr;

public:
    ~MetricsRecordRef();
    MetricsRecordRef() = default;
    MetricsRecordRef(const MetricsRecordRef&) = delete;
    MetricsRecordRef& operator=(const MetricsRecordRef&) = delete;
    MetricsRecordRef(MetricsRecordRef&&) = delete;
    MetricsRecordRef& operator=(MetricsRecordRef&&) = delete;
    void SetMetricsRecord(MetricsRecord* metricRecord);
    const std::string& GetCategory() const;
    const MetricLabelsPtr& GetLabels() const;
    const DynamicMetricLabelsPtr& GetDynamicLabels() const;
    CounterPtr CreateCounter(const std::string& name);
    TimeCounterPtr CreateTimeCounter(const std::string& name);
    IntGaugePtr CreateIntGauge(const std::string& name);
    DoubleGaugePtr CreateDoubleGauge(const std::string& name);
    const MetricsRecord* operator->() const;
    // this is not thread-safe, and should be only used before WriteMetrics::CommitMetricsRecordRef
    void AddLabels(MetricLabels&& labels);
#ifdef APSARA_UNIT_TEST_MAIN
    bool HasLabel(const std::string& key, const std::string& value) const;
#endif
};

inline bool operator==(const MetricsRecordRef& lhs, std::nullptr_t rhs) {
    return lhs.mMetrics == rhs;
}

inline bool operator==(std::nullptr_t lhs, const MetricsRecordRef& rhs) {
    return lhs == rhs.mMetrics;
}

inline bool operator!=(const MetricsRecordRef& lhs, std::nullptr_t rhs) {
    return !(lhs == rhs);
}

inline bool operator!=(std::nullptr_t lhs, const MetricsRecordRef& rhs) {
    return !(lhs == rhs);
}

}
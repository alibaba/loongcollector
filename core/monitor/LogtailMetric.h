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
#include <string>

#include "common/Lock.h"
#include "log_pb/sls_logs.pb.h"


namespace logtail {

enum class MetricType {
    METRIC_TYPE_COUNTER,
    METRIC_TYPE_GAUGE,
};

class Counter {
private:
    std::string mName;
    std::atomic_long mVal;

public:
    Counter(const std::string& name, uint64_t val);
    uint64_t GetValue() const;
    const std::string& GetName() const;
    void Add(uint64_t val);
    Counter* Collect();
};

using CounterPtr = std::shared_ptr<Counter>;

class Gauge {
private:
    std::string mName;
    std::atomic_long mVal;

public:
    Gauge(const std::string& name, uint64_t val);
    uint64_t GetValue() const;
    const std::string& GetName() const;
    void Set(uint64_t val);
    Gauge* Collect();
};

using GaugePtr = std::shared_ptr<Gauge>;

using MetricLabels = std::vector<std::pair<std::string, std::string>>;
using LabelsPtr = std::shared_ptr<MetricLabels>;

class MetricsRecord {
private:
    LabelsPtr mLabels;
    std::atomic_bool mDeleted;
    std::vector<CounterPtr> mCounters;
    std::vector<GaugePtr> mGauges;
    MetricsRecord* mNext = nullptr;

public:
    MetricsRecord(LabelsPtr labels);
    MetricsRecord() = default;
    void MarkDeleted();
    bool IsDeleted() const;
    const LabelsPtr& GetLabels() const;
    const std::vector<CounterPtr>& GetCounters() const;
    const std::vector<GaugePtr>& GetGauges() const;
    CounterPtr CreateCounter(const std::string& name);
    GaugePtr CreateGauge(const std::string& name);
    MetricsRecord* Collect();
    void SetNext(MetricsRecord* next);
    MetricsRecord* GetNext() const;
};

class MetricsRecordRef {
private:
    MetricsRecord* mMetrics = nullptr;
    std::vector<CounterPtr> mCounters;
    std::vector<GaugePtr> mGauges;

public:
    ~MetricsRecordRef();
    MetricsRecordRef() = default;
    MetricsRecordRef(const MetricsRecordRef&) = delete;
    MetricsRecordRef& operator=(const MetricsRecordRef&) = delete;
    MetricsRecordRef(MetricsRecordRef&&) = delete;
    MetricsRecordRef& operator=(MetricsRecordRef&&) = delete;
    void SetMetricsRecord(MetricsRecord* metricRecord);
    const LabelsPtr& GetLabels() const;
    CounterPtr CreateCounter(const std::string& name);
    GaugePtr CreateGauge(const std::string& name);
    const MetricsRecord* operator->() const;
};

class ReentrantMetricsRecord {
private:
    MetricsRecordRef mMetricsRecordRef;
    std::unordered_map<std::string, CounterPtr> mCounters;
    std::unordered_map<std::string, GaugePtr> mGauges;

public:
    void Init(MetricLabels& labels, std::unordered_map<std::string, MetricType>& metricKeys);
    const LabelsPtr& GetLabels() const;
    CounterPtr GetCounter(const std::string& name);
    GaugePtr GetGauge(const std::string& name);
};
using ReentrantMetricsRecordRef = std::shared_ptr<ReentrantMetricsRecord>;

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
    void PreparePluginCommonLabels(const std::string& projectName,
                                   const std::string& logstoreName,
                                   const std::string& region,
                                   const std::string& configName,
                                   const std::string& pluginName,
                                   const std::string& pluginID,
                                   MetricLabels& labels);
    void PrepareMetricsRecordRef(MetricsRecordRef& ref, MetricLabels&& labels);
    MetricsRecord* DoSnapshot();


#ifdef APSARA_UNIT_TEST_MAIN
    friend class ILogtailMetricUnittest;
#endif
};

class ReadMetrics {
private:
    ReadMetrics() = default;
    mutable ReadWriteLock mReadWriteLock;
    MetricsRecord* mHead = nullptr;
    void Clear();
    MetricsRecord* GetHead();

public:
    ~ReadMetrics();
    static ReadMetrics* GetInstance() {
        static ReadMetrics* ptr = new ReadMetrics();
        return ptr;
    }
    void ReadAsLogGroup(std::map<std::string, sls_logs::LogGroup*>& logGroupMap) const;
    void ReadAsFileBuffer(std::string& metricsContent) const;
    void UpdateMetrics();

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ILogtailMetricUnittest;
#endif
};
} // namespace logtail

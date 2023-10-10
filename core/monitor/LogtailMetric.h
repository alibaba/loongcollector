#pragma once
#include <string>
#include <atomic>
#include "common/Lock.h"
#include "log_pb/sls_logs.pb.h"


namespace logtail {

class Counter {
private:
    std::string mName;
    std::atomic_long mVal;

public:
    Counter(const std::string& name, uint64_t val);
    uint64_t GetValue() const;
    const std::string& GetName() const;
    void Add(uint64_t val);
    Counter* CopyAndReset();
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
    Gauge* CopyAndReset();
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
    MetricsRecord* CopyAndReset();
    void SetNext(MetricsRecord* next);
    MetricsRecord* GetNext() const;
};

class MetricsRecordRef {
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
    CounterPtr CreateCounter(const std::string& name);
    GaugePtr CreateGauge(const std::string& name);
    const MetricsRecord* operator->() const;
};

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
    void UpdateMetrics();

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ILogtailMetricUnittest;
#endif
};
} // namespace logtail

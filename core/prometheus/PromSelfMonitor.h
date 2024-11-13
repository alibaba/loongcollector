#pragma once

#include <map>
#include <string>
#include <unordered_map>

#include "monitor/MetricManager.h"
#include "monitor/PluginMetricManager.h"

namespace logtail {

// manage status metrics, thread unsafe
class PromSelfMonitorUnsafe {
public:
    PromSelfMonitorUnsafe() = default;

    void InitMetricManager(const std::unordered_map<std::string, MetricType>& metricKeys, const MetricLabels& labels);

    void AddCounter(const std::string& metricName, uint64_t status, uint64_t val = 1);

    void SetIntGauge(const std::string& metricName, uint64_t status, uint64_t value);

private:
    ReentrantMetricsRecordRef GetOrCreateReentrantMetricsRecordRef(const std::string& status);
    std::string& StatusToString(uint64_t status);

    PluginMetricManagerPtr mPluginMetricManagerPtr;
    std::map<std::string, ReentrantMetricsRecordRef> mPromStatusMap;
    std::map<std::string, std::map<std::string, CounterPtr>> mMetricsCounterMap;
    std::map<std::string, std::map<std::string, IntGaugePtr>> mMetricsIntGaugeMap;
    MetricLabelsPtr mDefaultLabels;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class PromSelfMonitorUnittest;
#endif
};

} // namespace logtail
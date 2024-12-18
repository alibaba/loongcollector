#pragma once

#include <set>
#include <string>

#include "StringView.h"
#include "common/Lock.h"
#include "json/value.h"

namespace logtail::prom {
class GlobalConfig {
public:
    GlobalConfig() = default;

    void Update(const Json::Value&) {}

    bool IsDropped(const std::string& metricName) { return IsDropped(StringView(metricName)); }

    bool IsDropped(const StringView metricName) {
        ReadLock lock(mDropMetricsLock);
        return mDropMetrics.find(metricName) != mDropMetrics.end();
    }

private:
    std::set<StringView> mDropMetrics;
    std::set<std::string> mDropMetricsSaved;
    ReadWriteLock mDropMetricsLock;

    std::vector<std::pair<StringView, StringView>> mExternalLabels;
    std::set<std::string> mExternalLabelsSaved;
    ReadWriteLock mExternalLabelsLock;
};
} // namespace logtail::prom
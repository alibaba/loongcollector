#pragma once

#include <set>
#include <string>

#include "StringTools.h"
#include "StringView.h"
#include "common/Lock.h"

namespace logtail::prom {
class GlobalConfig {
public:
    GlobalConfig() = default;

    void UpdateDropMetrics(const std::string& dropMetrics) {
        WriteLock lock(mDropMetricsLock);
        auto metricNames = SplitString(dropMetrics, ",");
        mDropMetrics.clear();
        mDropMetricsSaved.clear();
        for (auto& metricName : metricNames) {
            mDropMetricsSaved.insert(metricName);
            auto iter = mDropMetricsSaved.find(metricName);
            mDropMetrics.insert(StringView(iter->data(), iter->size()));
        }
    }

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
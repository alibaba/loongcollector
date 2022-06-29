/*
 * Copyright 2022 iLogtail Authors
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
#include <unordered_map>
#include "common/StringTools.h"
#include "common/Lock.h"

namespace logtail {

class MetricStore {
public:
    template <class T>
    void UpdateMetric(const std::string key, const T& val) {
        ScopedSpinLock lock(mMonitorMetricLock);
        mLogtailMetric[key] = ToString(val);
    }

    template <class T>
    void UpdateConstMetric(const std::string key, const T& val) {
        ScopedSpinLock lock(mMonitorMetricLock);
        mConstLogtailMetric[key] = ToString(val);
    }

protected:
    // Clear mutable metrics.
    void ClearMetric() {
        ScopedSpinLock lock(mMonitorMetricLock);
        mLogtailMetric.clear();
    }

    // MetricToString dumps stored metrics to string in JSON format.
    std::string MetricToString();

    // CheckLogtailStatus checks metrics status and return a string reprensent status.
    std::string CheckLogtailStatus();

    // Metrics and corresponding lock.
    SpinLock mMonitorMetricLock;
    // mConstLogtailMetric is used to store immutable metrics (assumption), such as IP.
    std::unordered_map<std::string, std::string> mConstLogtailMetric;
    std::unordered_map<std::string, std::string> mLogtailMetric;
};

} // namespace logtail
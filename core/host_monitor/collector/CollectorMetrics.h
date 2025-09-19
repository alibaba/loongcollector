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

#include <memory>
#include <string>
#include <unordered_map>

#include "monitor/metric_models/MetricRecord.h"

namespace logtail {

class CollectorMetrics {
public:
    static CollectorMetrics* GetInstance() {
        static CollectorMetrics instance;
        return &instance;
    }

    void Init();
    void UpdateFailMetrics(const std::string& collectorType);

private:
    CollectorMetrics() = default;
    ~CollectorMetrics() = default;

    MetricsRecordRef mMetricsRecordRef;
    std::unordered_map<std::string, CounterPtr> mFailCounters;
};

} // namespace logtail

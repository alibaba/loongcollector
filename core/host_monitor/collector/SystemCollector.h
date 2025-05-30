/*
 * Copyright 2025 iLogtail Authors
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

#include <filesystem>
#include <vector>

#include "host_monitor/Constants.h"
#include "host_monitor/collector/BaseCollector.h"
#include "host_monitor/collector/MetricCalculate.h"
#include "plugin/input/InputHostMonitor.h"

namespace logtail {

extern const uint32_t kMinInterval;
extern const uint32_t kDefaultInterval;
extern std::filesystem::path PROC_LOADAVG;


struct SystemStat {
    double load1;
    double load5;
    double load15;
    double load1PerCore;
    double load5PerCore;
    double load15PerCore;

    // Define the field descriptors
    static inline const FieldName<SystemStat> systemMetricFields[] = {
        FIELD_ENTRY(SystemStat, load1),
        FIELD_ENTRY(SystemStat, load5),
        FIELD_ENTRY(SystemStat, load15),
        FIELD_ENTRY(SystemStat, load1PerCore),
        FIELD_ENTRY(SystemStat, load5PerCore),
        FIELD_ENTRY(SystemStat, load15PerCore),
    };

    // Define the enumerate function for your metric type
    static void enumerate(const std::function<void(const FieldName<SystemStat, double>&)>& callback) {
        for (const auto& field : systemMetricFields) {
            callback(field);
        }
    }
};

class SystemCollector : public BaseCollector {
public:
    SystemCollector();

    int Init(int totalCount = kDefaultInterval / kMinInterval);
    ~SystemCollector() override = default;

    bool Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) override;

    static const std::string sName;
    const std::string& Name() const override { return sName; }

private:
    bool GetHostSystemLoadStat(SystemStat& systemload);

private:
    int mTotalCount = 0;
    int mCount = 0;
    MetricCalculate<SystemStat> mCalculate;
};

} // namespace logtail

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

#include "host_monitor/Constants.h"
#include "host_monitor/SystemInterface.h"
#include "host_monitor/collector/BaseCollector.h"
#include "host_monitor/collector/MetricCalculate.h"
#include "plugin/input/InputHostMonitor.h"

namespace logtail {

extern const uint32_t kHostMonitorMinInterval;
extern const uint32_t kHostMonitorDefaultInterval;

struct CPUPercent {
    double sys;
    double user;
    double wait;
    double idle;
    double other;
    double total;

    // Define the field descriptors
    static inline const FieldName<CPUPercent> CPUMetricFields[] = {
        FIELD_ENTRY(CPUPercent, sys),
        FIELD_ENTRY(CPUPercent, user),
        FIELD_ENTRY(CPUPercent, wait),
        FIELD_ENTRY(CPUPercent, idle),
        FIELD_ENTRY(CPUPercent, other),
        FIELD_ENTRY(CPUPercent, total),
    };

    // Define the enumerate function for your metric type
    static void enumerate(const std::function<void(const FieldName<CPUPercent, double>&)>& callback) {
        for (const auto& field : CPUMetricFields) {
            callback(field);
        }
    }
};

class CPUCollector : public BaseCollector {
public:
    CPUCollector();
    int Init(int totalCount = kHostMonitorDefaultInterval / kHostMonitorMinInterval);

    ~CPUCollector() override = default;

    bool Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) override;

    static const std::string sName;
    const std::string& Name() const override { return sName; }

private:
    bool GetHostSystemCPUStat(std::vector<CPUStat>& cpus);
    double ParseMetric(const std::vector<std::string>& cpuMetric, EnumCpuKey key) const;
    bool CalculateCPUPercent(CPUPercent& cpuPercent, CPUStat& cpu);

private:
    int mCountPerReport = 0;
    int mCount = 0;
    int cpuCount = 0;
    MetricCalculate<CPUPercent> mCalculate;
    CPUStat lastCpu{};
};

} // namespace logtail

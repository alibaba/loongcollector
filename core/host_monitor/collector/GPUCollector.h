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

#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "host_monitor/SystemInterface.h"
#include "host_monitor/collector/BaseCollector.h"
#include "host_monitor/collector/MetricCalculate.h"
#include "plugin/input/InputHostMonitor.h"

namespace logtail {

extern const uint32_t kHostMonitorMinInterval;
extern const uint32_t kHostMonitorDefaultInterval;

struct GPUMetric {
    double decoderUtilization;
    double encoderUtilization;
    double gpuUsedUtilization;
    double memoryFreeSpace;
    double memoryFreeUtilization;
    double memoryUsedSpace;
    double memoryUsedUtilization;
    double gpuTemperature;
    double powerReadingsPowerDraw;

    // Define the field descriptors
    static inline const FieldName<GPUMetric> GPUMetricFields[] = {
        FIELD_ENTRY(GPUMetric, decoderUtilization),
        FIELD_ENTRY(GPUMetric, encoderUtilization),
        FIELD_ENTRY(GPUMetric, gpuUsedUtilization),
        FIELD_ENTRY(GPUMetric, memoryFreeSpace),
        FIELD_ENTRY(GPUMetric, memoryFreeUtilization),
        FIELD_ENTRY(GPUMetric, memoryUsedSpace),
        FIELD_ENTRY(GPUMetric, memoryUsedUtilization),
        FIELD_ENTRY(GPUMetric, gpuTemperature),
        FIELD_ENTRY(GPUMetric, powerReadingsPowerDraw),
    };

    // Define the enumerate function for your metric type
    static void enumerate(const std::function<void(const FieldName<GPUMetric, double>&)>& callback) {
        for (const auto& field : GPUMetricFields) {
            callback(field);
        }
    }
};


class GPUCollector : public BaseCollector {
public:
    GPUCollector() = default;
    ~GPUCollector() override = default;

    bool Init(HostMonitorContext& collectContext) override;
    bool Collect(HostMonitorContext& collectContext, PipelineEventGroup* group) override;
    [[nodiscard]] const std::chrono::seconds GetCollectInterval() const override;

    static const std::string sName;
    const std::string& Name() const override { return sName; }

private:
    std::unordered_map<std::string, MetricCalculate<GPUMetric>> mCalculateMap;
};

} // namespace logtail

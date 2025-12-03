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

#include "host_monitor/collector/GPUCollector.h"

#include <string>
#include <unordered_set>

#include "_thirdparty/DCGM/dcgmlib/dcgm_fields.h"
#include "common/Flags.h"
#include "common/StringView.h"
#include "host_monitor/HostMonitorContext.h"
#include "host_monitor/SystemInterface.h"
#include "host_monitor/collector/BaseCollector.h"
#include "host_monitor/collector/CollectorConstants.h"
#include "models/MetricEvent.h"
#include "models/MetricValue.h"

DEFINE_FLAG_INT32(basic_host_monitor_gpu_collect_interval, "basic host monitor gpu collect interval, seconds", 15);
namespace logtail {

const std::string GPUCollector::sName = "gpu";

static const FieldMap fieldMap = {
     .int64Fields = {
         {DCGM_FI_DEV_GPU_UTIL, &GPUStat::gpuUtilization},
         {DCGM_FI_DEV_GPU_TEMP, &GPUStat::gpuTemperature},
         {DCGM_FI_DEV_FB_TOTAL, &GPUStat::memoryTotal},
         {DCGM_FI_DEV_FB_USED, &GPUStat::memoryUsed},
         {DCGM_FI_DEV_FB_FREE, &GPUStat::memoryFree},
         {DCGM_FI_DEV_FB_RESERVED, &GPUStat::memoryReserved},
         {DCGM_FI_DEV_DEC_UTIL, &GPUStat::decoderUtilization},
         {DCGM_FI_DEV_ENC_UTIL, &GPUStat::encoderUtilization},
     },
     .stringFields = {
         {DCGM_FI_DEV_PCI_BUSID, &GPUStat::gpuId},
     },
     .doubleFields = {
         {DCGM_FI_DEV_POWER_USAGE, &GPUStat::powerUsage},
     },
 };

bool GPUCollector::Init(HostMonitorContext& collectContext) {
    if (!BaseCollector::Init(collectContext)) {
        return false;
    }

    if (!SystemInterface::GetInstance()->InitGPUCollector(fieldMap)) {
        return false;
    }
    return true;
}

const std::chrono::seconds GPUCollector::GetCollectInterval() const {
    return std::chrono::seconds(INT32_FLAG(basic_host_monitor_gpu_collect_interval));
}

bool GPUCollector::Collect(HostMonitorContext& collectContext, PipelineEventGroup* groupPtr) {
    GPUInformation gpuInfo;
    if (!SystemInterface::GetInstance()->GetGPUInformation(collectContext.GetMetricTime(), gpuInfo)) {
        return false;
    }

    std::unordered_set<std::string> currentGpuIds;
    for (const auto& gpu : gpuInfo.stats) {
        double availableMemory = gpu.memoryTotal - gpu.memoryReserved;
        double memoryFreeUtilization = 0.0;
        double memoryUsedUtilization = 0.0;

        if (availableMemory > 0) {
            memoryFreeUtilization = (double)gpu.memoryFree / availableMemory * 100.0;
            memoryUsedUtilization = (double)gpu.memoryUsed / availableMemory * 100.0;
        }

        GPUMetric gpuMetric = {(double)gpu.decoderUtilization,
                               (double)gpu.encoderUtilization,
                               (double)gpu.gpuUtilization,
                               (double)gpu.memoryFree * 1024 * 1024, // MB to B
                               memoryFreeUtilization,
                               (double)gpu.memoryUsed * 1024 * 1024, // MB to B
                               memoryUsedUtilization,
                               (double)gpu.gpuTemperature,
                               gpu.powerUsage};
        mCalculateMap[gpu.gpuId].AddValue(gpuMetric);
        currentGpuIds.insert(gpu.gpuId);
    }

    // Clean up GPU entries that are no longer present in current collection
    for (auto it = mCalculateMap.begin(); it != mCalculateMap.end();) {
        if (currentGpuIds.find(it->first) == currentGpuIds.end()) {
            it = mCalculateMap.erase(it);
        } else {
            ++it;
        }
    }

    // If group is not provided, just collect data without generating metrics
    if (!groupPtr) {
        return true;
    }

    for (auto& mCalculate : mCalculateMap) {
        GPUMetric maxMetric, minMetric, avgMetric, lastMetric;
        mCalculate.second.Stat(maxMetric, minMetric, avgMetric, &lastMetric);
        mCalculate.second.Reset();

        struct MetricDef {
            StringView name;
            double* value;
        } metrics[] = {
            {kGpuDecoderUtilizationMax, &maxMetric.decoderUtilization},
            {kGpuDecoderUtilizationMin, &minMetric.decoderUtilization},
            {kGpuDecoderUtilizationAvg, &avgMetric.decoderUtilization},
            {kGpuEncoderUtilizationMax, &maxMetric.encoderUtilization},
            {kGpuEncoderUtilizationMin, &minMetric.encoderUtilization},
            {kGpuEncoderUtilizationAvg, &avgMetric.encoderUtilization},
            {kGpuGpuUsedutilizationMax, &maxMetric.gpuUsedUtilization},
            {kGpuGpuUsedutilizationMin, &minMetric.gpuUsedUtilization},
            {kGpuGpuUsedutilizationAvg, &avgMetric.gpuUsedUtilization},
            {kGpuMemoryFreespaceMax, &maxMetric.memoryFreeSpace},
            {kGpuMemoryFreespaceMin, &minMetric.memoryFreeSpace},
            {kGpuMemoryFreespaceAvg, &avgMetric.memoryFreeSpace},
            {kGpuMemoryFreeutilizationMax, &maxMetric.memoryFreeUtilization},
            {kGpuMemoryFreeutilizationMin, &minMetric.memoryFreeUtilization},
            {kGpuMemoryFreeutilizationAvg, &avgMetric.memoryFreeUtilization},
            {kGpuMemoryUsedspaceMax, &maxMetric.memoryUsedSpace},
            {kGpuMemoryUsedspaceMin, &minMetric.memoryUsedSpace},
            {kGpuMemoryUsedspaceAvg, &maxMetric.memoryUsedSpace},
            {kGpuMemoryUsedutilizationMax, &maxMetric.memoryUsedUtilization},
            {kGpuMemoryUsedutilizationMin, &minMetric.memoryUsedUtilization},
            {kGpuMemoryUsedutilizationAvg, &avgMetric.memoryUsedUtilization},
            {kGpuGpuTemperatureMax, &maxMetric.gpuTemperature},
            {kGpuGpuTemperatureMin, &minMetric.gpuTemperature},
            {kGpuGpuTemperatureAvg, &avgMetric.gpuTemperature},
            {kGpuPowerReadingsPowerDrawMax, &maxMetric.powerReadingsPowerDraw},
            {kGpuPowerReadingsPowerDrawMin, &minMetric.powerReadingsPowerDraw},
            {kGpuPowerReadingsPowerDrawAvg, &avgMetric.powerReadingsPowerDraw},
        };

        MetricEvent* metricEvent = groupPtr->AddMetricEvent(true);
        if (!metricEvent) {
            return false;
        }
        metricEvent->SetTimestamp(gpuInfo.collectTime, 0);
        const StringBuffer& gpuIdBuffer = metricEvent->GetSourceBuffer()->CopyString(mCalculate.first);
        metricEvent->SetTagNoCopy(kTagKeyGpuId, StringView(gpuIdBuffer.data, gpuIdBuffer.size));

        metricEvent->SetTagNoCopy(kTagKeyM, kMetricSystemGpu);

        metricEvent->SetValue<UntypedMultiDoubleValues>(metricEvent);
        auto* multiDoubleValues = metricEvent->MutableValue<UntypedMultiDoubleValues>();
        for (const auto& metric : metrics) {
            multiDoubleValues->SetValue(
                metric.name, UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, *metric.value});
        }
    }
    return true;
}

} // namespace logtail

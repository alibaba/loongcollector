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
            {KEY_GPU_DECODER_UTILIZATION_MAX, &maxMetric.decoderUtilization},
            {KEY_GPU_DECODER_UTILIZATION_MIN, &minMetric.decoderUtilization},
            {KEY_GPU_DECODER_UTILIZATION_AVG, &avgMetric.decoderUtilization},
            {KEY_GPU_ENCODER_UTILIZATION_MAX, &maxMetric.encoderUtilization},
            {KEY_GPU_ENCODER_UTILIZATION_MIN, &minMetric.encoderUtilization},
            {KEY_GPU_ENCODER_UTILIZATION_AVG, &avgMetric.encoderUtilization},
            {KEY_GPU_GPU_USEDUTILIZATION_MAX, &maxMetric.gpuUsedUtilization},
            {KEY_GPU_GPU_USEDUTILIZATION_MIN, &minMetric.gpuUsedUtilization},
            {KEY_GPU_GPU_USEDUTILIZATION_AVG, &avgMetric.gpuUsedUtilization},
            {KEY_GPU_MEMORY_FREESPACE_MAX, &maxMetric.memoryFreeSpace},
            {KEY_GPU_MEMORY_FREESPACE_MIN, &minMetric.memoryFreeSpace},
            {KEY_GPU_MEMORY_FREESPACE_AVG, &avgMetric.memoryFreeSpace},
            {KEY_GPU_MEMORY_FREEUTILIZATION_MAX, &maxMetric.memoryFreeUtilization},
            {KEY_GPU_MEMORY_FREEUTILIZATION_MIN, &minMetric.memoryFreeUtilization},
            {KEY_GPU_MEMORY_FREEUTILIZATION_AVG, &avgMetric.memoryFreeUtilization},
            {KEY_GPU_MEMORY_USEDSPACE_MAX, &maxMetric.memoryUsedSpace},
            {KEY_GPU_MEMORY_USEDSPACE_MIN, &minMetric.memoryUsedSpace},
            {KEY_GPU_MEMORY_USEDSPACE_AVG, &maxMetric.memoryUsedSpace},
            {KEY_GPU_MEMORY_USEDUTILIZATION_MAX, &maxMetric.memoryUsedUtilization},
            {KEY_GPU_MEMORY_USEDUTILIZATION_MIN, &minMetric.memoryUsedUtilization},
            {KEY_GPU_MEMORY_USEDUTILIZATION_AVG, &avgMetric.memoryUsedUtilization},
            {KEY_GPU_GPU_TEMPERATURE_MAX, &maxMetric.gpuTemperature},
            {KEY_GPU_GPU_TEMPERATURE_MIN, &minMetric.gpuTemperature},
            {KEY_GPU_GPU_TEMPERATURE_AVG, &avgMetric.gpuTemperature},
            {KEY_GPU_POWER_READINGS_POWER_DRAW_MAX, &maxMetric.powerReadingsPowerDraw},
            {KEY_GPU_POWER_READINGS_POWER_DRAW_MIN, &minMetric.powerReadingsPowerDraw},
            {KEY_GPU_POWER_READINGS_POWER_DRAW_AVG, &avgMetric.powerReadingsPowerDraw},
        };

        MetricEvent* metricEvent = groupPtr->AddMetricEvent(true);
        if (!metricEvent) {
            return false;
        }
        metricEvent->SetTimestamp(gpuInfo.collectTime, 0);
        const StringBuffer& gpuIdBuffer = metricEvent->GetSourceBuffer()->CopyString(mCalculate.first);
        metricEvent->SetTagNoCopy(TAG_KEY_GPU_ID, StringView(gpuIdBuffer.data, gpuIdBuffer.size));

        metricEvent->SetTagNoCopy(TAG_KEY_M, METRIC_SYSTEM_GPU);

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

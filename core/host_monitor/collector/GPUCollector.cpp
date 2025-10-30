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

#include "BaseCollector.h"
#include "Flags.h"
#include "MetricEvent.h"
#include "MetricValue.h"
#include "_thirdparty/DCGM/dcgmlib/dcgm_fields.h"
#include "host_monitor/HostMonitorContext.h"
#include "host_monitor/SystemInterface.h"

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
            const char* name;
            double* value;
        } metrics[] = {
            {"gpu_decoder_utilization_max", &maxMetric.decoderUtilization},
            {"gpu_decoder_utilization_min", &minMetric.decoderUtilization},
            {"gpu_decoder_utilization_avg", &avgMetric.decoderUtilization},
            {"gpu_encoder_utilization_max", &maxMetric.encoderUtilization},
            {"gpu_encoder_utilization_min", &minMetric.encoderUtilization},
            {"gpu_encoder_utilization_avg", &avgMetric.encoderUtilization},
            {"gpu_gpu_usedutilization_max", &maxMetric.gpuUsedUtilization},
            {"gpu_gpu_usedutilization_min", &minMetric.gpuUsedUtilization},
            {"gpu_gpu_usedutilization_avg", &avgMetric.gpuUsedUtilization},
            {"gpu_memory_freespace_max", &maxMetric.memoryFreeSpace},
            {"gpu_memory_freespace_min", &minMetric.memoryFreeSpace},
            {"gpu_memory_freespace_avg", &avgMetric.memoryFreeSpace},
            {"gpu_memory_freeutilization_max", &maxMetric.memoryFreeUtilization},
            {"gpu_memory_freeutilization_min", &minMetric.memoryFreeUtilization},
            {"gpu_memory_freeutilization_avg", &avgMetric.memoryFreeUtilization},
            {"gpu_memory_usedspace_max", &maxMetric.memoryUsedSpace},
            {"gpu_memory_usedspace_min", &minMetric.memoryUsedSpace},
            {"gpu_memory_usedspace_avg", &avgMetric.memoryUsedSpace},
            {"gpu_memory_usedutilization_max", &maxMetric.memoryUsedUtilization},
            {"gpu_memory_usedutilization_min", &minMetric.memoryUsedUtilization},
            {"gpu_memory_usedutilization_avg", &avgMetric.memoryUsedUtilization},
            {"gpu_gpu_temperature_max", &maxMetric.gpuTemperature},
            {"gpu_gpu_temperature_min", &minMetric.gpuTemperature},
            {"gpu_gpu_temperature_avg", &avgMetric.gpuTemperature},
            {"gpu_power_readings_power_draw_max", &maxMetric.powerReadingsPowerDraw},
            {"gpu_power_readings_power_draw_min", &minMetric.powerReadingsPowerDraw},
            {"gpu_power_readings_power_draw_avg", &avgMetric.powerReadingsPowerDraw},
        };

        MetricEvent* metricEvent = groupPtr->AddMetricEvent(true);
        if (!metricEvent) {
            return false;
        }
        metricEvent->SetTimestamp(gpuInfo.collectTime, 0);
        metricEvent->SetTag(std::string("gpuId"), mCalculate.first);
        metricEvent->SetTag(std::string("m"), std::string("system.gpu"));

        metricEvent->SetValue<UntypedMultiDoubleValues>(metricEvent);
        auto* multiDoubleValues = metricEvent->MutableValue<UntypedMultiDoubleValues>();
        for (const auto& metric : metrics) {
            multiDoubleValues->SetValue(
                std::string(metric.name),
                UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, *metric.value});
        }
    }
    return true;
}

} // namespace logtail

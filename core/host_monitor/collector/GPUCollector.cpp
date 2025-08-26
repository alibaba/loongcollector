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
 
 #include "MetricEvent.h"
 #include "MetricValue.h"
 #include "host_monitor/SystemInterface.h"
 #include "_thirdparty/DCGM/dcgmlib/dcgm_fields.h"
 
 namespace logtail {
 
 const std::string GPUCollector::sName = "gpu";
 const std::string kMetricLabelMode = "valueTag";
 
 GPUCollector::GPUCollector() {
     Init();
 }
 
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
 
 int GPUCollector::Init(int totalCount) {
     mCountPerReport = totalCount;
     mCount = 0;
     
     if(!SystemInterface::GetInstance()->InitGPUCollector(fieldMap)){
         return -1;
     }
     return 0;
 }
 
 bool GPUCollector::Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) {
     if (group == nullptr) {
         return false;
     }
     GPUInformation gpuInfo;
     if (!SystemInterface::GetInstance()->GetGPUInformation(gpuInfo)) {
         return false;
     }
 
     for (const auto& gpu : gpuInfo.stats) {
         GPUMetric gpuMetric = {
             (double)gpu.decoderUtilization,
             (double)gpu.encoderUtilization,
             (double)gpu.gpuUtilization,
             (double)gpu.memoryFree * 1024 * 1024,   // MB to B
             (double)gpu.memoryFree / (gpu.memoryTotal - gpu.memoryReserved) * 100.0,
             (double)gpu.memoryUsed * 1024 * 1024,   // MB to B
             (double)gpu.memoryUsed / (gpu.memoryTotal - gpu.memoryReserved) * 100.0,
             (double)gpu.gpuTemperature,
             gpu.powerUsage
         };
         mCalculateMap[gpu.gpuId].AddValue(gpuMetric);
     }
     mCount++;
     if (mCount < mCountPerReport) {
         return true;
     }
 
     for (auto& mCalculate : mCalculateMap) {
         GPUMetric maxMetric, minMetric, avgMetric, lastMetric;
         mCalculate.second.Stat(maxMetric, minMetric, avgMetric, &lastMetric);
         mCalculate.second.Reset();
 
         const time_t now = time(nullptr);
 
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
 
         MetricEvent* metricEvent = group->AddMetricEvent(true);
         if (!metricEvent) {
             return false;
         }
         metricEvent->SetTimestamp(now, 0);
         metricEvent->SetTag(std::string("gpuId"), mCalculate.first);
         metricEvent->SetTag(std::string("m"), std::string("system.gpu"));
 
         metricEvent->SetValue<UntypedMultiDoubleValues>(metricEvent);
         auto* multiDoubleValues = metricEvent->MutableValue<UntypedMultiDoubleValues>();
         for (const auto& metric : metrics) {
             multiDoubleValues->SetValue(std::string(metric.name),
                                         UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, *metric.value});
         }
     }
     mCount = 0;
     return true;
 }
 
 } // namespace logtail
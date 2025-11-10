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

#include "common/StringView.h"

namespace logtail {

// Common tag keys
constexpr StringView kTagKeyM("m");
constexpr StringView kTagKeyHostname("hostname");
constexpr StringView kTagKeyDevice("device");
constexpr StringView kTagKeyIp("IP");
constexpr StringView kTagKeyState("state");
constexpr StringView kTagKeyGpuId("gpuId");
constexpr StringView kTagKeyIdSerial("id_serial");
constexpr StringView kTagKeyDiskname("diskname");
constexpr StringView kTagKeyPid("pid");
constexpr StringView kTagKeyName("name");
constexpr StringView kTagKeyUser("user");

// Metric names (m tag values)
constexpr StringView kMetricSystemCpu("system.cpu");
constexpr StringView kMetricSystemMemory("system.memory");
constexpr StringView kMetricSystemNetOriginal("system.net_original");
constexpr StringView kMetricSystemTcp("system.tcp");
constexpr StringView kMetricSystemDisk("system.disk");
constexpr StringView kMetricSystemGpu("system.gpu");
constexpr StringView kMetricSystemLoad("system.load");
constexpr StringView kMetricSystemProcessCount("system.processCount");
constexpr StringView kMetricSystemProcess("system.process");

// TCP states
constexpr StringView kTcpStateListen("LISTEN");
constexpr StringView kTcpStateEstablished("ESTABLISHED");
constexpr StringView kTcpStateNonEstablished("NON_ESTABLISHED");
constexpr StringView kTcpStateTotal("TCP_TOTAL");

// Metric value keys - TCP
constexpr StringView kNetTcpConnectionMin("net_tcpconnection_min");
constexpr StringView kNetTcpConnectionMax("net_tcpconnection_max");
constexpr StringView kNetTcpConnectionAvg("net_tcpconnection_avg");

// Metric value keys - Network Rate
constexpr StringView kNetworkoutPackagesMin("networkout_packages_min");
constexpr StringView kNetworkoutPackagesMax("networkout_packages_max");
constexpr StringView kNetworkoutPackagesAvg("networkout_packages_avg");
constexpr StringView kNetworkinPackagesMin("networkin_packages_min");
constexpr StringView kNetworkinPackagesMax("networkin_packages_max");
constexpr StringView kNetworkinPackagesAvg("networkin_packages_avg");
constexpr StringView kNetworkoutErrorpackagesMin("networkout_errorpackages_min");
constexpr StringView kNetworkoutErrorpackagesMax("networkout_errorpackages_max");
constexpr StringView kNetworkoutErrorpackagesAvg("networkout_errorpackages_avg");
constexpr StringView kNetworkinErrorpackagesMin("networkin_errorpackages_min");
constexpr StringView kNetworkinErrorpackagesMax("networkin_errorpackages_max");
constexpr StringView kNetworkinErrorpackagesAvg("networkin_errorpackages_avg");
constexpr StringView kNetworkoutRateMin("networkout_rate_min");
constexpr StringView kNetworkoutRateMax("networkout_rate_max");
constexpr StringView kNetworkoutRateAvg("networkout_rate_avg");
constexpr StringView kNetworkinRateMin("networkin_rate_min");
constexpr StringView kNetworkinRateMax("networkin_rate_max");
constexpr StringView kNetworkinRateAvg("networkin_rate_avg");
constexpr StringView kNetworkoutDroppackagesMin("networkout_droppackages_min");
constexpr StringView kNetworkoutDroppackagesMax("networkout_droppackages_max");
constexpr StringView kNetworkoutDroppackagesAvg("networkout_droppackages_avg");
constexpr StringView kNetworkinDroppackagesMin("networkin_droppackages_min");
constexpr StringView kNetworkinDroppackagesMax("networkin_droppackages_max");
constexpr StringView kNetworkinDroppackagesAvg("networkin_droppackages_avg");

// Metric value keys - CPU
constexpr StringView kCpuSystemAvg("cpu_system_avg");
constexpr StringView kCpuSystemMin("cpu_system_min");
constexpr StringView kCpuSystemMax("cpu_system_max");
constexpr StringView kCpuIdleAvg("cpu_idle_avg");
constexpr StringView kCpuIdleMin("cpu_idle_min");
constexpr StringView kCpuIdleMax("cpu_idle_max");
constexpr StringView kCpuUserAvg("cpu_user_avg");
constexpr StringView kCpuUserMin("cpu_user_min");
constexpr StringView kCpuUserMax("cpu_user_max");
constexpr StringView kCpuWaitAvg("cpu_wait_avg");
constexpr StringView kCpuWaitMin("cpu_wait_min");
constexpr StringView kCpuWaitMax("cpu_wait_max");
constexpr StringView kCpuOtherAvg("cpu_other_avg");
constexpr StringView kCpuOtherMin("cpu_other_min");
constexpr StringView kCpuOtherMax("cpu_other_max");
constexpr StringView kCpuTotalAvg("cpu_total_avg");
constexpr StringView kCpuTotalMin("cpu_total_min");
constexpr StringView kCpuTotalMax("cpu_total_max");
constexpr StringView kCpuCoresValue("cpu_cores_value");

// Metric value keys - Memory
constexpr StringView kMemoryUsedutilizationMin("memory_usedutilization_min");
constexpr StringView kMemoryUsedutilizationMax("memory_usedutilization_max");
constexpr StringView kMemoryUsedutilizationAvg("memory_usedutilization_avg");
constexpr StringView kMemoryFreeutilizationMin("memory_freeutilization_min");
constexpr StringView kMemoryFreeutilizationMax("memory_freeutilization_max");
constexpr StringView kMemoryFreeutilizationAvg("memory_freeutilization_avg");
constexpr StringView kMemoryActualusedspaceMin("memory_actualusedspace_min");
constexpr StringView kMemoryActualusedspaceMax("memory_actualusedspace_max");
constexpr StringView kMemoryActualusedspaceAvg("memory_actualusedspace_avg");
constexpr StringView kMemoryFreespaceMin("memory_freespace_min");
constexpr StringView kMemoryFreespaceMax("memory_freespace_max");
constexpr StringView kMemoryFreespaceAvg("memory_freespace_avg");
constexpr StringView kMemoryUsedspaceMin("memory_usedspace_min");
constexpr StringView kMemoryUsedspaceMax("memory_usedspace_max");
constexpr StringView kMemoryUsedspaceAvg("memory_usedspace_avg");
constexpr StringView kMemoryTotalspaceMin("memory_totalspace_min");
constexpr StringView kMemoryTotalspaceMax("memory_totalspace_max");
constexpr StringView kMemoryTotalspaceAvg("memory_totalspace_avg");

// Metric value keys - Process
constexpr StringView kProcessCpuAvg("process_cpu_avg");
constexpr StringView kProcessMemoryAvg("process_memory_avg");
constexpr StringView kProcessOpenfileAvg("process_openfile_avg");
constexpr StringView kProcessNumberAvg("process_number_avg");
constexpr StringView kProcessNumberMax("process_number_max");
constexpr StringView kProcessNumberMin("process_number_min");
constexpr StringView kVmProcessMin("vm_process_min");
constexpr StringView kVmProcessMax("vm_process_max");
constexpr StringView kVmProcessAvg("vm_process_avg");

// Metric value keys - GPU
constexpr StringView kGpuDecoderUtilizationMax("gpu_decoder_utilization_max");
constexpr StringView kGpuDecoderUtilizationMin("gpu_decoder_utilization_min");
constexpr StringView kGpuDecoderUtilizationAvg("gpu_decoder_utilization_avg");
constexpr StringView kGpuEncoderUtilizationMax("gpu_encoder_utilization_max");
constexpr StringView kGpuEncoderUtilizationMin("gpu_encoder_utilization_min");
constexpr StringView kGpuEncoderUtilizationAvg("gpu_encoder_utilization_avg");
constexpr StringView kGpuGpuUsedutilizationMax("gpu_gpu_usedutilization_max");
constexpr StringView kGpuGpuUsedutilizationMin("gpu_gpu_usedutilization_min");
constexpr StringView kGpuGpuUsedutilizationAvg("gpu_gpu_usedutilization_avg");
constexpr StringView kGpuMemoryFreespaceMax("gpu_memory_freespace_max");
constexpr StringView kGpuMemoryFreespaceMin("gpu_memory_freespace_min");
constexpr StringView kGpuMemoryFreespaceAvg("gpu_memory_freespace_avg");
constexpr StringView kGpuMemoryFreeutilizationMax("gpu_memory_freeutilization_max");
constexpr StringView kGpuMemoryFreeutilizationMin("gpu_memory_freeutilization_min");
constexpr StringView kGpuMemoryFreeutilizationAvg("gpu_memory_freeutilization_avg");
constexpr StringView kGpuMemoryUsedspaceMax("gpu_memory_usedspace_max");
constexpr StringView kGpuMemoryUsedspaceMin("gpu_memory_usedspace_min");
constexpr StringView kGpuMemoryUsedspaceAvg("gpu_memory_usedspace_avg");
constexpr StringView kGpuMemoryUsedutilizationMax("gpu_memory_usedutilization_max");
constexpr StringView kGpuMemoryUsedutilizationMin("gpu_memory_usedutilization_min");
constexpr StringView kGpuMemoryUsedutilizationAvg("gpu_memory_usedutilization_avg");
constexpr StringView kGpuGpuTemperatureMax("gpu_gpu_temperature_max");
constexpr StringView kGpuGpuTemperatureMin("gpu_gpu_temperature_min");
constexpr StringView kGpuGpuTemperatureAvg("gpu_gpu_temperature_avg");
constexpr StringView kGpuPowerReadingsPowerDrawMax("gpu_power_readings_power_draw_max");
constexpr StringView kGpuPowerReadingsPowerDrawMin("gpu_power_readings_power_draw_min");
constexpr StringView kGpuPowerReadingsPowerDrawAvg("gpu_power_readings_power_draw_avg");

// Metric value keys - Disk
constexpr StringView kDiskusageTotalAvg("diskusage_total_avg");
constexpr StringView kDiskusageTotalMin("diskusage_total_min");
constexpr StringView kDiskusageTotalMax("diskusage_total_max");
constexpr StringView kDiskusageUsedAvg("diskusage_used_avg");
constexpr StringView kDiskusageUsedMin("diskusage_used_min");
constexpr StringView kDiskusageUsedMax("diskusage_used_max");
constexpr StringView kDiskusageFreeAvg("diskusage_free_avg");
constexpr StringView kDiskusageFreeMin("diskusage_free_min");
constexpr StringView kDiskusageFreeMax("diskusage_free_max");
constexpr StringView kDiskusageAvailAvg("diskusage_avail_avg");
constexpr StringView kDiskusageAvailMin("diskusage_avail_min");
constexpr StringView kDiskusageAvailMax("diskusage_avail_max");
constexpr StringView kDiskusageUtilizationAvg("diskusage_utilization_avg");
constexpr StringView kDiskusageUtilizationMin("diskusage_utilization_min");
constexpr StringView kDiskusageUtilizationMax("diskusage_utilization_max");
constexpr StringView kDiskReadiopsAvg("disk_readiops_avg");
constexpr StringView kDiskReadiopsMin("disk_readiops_min");
constexpr StringView kDiskReadiopsMax("disk_readiops_max");
constexpr StringView kDiskWriteiopsAvg("disk_writeiops_avg");
constexpr StringView kDiskWriteiopsMin("disk_writeiops_min");
constexpr StringView kDiskWriteiopsMax("disk_writeiops_max");
constexpr StringView kDiskWritebytesAvg("disk_writebytes_avg");
constexpr StringView kDiskWritebytesMin("disk_writebytes_min");
constexpr StringView kDiskWritebytesMax("disk_writebytes_max");
constexpr StringView kDiskReadbytesAvg("disk_readbytes_avg");
constexpr StringView kDiskReadbytesMin("disk_readbytes_min");
constexpr StringView kDiskReadbytesMax("disk_readbytes_max");
constexpr StringView kFsInodeutilizationAvg("fs_inodeutilization_avg");
constexpr StringView kFsInodeutilizationMin("fs_inodeutilization_min");
constexpr StringView kFsInodeutilizationMax("fs_inodeutilization_max");
constexpr StringView kDiskioqueuesizeAvg("DiskIOQueueSize_avg");
constexpr StringView kDiskioqueuesizeMin("DiskIOQueueSize_min");
constexpr StringView kDiskioqueuesizeMax("DiskIOQueueSize_max");

// Metric value keys - System Load
constexpr StringView kLoad1mMin("load_1m_min");
constexpr StringView kLoad1mMax("load_1m_max");
constexpr StringView kLoad1mAvg("load_1m_avg");
constexpr StringView kLoad5mMin("load_5m_min");
constexpr StringView kLoad5mMax("load_5m_max");
constexpr StringView kLoad5mAvg("load_5m_avg");
constexpr StringView kLoad15mMin("load_15m_min");
constexpr StringView kLoad15mMax("load_15m_max");
constexpr StringView kLoad15mAvg("load_15m_avg");
constexpr StringView kLoadPerCore1mMin("load_per_core_1m_min");
constexpr StringView kLoadPerCore1mMax("load_per_core_1m_max");
constexpr StringView kLoadPerCore1mAvg("load_per_core_1m_avg");
constexpr StringView kLoadPerCore5mMin("load_per_core_5m_min");
constexpr StringView kLoadPerCore5mMax("load_per_core_5m_max");
constexpr StringView kLoadPerCore5mAvg("load_per_core_5m_avg");
constexpr StringView kLoadPerCore15mMin("load_per_core_15m_min");
constexpr StringView kLoadPerCore15mMax("load_per_core_15m_max");
constexpr StringView kLoadPerCore15mAvg("load_per_core_15m_avg");

} // namespace logtail

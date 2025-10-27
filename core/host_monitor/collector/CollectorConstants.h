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
constexpr StringView TAG_KEY_M("m");
constexpr StringView TAG_KEY_HOSTNAME("hostname");
constexpr StringView TAG_KEY_DEVICE("device");
constexpr StringView TAG_KEY_IP("IP");
constexpr StringView TAG_KEY_STATE("state");
constexpr StringView TAG_KEY_GPU_ID("gpuId");
constexpr StringView TAG_KEY_ID_SERIAL("id_serial");
constexpr StringView TAG_KEY_DISKNAME("diskname");
constexpr StringView TAG_KEY_PID("pid");
constexpr StringView TAG_KEY_NAME("name");
constexpr StringView TAG_KEY_USER("user");

// Metric names (m tag values)
constexpr StringView METRIC_SYSTEM_CPU("system.cpu");
constexpr StringView METRIC_SYSTEM_MEMORY("system.memory");
constexpr StringView METRIC_SYSTEM_NET_ORIGINAL("system.net_original");
constexpr StringView METRIC_SYSTEM_TCP("system.tcp");
constexpr StringView METRIC_SYSTEM_DISK("system.disk");
constexpr StringView METRIC_SYSTEM_GPU("system.gpu");
constexpr StringView METRIC_SYSTEM_LOAD("system.load");
constexpr StringView METRIC_SYSTEM_PROCESS_COUNT("system.processCount");
constexpr StringView METRIC_SYSTEM_PROCESS("system.process");

// TCP states
constexpr StringView TCP_STATE_LISTEN("LISTEN");
constexpr StringView TCP_STATE_ESTABLISHED("ESTABLISHED");
constexpr StringView TCP_STATE_NON_ESTABLISHED("NON_ESTABLISHED");
constexpr StringView TCP_STATE_TOTAL("TCP_TOTAL");

// Metric value keys - TCP
constexpr StringView KEY_NET_TCP_CONNECTION_MIN("net_tcpconnection_min");
constexpr StringView KEY_NET_TCP_CONNECTION_MAX("net_tcpconnection_max");
constexpr StringView KEY_NET_TCP_CONNECTION_AVG("net_tcpconnection_avg");

// Metric value keys - Network Rate
constexpr StringView KEY_NETWORKOUT_PACKAGES_MIN("networkout_packages_min");
constexpr StringView KEY_NETWORKOUT_PACKAGES_MAX("networkout_packages_max");
constexpr StringView KEY_NETWORKOUT_PACKAGES_AVG("networkout_packages_avg");
constexpr StringView KEY_NETWORKIN_PACKAGES_MIN("networkin_packages_min");
constexpr StringView KEY_NETWORKIN_PACKAGES_MAX("networkin_packages_max");
constexpr StringView KEY_NETWORKIN_PACKAGES_AVG("networkin_packages_avg");
constexpr StringView KEY_NETWORKOUT_ERRORPACKAGES_MIN("networkout_errorpackages_min");
constexpr StringView KEY_NETWORKOUT_ERRORPACKAGES_MAX("networkout_errorpackages_max");
constexpr StringView KEY_NETWORKOUT_ERRORPACKAGES_AVG("networkout_errorpackages_avg");
constexpr StringView KEY_NETWORKIN_ERRORPACKAGES_MIN("networkin_errorpackages_min");
constexpr StringView KEY_NETWORKIN_ERRORPACKAGES_MAX("networkin_errorpackages_max");
constexpr StringView KEY_NETWORKIN_ERRORPACKAGES_AVG("networkin_errorpackages_avg");
constexpr StringView KEY_NETWORKOUT_RATE_MIN("networkout_rate_min");
constexpr StringView KEY_NETWORKOUT_RATE_MAX("networkout_rate_max");
constexpr StringView KEY_NETWORKOUT_RATE_AVG("networkout_rate_avg");
constexpr StringView KEY_NETWORKIN_RATE_MIN("networkin_rate_min");
constexpr StringView KEY_NETWORKIN_RATE_MAX("networkin_rate_max");
constexpr StringView KEY_NETWORKIN_RATE_AVG("networkin_rate_avg");
constexpr StringView KEY_NETWORKOUT_DROPPACKAGES_MIN("networkout_droppackages_min");
constexpr StringView KEY_NETWORKOUT_DROPPACKAGES_MAX("networkout_droppackages_max");
constexpr StringView KEY_NETWORKOUT_DROPPACKAGES_AVG("networkout_droppackages_avg");
constexpr StringView KEY_NETWORKIN_DROPPACKAGES_MIN("networkin_droppackages_min");
constexpr StringView KEY_NETWORKIN_DROPPACKAGES_MAX("networkin_droppackages_max");
constexpr StringView KEY_NETWORKIN_DROPPACKAGES_AVG("networkin_droppackages_avg");

// Metric value keys - CPU
constexpr StringView KEY_CPU_SYSTEM_AVG("cpu_system_avg");
constexpr StringView KEY_CPU_SYSTEM_MIN("cpu_system_min");
constexpr StringView KEY_CPU_SYSTEM_MAX("cpu_system_max");
constexpr StringView KEY_CPU_IDLE_AVG("cpu_idle_avg");
constexpr StringView KEY_CPU_IDLE_MIN("cpu_idle_min");
constexpr StringView KEY_CPU_IDLE_MAX("cpu_idle_max");
constexpr StringView KEY_CPU_USER_AVG("cpu_user_avg");
constexpr StringView KEY_CPU_USER_MIN("cpu_user_min");
constexpr StringView KEY_CPU_USER_MAX("cpu_user_max");
constexpr StringView KEY_CPU_WAIT_AVG("cpu_wait_avg");
constexpr StringView KEY_CPU_WAIT_MIN("cpu_wait_min");
constexpr StringView KEY_CPU_WAIT_MAX("cpu_wait_max");
constexpr StringView KEY_CPU_OTHER_AVG("cpu_other_avg");
constexpr StringView KEY_CPU_OTHER_MIN("cpu_other_min");
constexpr StringView KEY_CPU_OTHER_MAX("cpu_other_max");
constexpr StringView KEY_CPU_TOTAL_AVG("cpu_total_avg");
constexpr StringView KEY_CPU_TOTAL_MIN("cpu_total_min");
constexpr StringView KEY_CPU_TOTAL_MAX("cpu_total_max");
constexpr StringView KEY_CPU_CORES_VALUE("cpu_cores_value");

// Metric value keys - Memory
constexpr StringView KEY_MEMORY_USEDUTILIZATION_MIN("memory_usedutilization_min");
constexpr StringView KEY_MEMORY_USEDUTILIZATION_MAX("memory_usedutilization_max");
constexpr StringView KEY_MEMORY_USEDUTILIZATION_AVG("memory_usedutilization_avg");
constexpr StringView KEY_MEMORY_FREEUTILIZATION_MIN("memory_freeutilization_min");
constexpr StringView KEY_MEMORY_FREEUTILIZATION_MAX("memory_freeutilization_max");
constexpr StringView KEY_MEMORY_FREEUTILIZATION_AVG("memory_freeutilization_avg");
constexpr StringView KEY_MEMORY_ACTUALUSEDSPACE_MIN("memory_actualusedspace_min");
constexpr StringView KEY_MEMORY_ACTUALUSEDSPACE_MAX("memory_actualusedspace_max");
constexpr StringView KEY_MEMORY_ACTUALUSEDSPACE_AVG("memory_actualusedspace_avg");
constexpr StringView KEY_MEMORY_FREESPACE_MIN("memory_freespace_min");
constexpr StringView KEY_MEMORY_FREESPACE_MAX("memory_freespace_max");
constexpr StringView KEY_MEMORY_FREESPACE_AVG("memory_freespace_avg");
constexpr StringView KEY_MEMORY_USEDSPACE_MIN("memory_usedspace_min");
constexpr StringView KEY_MEMORY_USEDSPACE_MAX("memory_usedspace_max");
constexpr StringView KEY_MEMORY_USEDSPACE_AVG("memory_usedspace_avg");
constexpr StringView KEY_MEMORY_TOTALSPACE_MIN("memory_totalspace_min");
constexpr StringView KEY_MEMORY_TOTALSPACE_MAX("memory_totalspace_max");
constexpr StringView KEY_MEMORY_TOTALSPACE_AVG("memory_totalspace_avg");

// Metric value keys - Process
constexpr StringView KEY_PROCESS_CPU_AVG("process_cpu_avg");
constexpr StringView KEY_PROCESS_MEMORY_AVG("process_memory_avg");
constexpr StringView KEY_PROCESS_OPENFILE_AVG("process_openfile_avg");
constexpr StringView KEY_PROCESS_NUMBER_AVG("process_number_avg");
constexpr StringView KEY_PROCESS_NUMBER_MAX("process_number_max");
constexpr StringView KEY_PROCESS_NUMBER_MIN("process_number_min");
constexpr StringView KEY_VM_PROCESS_MIN("vm_process_min");
constexpr StringView KEY_VM_PROCESS_MAX("vm_process_max");
constexpr StringView KEY_VM_PROCESS_AVG("vm_process_avg");

// Metric value keys - GPU
constexpr StringView KEY_GPU_DECODER_UTILIZATION_MAX("gpu_decoder_utilization_max");
constexpr StringView KEY_GPU_DECODER_UTILIZATION_MIN("gpu_decoder_utilization_min");
constexpr StringView KEY_GPU_DECODER_UTILIZATION_AVG("gpu_decoder_utilization_avg");
constexpr StringView KEY_GPU_ENCODER_UTILIZATION_MAX("gpu_encoder_utilization_max");
constexpr StringView KEY_GPU_ENCODER_UTILIZATION_MIN("gpu_encoder_utilization_min");
constexpr StringView KEY_GPU_ENCODER_UTILIZATION_AVG("gpu_encoder_utilization_avg");
constexpr StringView KEY_GPU_GPU_USEDUTILIZATION_MAX("gpu_gpu_usedutilization_max");
constexpr StringView KEY_GPU_GPU_USEDUTILIZATION_MIN("gpu_gpu_usedutilization_min");
constexpr StringView KEY_GPU_GPU_USEDUTILIZATION_AVG("gpu_gpu_usedutilization_avg");
constexpr StringView KEY_GPU_MEMORY_FREESPACE_MAX("gpu_memory_freespace_max");
constexpr StringView KEY_GPU_MEMORY_FREESPACE_MIN("gpu_memory_freespace_min");
constexpr StringView KEY_GPU_MEMORY_FREESPACE_AVG("gpu_memory_freespace_avg");
constexpr StringView KEY_GPU_MEMORY_FREEUTILIZATION_MAX("gpu_memory_freeutilization_max");
constexpr StringView KEY_GPU_MEMORY_FREEUTILIZATION_MIN("gpu_memory_freeutilization_min");
constexpr StringView KEY_GPU_MEMORY_FREEUTILIZATION_AVG("gpu_memory_freeutilization_avg");
constexpr StringView KEY_GPU_MEMORY_USEDSPACE_MAX("gpu_memory_usedspace_max");
constexpr StringView KEY_GPU_MEMORY_USEDSPACE_MIN("gpu_memory_usedspace_min");
constexpr StringView KEY_GPU_MEMORY_USEDSPACE_AVG("gpu_memory_usedspace_avg");
constexpr StringView KEY_GPU_MEMORY_USEDUTILIZATION_MAX("gpu_memory_usedutilization_max");
constexpr StringView KEY_GPU_MEMORY_USEDUTILIZATION_MIN("gpu_memory_usedutilization_min");
constexpr StringView KEY_GPU_MEMORY_USEDUTILIZATION_AVG("gpu_memory_usedutilization_avg");
constexpr StringView KEY_GPU_GPU_TEMPERATURE_MAX("gpu_gpu_temperature_max");
constexpr StringView KEY_GPU_GPU_TEMPERATURE_MIN("gpu_gpu_temperature_min");
constexpr StringView KEY_GPU_GPU_TEMPERATURE_AVG("gpu_gpu_temperature_avg");
constexpr StringView KEY_GPU_POWER_READINGS_POWER_DRAW_MAX("gpu_power_readings_power_draw_max");
constexpr StringView KEY_GPU_POWER_READINGS_POWER_DRAW_MIN("gpu_power_readings_power_draw_min");
constexpr StringView KEY_GPU_POWER_READINGS_POWER_DRAW_AVG("gpu_power_readings_power_draw_avg");

// Metric value keys - Disk
constexpr StringView KEY_DISKUSAGE_TOTAL_AVG("diskusage_total_avg");
constexpr StringView KEY_DISKUSAGE_TOTAL_MIN("diskusage_total_min");
constexpr StringView KEY_DISKUSAGE_TOTAL_MAX("diskusage_total_max");
constexpr StringView KEY_DISKUSAGE_USED_AVG("diskusage_used_avg");
constexpr StringView KEY_DISKUSAGE_USED_MIN("diskusage_used_min");
constexpr StringView KEY_DISKUSAGE_USED_MAX("diskusage_used_max");
constexpr StringView KEY_DISKUSAGE_FREE_AVG("diskusage_free_avg");
constexpr StringView KEY_DISKUSAGE_FREE_MIN("diskusage_free_min");
constexpr StringView KEY_DISKUSAGE_FREE_MAX("diskusage_free_max");
constexpr StringView KEY_DISKUSAGE_AVAIL_AVG("diskusage_avail_avg");
constexpr StringView KEY_DISKUSAGE_AVAIL_MIN("diskusage_avail_min");
constexpr StringView KEY_DISKUSAGE_AVAIL_MAX("diskusage_avail_max");
constexpr StringView KEY_DISKUSAGE_UTILIZATION_AVG("diskusage_utilization_avg");
constexpr StringView KEY_DISKUSAGE_UTILIZATION_MIN("diskusage_utilization_min");
constexpr StringView KEY_DISKUSAGE_UTILIZATION_MAX("diskusage_utilization_max");
constexpr StringView KEY_DISK_READIOPS_AVG("disk_readiops_avg");
constexpr StringView KEY_DISK_READIOPS_MIN("disk_readiops_min");
constexpr StringView KEY_DISK_READIOPS_MAX("disk_readiops_max");
constexpr StringView KEY_DISK_WRITEIOPS_AVG("disk_writeiops_avg");
constexpr StringView KEY_DISK_WRITEIOPS_MIN("disk_writeiops_min");
constexpr StringView KEY_DISK_WRITEIOPS_MAX("disk_writeiops_max");
constexpr StringView KEY_DISK_WRITEBYTES_AVG("disk_writebytes_avg");
constexpr StringView KEY_DISK_WRITEBYTES_MIN("disk_writebytes_min");
constexpr StringView KEY_DISK_WRITEBYTES_MAX("disk_writebytes_max");
constexpr StringView KEY_DISK_READBYTES_AVG("disk_readbytes_avg");
constexpr StringView KEY_DISK_READBYTES_MIN("disk_readbytes_min");
constexpr StringView KEY_DISK_READBYTES_MAX("disk_readbytes_max");
constexpr StringView KEY_FS_INODEUTILIZATION_AVG("fs_inodeutilization_avg");
constexpr StringView KEY_FS_INODEUTILIZATION_MIN("fs_inodeutilization_min");
constexpr StringView KEY_FS_INODEUTILIZATION_MAX("fs_inodeutilization_max");
constexpr StringView KEY_DISKIOQUEUESIZE_AVG("DiskIOQueueSize_avg");
constexpr StringView KEY_DISKIOQUEUESIZE_MIN("DiskIOQueueSize_min");
constexpr StringView KEY_DISKIOQUEUESIZE_MAX("DiskIOQueueSize_max");

// Metric value keys - System Load
constexpr StringView KEY_LOAD_1M_MIN("load_1m_min");
constexpr StringView KEY_LOAD_1M_MAX("load_1m_max");
constexpr StringView KEY_LOAD_1M_AVG("load_1m_avg");
constexpr StringView KEY_LOAD_5M_MIN("load_5m_min");
constexpr StringView KEY_LOAD_5M_MAX("load_5m_max");
constexpr StringView KEY_LOAD_5M_AVG("load_5m_avg");
constexpr StringView KEY_LOAD_15M_MIN("load_15m_min");
constexpr StringView KEY_LOAD_15M_MAX("load_15m_max");
constexpr StringView KEY_LOAD_15M_AVG("load_15m_avg");
constexpr StringView KEY_LOAD_PER_CORE_1M_MIN("load_per_core_1m_min");
constexpr StringView KEY_LOAD_PER_CORE_1M_MAX("load_per_core_1m_max");
constexpr StringView KEY_LOAD_PER_CORE_1M_AVG("load_per_core_1m_avg");
constexpr StringView KEY_LOAD_PER_CORE_5M_MIN("load_per_core_5m_min");
constexpr StringView KEY_LOAD_PER_CORE_5M_MAX("load_per_core_5m_max");
constexpr StringView KEY_LOAD_PER_CORE_5M_AVG("load_per_core_5m_avg");
constexpr StringView KEY_LOAD_PER_CORE_15M_MIN("load_per_core_15m_min");
constexpr StringView KEY_LOAD_PER_CORE_15M_MAX("load_per_core_15m_max");
constexpr StringView KEY_LOAD_PER_CORE_15M_AVG("load_per_core_15m_avg");

} // namespace logtail

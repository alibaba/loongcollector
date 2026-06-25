/*
 * Copyright 2023 iLogtail Authors
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

#include "plugin/input/InputEventTypeRegistry.h"

#include <algorithm>

namespace logtail {

const std::vector<InputEventTypeEntry>& GetNativeInputEventTypeEntries() {
    static const std::vector<InputEventTypeEntry> sEntries = {
        {"input_file", InputEventMask::LOG, "core/file_server/reader/LogFileReader.cpp", "file logs"},
        {"input_static_file_onetime", InputEventMask::LOG, "core/file_server/reader/LogFileReader.cpp", "file logs"},
        {"input_container_stdio", InputEventMask::LOG, "core/file_server/reader/LogFileReader.cpp", "container stdout/stderr logs"},
        {"input_prometheus", InputEventMask::METRIC, "core/prometheus/labels/TextParser.cpp", "prometheus samples"},
        {"input_host_monitor", InputEventMask::METRIC, "core/host_monitor/collector/*Collector.cpp", "host metrics"},
        {"input_host_meta", InputEventMask::LOG, "core/host_monitor/collector/ProcessEntityCollector.cpp", "process entity snapshots"},
        {"input_internal_metrics", InputEventMask::METRIC, "core/monitor/SelfMonitorServer.cpp", "self-monitor metrics"},
        {"input_internal_alarms", InputEventMask::LOG, "core/monitor/AlarmManager.cpp", "self-monitor alarms"},
        {"input_internal_config_container_info",
         InputEventMask::LOG,
         "core/container_manager/ContainerManager.cpp",
         "container config snapshots"},
        {"input_file_security", InputEventMask::LOG, "core/ebpf/plugin/file_security/FileSecurityManager.cpp", "security events"},
        {"input_network_security",
         InputEventMask::LOG,
         "core/ebpf/plugin/network_security/NetworkSecurityManager.cpp",
         "security events"},
        {"input_process_security",
         InputEventMask::LOG,
         "core/ebpf/plugin/process_security/ProcessSecurityManager.cpp",
         "security events"},
        {"input_agentsight", InputEventMask::LOG, "core/ebpf/plugin/agentsight/AgentsightManager.cpp", "agentsight events"},
        {"input_cpu_profiling",
         InputEventMask::LOG,
         "core/ebpf/plugin/cpu_profiling/CpuProfilingManager.cpp",
         "cpu profiling logs"},
        {"input_network_observer",
         InputEventMask::LOG | InputEventMask::METRIC | InputEventMask::SPAN,
         "core/ebpf/plugin/network_observer/NetworkObserverManager.cpp",
         "l7 logs + metrics + traces"},
        {"input_forward",
         InputEventMask::LOG | InputEventMask::METRIC | InputEventMask::SPAN,
         "core/plugin/processor/inner/ProcessorParseFromPBNative.cpp",
         "decoded from forwarded PipelineEventGroup payload"},
    };
    return sEntries;
}

bool InputProducesEventType(std::string_view inputType, PipelineEvent::Type eventType) {
    auto it = std::find_if(GetNativeInputEventTypeEntries().begin(),
                           GetNativeInputEventTypeEntries().end(),
                           [inputType](const InputEventTypeEntry& entry) { return entry.mInputType == inputType; });
    if (it == GetNativeInputEventTypeEntries().end()) {
        return false;
    }

    switch (eventType) {
        case PipelineEvent::Type::LOG:
            return HasMask(it->mMask, InputEventMask::LOG);
        case PipelineEvent::Type::METRIC:
            return HasMask(it->mMask, InputEventMask::METRIC);
        case PipelineEvent::Type::SPAN:
            return HasMask(it->mMask, InputEventMask::SPAN);
        default:
            return false;
    }
}

std::string InputEventMaskToString(InputEventMask mask) {
    std::string out;
    if (HasMask(mask, InputEventMask::LOG)) {
        out += "Log";
    }
    if (HasMask(mask, InputEventMask::METRIC)) {
        if (!out.empty()) {
            out += "|";
        }
        out += "Metric";
    }
    if (HasMask(mask, InputEventMask::SPAN)) {
        if (!out.empty()) {
            out += "|";
        }
        out += "Span";
    }
    if (out.empty()) {
        out = "None";
    }
    return out;
}

} // namespace logtail

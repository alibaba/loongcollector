#pragma once

#include <coolbpf/security/bpf_process_event_type.h>
#include <cstdint>

#include <memory>
#include <vector>

#include "CommonDataEvent.h"

namespace logtail::ebpf {

class ProcessEvent : public CommonEvent {
public:
    ProcessEvent(uint32_t pid, uint64_t ktime, KernelEventType type, uint64_t timestamp)
        : CommonEvent(pid, ktime, type, timestamp) {}
    [[nodiscard]] PluginType GetPluginType() const override { return PluginType::PROCESS_SECURITY; }
};

class ProcessExitEvent : public ProcessEvent {
public:
    ProcessExitEvent(
        uint32_t pid, uint64_t ktime, KernelEventType type, uint64_t timestamp, uint32_t exitCode, uint32_t exitTid)
        : ProcessEvent(pid, ktime, type, timestamp), mExitCode(exitCode), mExitTid(exitTid) {}
    uint32_t mExitCode;
    uint32_t mExitTid;
};

class ProcessEventGroup {
public:
    ProcessEventGroup(uint32_t pid, uint64_t ktime) : mPid(pid), mKtime(ktime) {}
    uint32_t mPid;
    uint64_t mKtime;
    // attrs
    std::vector<std::shared_ptr<CommonEvent>> mInnerEvents;
};

} // namespace logtail::ebpf

#pragma once

#include <coolbpf/security/bpf_process_event_type.h>
#include <cstdint>

#include <memory>
#include <string>
#include <vector>

#include "CommonDataEvent.h"
#include "common/StringView.h"
#include "ebpf/type/CommonDataEvent.h"

namespace logtail::ebpf {

class FileEvent : public CommonEvent {
public:
    FileEvent(uint32_t pid, uint64_t ktime, KernelEventType type, uint64_t timestamp)
        : CommonEvent(type), mPid(pid), mKtime(ktime), mTimestamp(timestamp) {}
    FileEvent(uint32_t pid, uint64_t ktime, KernelEventType type, uint64_t timestamp, StringView path)
        : CommonEvent(type), mPid(pid), mKtime(ktime), mTimestamp(timestamp), mPath(path.data(), path.size()) {}
    [[nodiscard]] PluginType GetPluginType() const override { return PluginType::FILE_SECURITY; };

    uint32_t mPid;
    uint64_t mKtime;
    uint64_t mTimestamp; // for kernel ts nano
    std::string mPath;
};

class FileEventGroup {
public:
    FileEventGroup(uint32_t pid, uint64_t ktime, const std::string& path) : mPid(pid), mKtime(ktime), mPath(path) {}
    uint32_t mPid;
    uint64_t mKtime;
    std::string mPath;
    // attrs
    std::vector<std::shared_ptr<CommonEvent>> mInnerEvents;
};

} // namespace logtail::ebpf

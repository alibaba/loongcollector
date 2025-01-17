#pragma once

#include <cstdint>
#include <stddef.h>
#include <string>
#include <vector>
#include <memory>

#include "ebpf/driver/coolbpf/src/security/bpf_process_event_type.h"
#include "CommonDataEvent.h"

namespace logtail {
namespace ebpf {

class FileEvent : public CommonEvent {
public:
    FileEvent(uint32_t pid, uint64_t ktime, KernelEventType type, uint64_t timestamp) : CommonEvent(pid,ktime,type,timestamp) {}
    FileEvent(uint32_t pid, uint64_t ktime, KernelEventType type, uint64_t timestamp, const std::string& path) : CommonEvent(pid,ktime,type,timestamp), mPath(path) {}
    virtual PluginType GetPluginType() const { return PluginType::FILE_SECURITY; };
    std::string mPath;
};

class FileEventGroup {
public:
    FileEventGroup(uint32_t pid, uint64_t ktime, const std::string& path) : mPid(pid), mKtime(ktime), mPath(path) {}
    uint32_t mPid;
    uint64_t mKtime;
    std::string mPath;
    // attrs
    std::vector<std::shared_ptr<FileEvent>> mInnerEvents;
};

}
}

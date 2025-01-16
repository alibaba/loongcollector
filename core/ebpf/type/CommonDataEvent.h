// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "ebpf/include/export.h"

namespace logtail {
namespace ebpf {

enum class KernelEventType {
    PROCESS_EXECVE_EVENT,
    PROCESS_CLONE_EVENT,
    PROCESS_EXIT_EVENT,
    PROCESS_DATA_EVENT,

    TCP_SENDMSG_EVENT,
    TCP_CONNECT_EVENT,
    TCP_CLOSE_EVENT,

    SYS_WRITE_EVENT,
    SYS_READ_EVENT,
    FILE_PERMISSION_EVENT,
};

class CommonEvent {
public:
    ~CommonEvent() {}
    explicit CommonEvent(uint32_t pid, uint64_t ktime, KernelEventType type, uint64_t timestamp) : mPid(pid), mKtime(ktime), mEventType(type), mTimestamp(timestamp) {}
    virtual PluginType GetPluginType() const = 0;
    virtual KernelEventType GetKernelEventType() const { return mEventType; };
    uint32_t mPid;
    uint64_t mKtime;
    KernelEventType mEventType;
    uint64_t mTimestamp; // for kernel ts nano
private:
    CommonEvent() = delete;
};

class ProcessEvent : public CommonEvent {
public:
    ProcessEvent(uint32_t pid, uint64_t ktime, KernelEventType type, uint64_t timestamp) : CommonEvent(pid,ktime,type,timestamp) {}
    virtual PluginType GetPluginType() const { return PluginType::PROCESS_SECURITY; };
};

class FileEvent : public CommonEvent {
public:
    FileEvent(uint32_t pid, uint64_t ktime, KernelEventType type, uint64_t timestamp) : CommonEvent(pid,ktime,type,timestamp) {}
    virtual PluginType GetPluginType() const { return PluginType::FILE_SECURITY; };
    std::string mPath;
};

class NetworkEvent : public CommonEvent {
public:
    NetworkEvent(uint32_t pid, uint64_t ktime, KernelEventType type, uint64_t timestamp) : CommonEvent(pid,ktime,type,timestamp) {}
    virtual PluginType GetPluginType() const { return PluginType::NETWORK_SECURITY; };
    // addr etc.
};

}
}

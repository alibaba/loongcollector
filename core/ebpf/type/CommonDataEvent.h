// Copyright 2025 iLogtail Authors
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

    FILE_PATH_TRUNCATE,
    FILE_MMAP,
    FILE_PERMISSION_EVENT,

    L7_RECORD,
    CONN_STATS_RECORD,

    FILE_PERMISSION_EVENT_WRITE,
    FILE_PERMISSION_EVENT_READ,
};

class CommonEvent {
public:
    explicit CommonEvent(KernelEventType type) : mEventType(type) {}
    virtual ~CommonEvent() {}

    [[nodiscard]] virtual PluginType GetPluginType() const = 0;
    [[nodiscard]] virtual KernelEventType GetKernelEventType() const { return mEventType; }
    KernelEventType mEventType;

private:
    CommonEvent() = delete;
};


} // namespace ebpf
} // namespace logtail

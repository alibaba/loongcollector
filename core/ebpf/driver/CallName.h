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

#include <coolbpf/security/type.h>

#include "Log.h"

namespace logtail::ebpf {

#define ERR_UNKNOWN_CALLNAME -1

static inline int GetCallNameIdx(const std::string& callName) {
    if (callName == "security_file_permission") {
        return SECURE_FUNC_TRACEPOINT_FUNC_SECURITY_FILE_PERMISSION;
    }
    if (callName == "security_mmap_file") {
        return SECURE_FUNC_TRACEPOINT_FUNC_SECURITY_MMAP_FILE;
    }
    if (callName == "security_path_truncate") {
        return SECURE_FUNC_TRACEPOINT_FUNC_SECURITY_PATH_TRUNCATE;
    }
    if (callName == "sys_write") {
        return SECURE_FUNC_TRACEPOINT_FUNC_SYS_WRITE;
    }
    if (callName == "sys_read") {
        return SECURE_FUNC_TRACEPOINT_FUNC_SYS_READ;
    }
    if (callName == "tcp_close") {
        return SECURE_FUNC_TRACEPOINT_FUNC_TCP_CLOSE;
    }
    if (callName == "tcp_connect") {
        return SECURE_FUNC_TRACEPOINT_FUNC_TCP_CONNECT;
    }
    if (callName == "tcp_sendmsg") {
        return SECURE_FUNC_TRACEPOINT_FUNC_TCP_SENDMSG;
    }
    ebpf_log(
        logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN, "[GetCallNameIdx] unknown call name: %s \n", callName.c_str());
    return ERR_UNKNOWN_CALLNAME;
}

} // namespace logtail::ebpf

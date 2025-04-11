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

#include <coolbpf/security/type.h>

#include "Log.h"

namespace logtail {
namespace ebpf {

static inline int GetCallNameIdx(const std::string& call_name) {
    if (call_name == "security_file_permission") {
        return SECURE_FUNC_TRACEPOINT_FUNC_SECURITY_FILE_PERMISSION;
    } else if (call_name == "security_mmap_file") {
        return SECURE_FUNC_TRACEPOINT_FUNC_SECURITY_MMAP_FILE;
    } else if (call_name == "security_path_truncate") {
        return SECURE_FUNC_TRACEPOINT_FUNC_SECURITY_PATH_TRUNCATE;
    } else if (call_name == "sys_write") {
        return SECURE_FUNC_TRACEPOINT_FUNC_SYS_WRITE;
    } else if (call_name == "sys_read") {
        return SECURE_FUNC_TRACEPOINT_FUNC_SYS_READ;
    } else if (call_name == "tcp_close") {
        return SECURE_FUNC_TRACEPOINT_FUNC_TCP_CLOSE;
    } else if (call_name == "tcp_connect") {
        return SECURE_FUNC_TRACEPOINT_FUNC_TCP_CONNECT;
    } else if (call_name == "tcp_sendmsg") {
        return SECURE_FUNC_TRACEPOINT_FUNC_TCP_SENDMSG;
    }
    ebpf_log(
        logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN, "[GetCallNameIdx] unknown call name: %s \n", call_name.c_str());
    return -1;
}

} // namespace ebpf
} // namespace logtail

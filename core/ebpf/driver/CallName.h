#pragma once

#include "Log.h"
#include "ebpf/driver/coolbpf/src/security/type.h"

namespace logtail {
namespace ebpf {

static inline int GetCallNameIdx(const std::string& call_name) {
    if (call_name == "security_file_permission")
        return SECURE_FUNC_TRACEPOINT_FUNC_SECURITY_FILE_PERMISSION;
    else if (call_name == "security_mmap_file")
        return SECURE_FUNC_TRACEPOINT_FUNC_SECURITY_MMAP_FILE;
    else if (call_name == "security_path_truncate")
        return SECURE_FUNC_TRACEPOINT_FUNC_SECURITY_PATH_TRUNCATE;
    else if (call_name == "sys_write")
        return SECURE_FUNC_TRACEPOINT_FUNC_SYS_WRITE;
    else if (call_name == "sys_read")
        return SECURE_FUNC_TRACEPOINT_FUNC_SYS_READ;
    else if (call_name == "tcp_close")
        return SECURE_FUNC_TRACEPOINT_FUNC_TCP_CLOSE;
    else if (call_name == "tcp_connect")
        return SECURE_FUNC_TRACEPOINT_FUNC_TCP_CONNECT;
    else if (call_name == "tcp_sendmsg")
        return SECURE_FUNC_TRACEPOINT_FUNC_TCP_SENDMSG;
    ebpf_log(
        logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN, "[GetCallNameIdx] unknown call name: %s \n", call_name.c_str());
    return -1;
}

} // namespace ebpf
} // namespace logtail

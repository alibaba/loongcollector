#pragma once

#include <coolbpf/security/bpf_process_event_type.h>
#include <cstdint>

#include <memory>
#include <string>
#include <vector>

#include "CommonDataEvent.h"

namespace logtail {
namespace ebpf {

struct MsgExecveEvent {
public:
    EXECVE_EVENT_COMMON_MEMBERS
};

struct MsgK8sUnix {
    std::string docker;
};

struct MsgUserRecord {
    std::string name;
};

struct MsgProcess {
    uint32_t size;
    uint32_t pid;
    uint32_t tid;
    uint32_t nspid;
    uint32_t secure_exec;
    uint32_t uid;
    uint32_t auid;
    uint32_t flags;
    uint32_t nlink;
    uint64_t ino;
    uint64_t ktime;
    std::string filename;
    std::string args;
    std::string cmdline;
    std::string cwd;
    std::string binary;
    MsgUserRecord user;
#ifdef APSARA_UNIT_TEST_MAIN
    std::string testFileName;
    std::string testCmdline;
#endif
};

class MsgExecveEventUnix {
public:
    MsgExecveEventUnix() : msg(nullptr) {}
    std::unique_ptr<MsgExecveEvent> msg;
    MsgK8sUnix kube;
    MsgProcess process;
    std::string exec_id;
    std::string parent_exec_id;
    bool kernel_thread;
    std::string tags;
};

struct Procs {
public:
    uint32_t psize;
    uint32_t ppid;
    uint32_t pnspid;
    uint32_t pflags;
    uint64_t pktime;
    std::string pcmdline;
    std::string pexe;
    uint32_t size;
    std::vector<uint32_t> uids;
    std::vector<uint32_t> gids;
    uint32_t pid;
    uint32_t tid;
    uint32_t nspid;
    uint32_t auid;
    uint32_t flags;
    uint64_t ktime;
    std::string cmdline;
    std::string exe;
    uint64_t effective;
    uint64_t inheritable;
    uint64_t permitted;
    uint32_t uts_ns;
    uint32_t ipc_ns;
    uint32_t mnt_ns;
    uint32_t pid_ns;
    uint32_t pid_for_children_ns;
    uint32_t net_ns;
    uint32_t time_ns;
    uint32_t time_for_children_ns;
    uint32_t cgroup_ns;
    uint32_t user_ns;
    bool kernel_thread;
};

class ProcessEvent : public CommonEvent {
public:
    ProcessEvent(uint32_t pid, uint64_t ktime, KernelEventType type, uint64_t timestamp)
        : CommonEvent(pid, ktime, type, timestamp) {}
    virtual PluginType GetPluginType() const { return PluginType::PROCESS_SECURITY; }
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

} // namespace ebpf
} // namespace logtail

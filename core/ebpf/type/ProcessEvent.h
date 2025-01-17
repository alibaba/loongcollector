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


/**
 * eBPF Type for Kernel Event
 */
struct MsgCommon {
    uint8_t op;
    uint8_t flags;
    uint8_t pad_v2[2];
    uint32_t size;
    uint64_t ktime;
};

struct MsgExecveKey {
    uint32_t pid;
    uint32_t pad;
    uint64_t ktime;
};

struct MsgCapabilities {
    uint64_t permitted;
    uint64_t effective;
    uint64_t inheritable;
};

struct MsgUserNamespace {
    int32_t level;
    uint32_t uid;
    uint32_t gid;
    uint32_t ns_inum;
};

struct MsgGenericCred {
    uint32_t uid;
    uint32_t gid;
    uint32_t suid;
    uint32_t sgid;
    uint32_t euid;
    uint32_t egid;
    uint32_t fsuid;
    uint32_t fsgid;
    uint32_t secure_bits;
    uint32_t pad;
    MsgCapabilities cap;
    MsgUserNamespace user_ns;
};

struct MsgK8s {
    uint32_t net_ns;
    uint32_t cid;
    uint64_t cgrpid;
    char docker[DOCKER_ID_LENGTH];
    //  std::array<char, DOCKER_ID_LENGTH> docker;
};

struct MsgNamespaces {
    uint32_t uts_inum;
    uint32_t ipc_inum;
    uint32_t mnt_inum;
    uint32_t pid_inum;
    uint32_t pid_child_inum;
    uint32_t net_inum;
    uint32_t time_inum;
    uint32_t time_child_inum;
    uint32_t cgroup_inum;
    uint32_t user_inum;
};

struct MsgExecveEvent {
public:
    MsgCommon common;
    MsgK8s kube;
    MsgExecveKey parent;
    uint64_t parent_flags;
    MsgGenericCred creds;
    MsgNamespaces namespaces;
    MsgExecveKey cleanup_process;
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
    MsgUserRecord user;
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
    inline void print() const;
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


class ProcessExitEvent : public ProcessEvent {
public:
    ProcessExitEvent(uint32_t pid, uint64_t ktime, KernelEventType type, uint64_t timestamp, uint32_t exitCode, uint32_t exitTid) 
        : ProcessEvent(pid,ktime,type,timestamp), mExitCode(exitCode), mExitTid(exitTid) {}
    uint32_t mExitCode;
    uint32_t mExitTid;
};

/// newest ...

// class ProcessEventNode {
// public:
//     ProcessEventNode(KernelEventType type, uint64_t ts) : mEventType(type), mTimestamp(ts) {}
//     KernelEventType mEventType;
//     uint64_t mTimestamp;
//     // attrs
    
// };

// std::vector<ProcessEventGroup>
class ProcessEventGroup {
public:
    ProcessEventGroup(uint32_t pid, uint64_t ktime) : mPid(pid), mKtime(ktime) {}
    uint32_t mPid;
    uint64_t mKtime;
    // attrs
    std::vector<std::shared_ptr<ProcessEvent>> mInnerEvents;
};

}
}

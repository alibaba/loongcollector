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

#include <cstdint>

#include <filesystem>
#include <string>
#include <vector>

#include "common/StringView.h"

using std::chrono::milliseconds;
using std::chrono::steady_clock;
using std::chrono::system_clock;

namespace logtail {

// TODO use definations in bpf_process_event_type.h
#define DOCKER_ID_LENGTH 128

struct Proc {
public:
    uint32_t ppid = 0U; // parent pid
    uint64_t pktime = 0U;

    uid_t realUid = 0;
    uid_t effectiveUid = 0;
    uid_t savedUid = 0;
    uid_t fsUid = 0;

    gid_t realGid = 0;
    gid_t effectiveGid = 0;
    gid_t savedGid = 0;
    gid_t fsGid = 0;

    uint32_t pid = 0;
    uint32_t tid = 0;
    uint32_t nspid = 0;
    uint32_t auid = 0; // Audit UID, loginuid
    uint32_t flags = 0;
    uint64_t ktime = 0;
    std::string cmdline; // \0 separated binary and args
    std::string comm;
    std::string cwd;
#ifdef APSARA_UNIT_TEST_MAIN
    std::string environ;
#endif
    std::string exe;
    std::string container_id;
    uint64_t effective = 0;
    uint64_t inheritable = 0;
    uint64_t permitted = 0;
    uint32_t uts_ns = 0;
    uint32_t ipc_ns = 0;
    uint32_t mnt_ns = 0;
    uint32_t pid_ns = 0;
    uint32_t pid_for_children_ns = 0;
    uint32_t net_ns = 0;
    uint32_t time_ns = 0;
    uint32_t time_for_children_ns = 0;
    uint32_t cgroup_ns = 0;
    uint32_t user_ns = 0;
};

struct ProcessStat {
    pid_t pid = 0;
    uint64_t vSize = 0;
    uint64_t rss = 0;
    uint64_t minorFaults = 0;
    uint64_t majorFaults = 0;
    pid_t parentPid = 0;
    int tty = 0;
    int priority = 0;
    int nice = 0;
    int numThreads = 0;
    int64_t startTicks = 0;

    uint64_t utimeTicks{0};
    uint64_t stimeTicks{0};
    uint64_t cutimeTicks{0};
    uint64_t cstimeTicks{0};

    std::string name;
    char state = '\0';
    int processor = 0;
};

// See https://man7.org/linux/man-pages/man5/proc.5.html
enum class EnumProcessStat : int {
    pid, // 0
    comm, // 1
    state, // 2
    ppid, // 3
    pgrp, // 4
    session, // 5
    tty_nr, // 6
    tpgid, // 7
    flags, // 8
    minflt, // 9
    cminflt, // 10
    majflt, // 11
    cmajflt, // 12
    utime, // 13
    stime, // 14
    cutime, // 15
    cstime, // 16
    priority, // 17
    nice, // 18
    num_threads, // 19
    itrealvalue, // 20
    starttime, // 21
    vsize, // 22
    rss, // 23
    rsslim, // 24
    startcode, // 25
    endcode, // 26
    startstack, // 27
    kstkesp, // 28
    kstkeip, // 29
    signal, // 30
    blocked, // 31
    sigignore, // 32
    sigcatch, // 33
    wchan, // 34
    nswap, // 35
    cnswap, // 36
    exit_signal, // 37
    processor, // 38 <--- 至少需要有该字段
    rt_priority, // 39
    policy, // 40
    delayacct_blkio_ticks, // 41
    guest_time, // 42
    cguest_time, // 43
    start_data, // 44
    end_data, // 45
    start_brk, // 46
    arg_start, // 47
    arg_end, // 48
    env_start, // 49
    env_end, // 50
    exit_code, // 51

    _count, // 只是用于计数，非实际字段
};
static_assert((int)EnumProcessStat::comm == 1, "EnumProcessStat invalid");
static_assert((int)EnumProcessStat::processor == 38, "EnumProcessStat invalid");

constexpr int operator-(EnumProcessStat a, EnumProcessStat b) {
    return (int)a - (int)b;
}

// 定义 ProcessStatus结构体，用于存储/proc/<pid>/status 文件解析结果
struct ProcessStatus {
    pid_t pid = 0;

    // UID信息 (real, effective, saved, filesystem)
    uid_t realUid = 0;
    uid_t effectiveUid = 0;
    uid_t savedUid = 0;
    uid_t fsUid = 0;
    // GID 信息(real, effective, saved, filesystem)
    gid_t realGid = 0;
    gid_t effectiveGid = 0;
    gid_t savedGid = 0;
    gid_t fsGid = 0;

    // 命名空间线程组ID
    std::vector<pid_t> nstgid;

    // 进程权限能力 (capabilities)
    uint64_t capPrm = 0; // 允许的权限能力
    uint64_t capEff = 0; // 有效的权限能力
    uint64_t capInh = 0; // 可继承的权限能力
};

class ProcParser {
public:
    explicit ProcParser(const std::string& prefix) : mProcPath(prefix + "/proc") {}
    bool ParseProc(uint32_t pid, Proc& proc) const;

    std::string GetPIDCmdline(uint32_t pid) const;
    std::string GetPIDComm(uint32_t pid) const;
    std::string GetPIDEnviron(uint32_t pid) const;
    uint32_t GetPIDCWD(uint32_t pid, std::string& cwd) const;
    bool ReadProcessStat(pid_t pid, ProcessStat& ps) const;
    bool ParseProcessStat(pid_t pid, const std::string& line, ProcessStat& ps) const;
    bool ReadProcessStatus(pid_t pid, ProcessStatus& ps) const;
    bool ParseProcessStatus(pid_t pid, const std::string& content, ProcessStatus& ps) const;
    int64_t GetStatsKtime(ProcessStat& procStat) const;
    uid_t GetLoginUid(uint32_t pid) const;

    std::string GetPIDDockerId(uint32_t) const;
    uint32_t GetPIDNsInode(uint32_t pid, const std::string& nsStr) const;
    std::string GetPIDExePath(uint32_t pid) const;
    std::tuple<std::string, std::string> ProcsFilename(const std::string& args);

    std::string GetUserNameByUid(uid_t uid);

private:
    std::filesystem::path procPidPath(uint32_t pid, const std::string& subpath) const;
    std::string readPidFile(uint32_t pid, const std::string& filename) const;
    std::string readPidLink(uint32_t pid, const std::string& filename) const;
    StringView lookupContainerId(const StringView& cgroupline) const;
    bool isValidContainerId(const StringView& id) const;

    std::filesystem::path mProcPath;

    static constexpr size_t kContainerIdLength = 64;
    static constexpr size_t kCgroupNameLength = 128;
};
} // namespace logtail

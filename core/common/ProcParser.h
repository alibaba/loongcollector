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
#include <utility>
#include <vector>

namespace logtail {

// TODO use definations in bpf_process_event_type.h
#define DOCKER_ID_LENGTH 128

struct Proc {
public:
    uint32_t ppid = 0U; // parent pid
    uint64_t pktime = 0U;
    // uint32_t pnspid;
    // uint32_t pflags;
    // std::string pcmdline;
    // std::string pexe;
    std::vector<uint32_t> uids = {0, 0, 0, 0}; // Real UID, Effective UID, Saved Set-UID, Filesystem UID
    std::vector<uint32_t> gids = {0, 0, 0, 0};
    ;
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

struct Status {
public:
    std::vector<std::string> uids;
    std::vector<std::string> gids;
    std::string loginUid;
    std::vector<uint32_t> GetUids() const { return ConvertToInt(uids); }

    std::vector<uint32_t> GetGids() const { return ConvertToInt(gids); }

    uint32_t GetLoginUid() const { return ConvertToInt(loginUid); }

private:
    std::vector<uint32_t> ConvertToInt(const std::vector<std::string>& ids) const;

    uint32_t ConvertToInt(const std::string& id) const;
};

class ProcStat {
public:
    ProcStat() {
        buffer.reserve(512);
        stats.reserve(52); // 52 is the number of fields in proc/stat
    }
    std::string buffer;
    std::vector<std::string_view> stats;
};

class ProcParser {
public:
    ProcParser(const std::string& prefix) : mProcPath(prefix + "/proc") {}
    bool ParseProc(uint32_t pid, Proc& proc) const;

    std::string GetPIDCmdline(uint32_t pid) const;
    std::string GetPIDComm(uint32_t pid) const;
    std::string GetPIDEnviron(uint32_t pid) const;
    std::pair<std::string, uint32_t> GetPIDCWD(uint32_t) const;
    int GetProcStatStrings(uint32_t pid, ProcStat& stat) const;
    int64_t GetStatsKtime(ProcStat& procStat) const;
    int GetStatus(uint32_t pid, Status& status) const;

    std::tuple<uint32_t, uint64_t, uint64_t, uint64_t> GetPIDCaps(uint32_t pid) const;
    std::string GetPIDDockerId(uint32_t) const;
    uint32_t GetPIDNsInode(uint32_t pid, const std::string& nsStr) const;
    std::string GetPIDExePath(uint32_t pid) const;
    std::tuple<std::string, int> LookupContainerId(const std::string& cgroup, bool bpfSource, bool walkParent) const;
    std::tuple<std::string, std::string> ProcsFilename(const std::string& args);

    std::string GetUserNameByUid(uid_t uid);

private:
    std::filesystem::path ProcPidPath(uint32_t pid, const std::string& subpath) const;
    int FillStatus(uint32_t pid, Status& status) const;
    int FillLoginUid(uint32_t pid, Status& status) const;
    std::string ReadPIDFile(uint32_t pid, const std::string& filename) const;
    std::string ReadPIDLink(uint32_t pid, const std::string& filename) const;
    std::tuple<std::string, int> ProcsFindDockerId(const std::string& cgroups) const;
    std::vector<std::string> split(const std::string& str, char delimiter) const;
    std::tuple<std::string, int> ProcsContainerIdOffset(const std::string& subdir) const;

    std::filesystem::path mProcPath;

    static constexpr size_t kContainerIdLength = 64;
    static constexpr size_t kBpfContainerIdLength = 64;
    //  const int DOCKER_ID_LENGTH = 128;
    static constexpr size_t kCgroupNameLength = 128;
};
} // namespace logtail

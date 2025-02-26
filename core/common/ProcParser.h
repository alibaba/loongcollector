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

#include <filesystem>
#include <istream>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace logtail {

// TODO use definations in bpf_process_event_type.h
#define DOCKER_ID_LENGTH 128

enum class ApiEventFlag : uint32_t {
    Unknown = 0x00,
    Execve = 0x01,
    ExecveAt = 0x02,
    ProcFS = 0x04,
    TruncFilename = 0x08,
    TruncArgs = 0x10,
    TaskWalk = 0x20,
    Miss = 0x40,
    NeedsAUID = 0x80,
    ErrorFilename = 0x100,
    ErrorArgs = 0x200,
    NeedsCWD = 0x400,
    NoCWDSupport = 0x800,
    RootCWD = 0x1000,
    ErrorCWD = 0x2000,
    Clone = 0x4000,
    ErrorCgroupName = 0x010000,
    ErrorCgroupKn = 0x020000,
    ErrorCgroupSubsysCgrp = 0x040000,
    ErrorCgroupSubsys = 0x080000,
    ErrorCgroups = 0x100000,
    ErrorCgroupId = 0x200000,
    ErrorPathComponents = 0x400000,
    DataFilename = 0x800000,
    DataArgs = 0x1000000
};

inline ApiEventFlag operator|(ApiEventFlag lhs, ApiEventFlag rhs) {
    return static_cast<ApiEventFlag>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline ApiEventFlag& operator|=(ApiEventFlag& lhs, ApiEventFlag rhs) {
    lhs = lhs | rhs;
    return lhs;
}

inline bool operator&(ApiEventFlag lhs, ApiEventFlag rhs) {
    return (static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)) != 0;
}

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
    uint32_t invalid_uid_ = UINT32_MAX;
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
    static constexpr int64_t kNanoPerSeconds = 1000000000;
    static constexpr int64_t kClktck = 100;
};
} // namespace logtail

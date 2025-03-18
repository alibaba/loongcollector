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

#include <charconv>
#include <climits>
#include <coolbpf/security/bpf_process_event_type.h>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "common/TimeUtil.h"
#if defined(__linux__)
#include <pwd.h>
#endif

#include "Logger.h"
#include "ProcParser.h"
#include "common/StringTools.h"

namespace logtail {

std::vector<uint32_t> Status::ConvertToInt(const std::vector<std::string>& ids) const {
    std::vector<uint32_t> result;
    for (const auto& id : ids) {
        try {
            auto num = std::stoi(id);
            result.push_back(num);
        } catch (std::out_of_range& e) {
            result.push_back(UINT32_MAX);
            LOG_WARNING(sLogger, ("[ConvertToInt] vec id", id)("e", e.what()));
        }
    }
    return result;
}

uint32_t Status::ConvertToInt(const std::string& id) const {
    try {
        unsigned long res = std::stoul(id);
        return static_cast<uint32_t>(res);
    } catch (std::out_of_range& e) {
        LOG_WARNING(sLogger, ("[ConvertToInt] u32 id", id)("e", e.what()));
    } catch (std::invalid_argument& ee) {
        LOG_WARNING(sLogger, ("[ConvertToInt] u32 id", id)("e", ee.what()));
    }
    return UINT32_MAX;
}

std::filesystem::path ProcParser::ProcPidPath(uint32_t pid, const std::string& subpath) const {
    return mProcPath / std::to_string(pid) / subpath;
}

std::string ProcParser::ReadPIDLink(uint32_t pid, const std::string& filename) const {
    const auto fpath = ProcPidPath(pid, filename);
    std::error_code ec;
    std::string netStr = std::filesystem::read_symlink(fpath, ec).string();
    if (ec) {
        LOG_DEBUG(sLogger, ("[ReadPIDLink] failed pid", pid)("filename", filename)("e", ec.message()));
        return "";
    }
    return netStr;
}

std::string ProcParser::ReadPIDFile(uint32_t pid, const std::string& filename) const {
    std::filesystem::path fpath = mProcPath / std::to_string(pid) / filename;
    std::ifstream ifs(fpath);
    if (!ifs) {
        return "";
    }
    try {
        std::string res((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        if (!res.empty() && res[res.size() - 1] == 0) {
            res.pop_back();
        }
        return res;
    } catch (const std::ios_base::failure& e) {
    }
    return "";
}

bool ProcParser::ParseProc(uint32_t pid, Proc& proc) const {
    proc.pid = pid;
    proc.tid = pid;
    proc.cmdline = GetPIDCmdline(pid);
    proc.comm = GetPIDComm(pid);
    proc.exe = GetPIDExePath(pid);

    std::tie(proc.cwd, proc.flags) = GetPIDCWD(pid);
    proc.flags |= static_cast<uint32_t>(EVENT_PROCFS | EVENT_NEEDS_CWD | EVENT_NEEDS_AUID);

    ProcStat stats;
    if (0 != GetProcStatStrings(pid, stats)) {
        LOG_WARNING(sLogger, ("GetProcStatStrings", "failed"));
        return false;
    }
    if (stats.stats.size() < 4) {
        LOG_WARNING(sLogger, ("GetProcStatStrings", "failed"));
        return false;
    }
    auto ppid = stats.stats[3];

    // get ppid
    if (!StringTo(ppid, proc.ppid)) {
        LOG_WARNING(sLogger, ("Parse ppid", "failed"));
        return false;
    }
    proc.ktime = GetStatsKtime(stats);

    Status status;
    if (0 != GetStatus(pid, status)) {
        LOG_WARNING(sLogger, ("GetStatus failed", "failed"));
        return false;
    }
    proc.uids = status.GetUids();
    proc.gids = status.GetGids();
    proc.auid = status.GetLoginUid();


    std::tie(proc.nspid, proc.permitted, proc.effective, proc.inheritable) = GetPIDCaps(pid);
    proc.uts_ns = GetPIDNsInode(pid, "uts");
    proc.ipc_ns = GetPIDNsInode(pid, "ipc");
    proc.mnt_ns = GetPIDNsInode(pid, "mnt");
    proc.pid_ns = GetPIDNsInode(pid, "pid");
    proc.pid_for_children_ns = GetPIDNsInode(pid, "pid_for_children");
    proc.net_ns = GetPIDNsInode(pid, "net");
    proc.cgroup_ns = GetPIDNsInode(pid, "cgroup");
    proc.user_ns = GetPIDNsInode(pid, "user");
    proc.time_ns = GetPIDNsInode(pid, "time");
    proc.time_for_children_ns = GetPIDNsInode(pid, "time_for_children");

    proc.container_id = GetPIDDockerId(pid);
    if (proc.container_id.empty()) {
        proc.nspid = 0;
    }

    if (proc.ppid) {
        // proc.pcmdline = GetPIDCmdline(proc.ppid);
        // auto parentComm = GetPIDComm(proc.ppid);
        ProcStat parentStats;
        GetProcStatStrings(proc.ppid, parentStats);
        proc.pktime = GetStatsKtime(parentStats);
        // proc.pexe = GetPIDExePath(proc.ppid);
        // auto [pnspid, ppermitted, peffective, pinheritable] = GetPIDCaps(proc.ppid);
        // std::string pDockerId = GetPIDDockerId(proc.ppid);
        // if (pDockerId.empty()) {
        //     pnspid = 0;
        // }
        // proc.pnspid = pnspid;
        // proc.pflags = static_cast<uint32_t>(EVENT_PROCFS | EVENT_NEEDS_CWD | EVENT_NEEDS_AUID);
    }
    return true;
}

std::string ProcParser::GetPIDCmdline(uint32_t pid) const {
    return ReadPIDFile(pid, "cmdline");
}

std::string ProcParser::GetPIDComm(uint32_t pid) const {
    return ReadPIDFile(pid, "comm");
}

std::string ProcParser::GetPIDEnviron(uint32_t pid) const {
    return ReadPIDFile(pid, "environ");
}

std::tuple<std::string, int> ProcParser::ProcsContainerIdOffset(const std::string& subdir) const {
    size_t p = subdir.rfind(':') + 1;
    std::vector<std::string> fields = SplitString(subdir, ":");
    std::string idStr = fields.back();
    size_t off = idStr.rfind('-') + 1;
    std::vector<std::string> s = SplitString(idStr, "-");
    return {s.back(), static_cast<int>(off + p)};
}

std::tuple<std::string, int>
ProcParser::LookupContainerId(const std::string& cgroup, bool bpfSource, bool walkParent) const {
    bool idTruncated = false;
    std::vector<std::string> subDirs = SplitString(cgroup, "/");
    std::string subdir = subDirs.back();

    if (subdir.find("syscont-cgroup-root") != std::string::npos) {
        if (subDirs.size() > 4) {
            subdir = subDirs[4];
            walkParent = false;
        }
    }

    if (bpfSource && subdir.size() >= DOCKER_ID_LENGTH - 1) {
        idTruncated = true;
    }

    auto [container, i] = ProcsContainerIdOffset(subdir);

    if (container.size() >= kContainerIdLength || (idTruncated && container.size() >= kBpfContainerIdLength)) {
        return {container.substr(0, kBpfContainerIdLength), i};
    }

    if (cgroup.find("libpod") != std::string::npos && container == "container") {
        walkParent = true;
    }

    if (!walkParent) {
        return {"", 0};
    }

    for (int j = subDirs.size() - 2; j > 1; --j) {
        auto [container, i] = ProcsContainerIdOffset(subDirs[j]);
        if (container.size() == kContainerIdLength
            || (container.size() > kContainerIdLength && container.find("scope") != std::string::npos)) {
            return {container.substr(0, kBpfContainerIdLength), i};
        }
    }
    return {"", 0};
}

std::tuple<std::string, int> ProcParser::ProcsFindDockerId(const std::string& cgroups) const {
    std::vector<std::string> cgrpPaths = SplitString(cgroups, "\n");
    for (const auto& s : cgrpPaths) {
        if (s.find("pods") != std::string::npos || s.find("docker") != std::string::npos
            || s.find("libpod") != std::string::npos) {
            auto [container, i] = LookupContainerId(s, false, false);
            if (!container.empty()) {
                LOG_DEBUG(sLogger, ("[ProcsFindDockerId] containerid", container));
                return {container, i};
            }
        }
    }
    return {"", 0};
}

std::string ProcParser::GetPIDDockerId(uint32_t pid) const {
    std::string cgroups = ReadPIDFile(pid, "cgroup");
    auto [dockerId, offset] = ProcsFindDockerId(cgroups);
    LOG_DEBUG(sLogger, ("[GetPIDDockerId] failed, pid:", pid)("containerid", dockerId));
    return dockerId;
}

std::string ProcParser::GetPIDExePath(uint32_t pid) const {
    return ReadPIDLink(pid, "exe");
}

std::pair<std::string, uint32_t> ProcParser::GetPIDCWD(uint32_t pid) const {
    uint32_t flags = EVENT_UNKNOWN;
    if (pid == 0) {
        return {"", static_cast<uint32_t>(flags)};
    }

    try {
        std::string cwd = ReadPIDLink(pid, "cwd");
        if (cwd == "/") {
            flags |= EVENT_ROOT_CWD;
        }
        return {cwd, static_cast<uint32_t>(flags)};
    } catch (const std::filesystem::filesystem_error&) { // possibly kernel thread
        flags |= EVENT_ROOT_CWD | EVENT_ERROR_CWD;
        return {"", static_cast<uint32_t>(flags)};
    }
}

std::string ProcParser::GetUserNameByUid(uid_t uid) {
    static std::string sEmpty;
#if defined(__linux__)
    thread_local static std::unordered_map<uid_t, std::string> sUserNameCache;

    auto it = sUserNameCache.find(uid);
    if (it != sUserNameCache.end()) {
        return it->second;
    }
    struct passwd pwd {};
    struct passwd* result = nullptr;
    char buf[8192]; // This buffer size is quite large. If it's still not enough, it's unusual and we return an empty
                    // result.

    int ret = getpwuid_r(uid, &pwd, buf, sizeof(buf), &result);
    if (ret == 0 && result) {
        if (sUserNameCache.size() > 100000) { // If we have too many entries, reset the cache.
            sUserNameCache.clear();
        }
        sUserNameCache[uid] = pwd.pw_name;
        return sUserNameCache[uid];
    }
    return sEmpty;
#elif defined(_MSC_VER)
    return sEmpty;
#endif
}

// TODO: 实现与其他读status的合并一下
std::tuple<uint32_t, uint64_t, uint64_t, uint64_t> ProcParser::GetPIDCaps(uint32_t originPid) const {
    uint32_t pid = 0;
    uint64_t permitted = 0;
    uint64_t effective = 0;
    uint64_t inheritable = 0;

    auto getValue64Hex = [](const std::string& line) -> std::tuple<uint64_t, std::string> {
        std::istringstream stream(line);
        std::string field;
        uint64_t value = 0;

        while (stream >> field) {
        }
        auto [ptr, ec] = std::from_chars(field.data(), field.data() + field.size(), value, 16);
        if (ec == std::errc::invalid_argument) {
            return {0, "Invalid argument in line: " + line};
        }
        if (ec == std::errc::result_out_of_range) {
            return {0, "Out of range in line: " + line};
        }
        return {value, ""};
    };

    auto getValue32Int = [](const std::string& line) -> std::tuple<uint32_t, std::string> {
        std::istringstream stream(line);
        std::string field;
        uint32_t value = 0;

        while (stream >> field) {
        }
        auto [ptr, ec] = std::from_chars(field.data(), field.data() + field.size(), value);
        if (ec == std::errc::invalid_argument) {
            return {0, "Invalid argument in line: " + line};
        }
        if (ec == std::errc::result_out_of_range) {
            return {0, "Out of range in line: " + line};
        }
        return {value, ""};
    };

    std::string filename = mProcPath / std::to_string(originPid) / "status";
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_WARNING(sLogger, ("ReadFile failed", filename));
        return {0, 0, 0, 0};
    }

    std::string line;
    while (std::getline(file, line)) {
        std::string err;
        if (line.find("NStgid:") != std::string::npos) {
            std::tie(pid, err) = getValue32Int(line);
        } else if (line.find("CapPrm:") != std::string::npos) {
            std::tie(permitted, err) = getValue64Hex(line);
        } else if (line.find("CapEff:") != std::string::npos) {
            std::tie(effective, err) = getValue64Hex(line);
        } else if (line.find("CapInh:") != std::string::npos) {
            std::tie(inheritable, err) = getValue64Hex(line);
        }
        if (!err.empty()) {
            LOG_WARNING(sLogger, ("ReadFile error, filename", filename)("line", line));
        }
    }

    return {pid, permitted, effective, inheritable};
}

int64_t ProcParser::GetStatsKtime(ProcStat& procStat) const {
    if (procStat.stats.size() <= 21) {
        return -1;
    }

    int64_t ktime = 0L;
    if (!StringTo(procStat.stats[21], ktime)) {
        LOG_WARNING(sLogger, ("Parse ktime", "failed"));
        return -1;
    }
    return ktime * (kNanoPerSeconds / GetTicksPerSecond());
}

uint32_t ProcParser::GetPIDNsInode(uint32_t pid, const std::string& nsStr) const {
    std::string pidStr = std::to_string(pid);
    std::filesystem::path netns = std::filesystem::path(mProcPath) / pidStr / "ns" / nsStr;

    std::error_code ec;
    std::string netStr = std::filesystem::read_symlink(netns, ec).string();
    if (ec) {
        LOG_WARNING(sLogger, ("namespace", netns)("error", ec.message()));
        return 0;
    }

    std::vector<std::string> fields = SplitString(netStr, ":");
    if (fields.size() < 2) {
        LOG_WARNING(sLogger, ("parsing namespace fields less than 2, net str ", netStr)("netns", netns));
        return 0;
    }
    auto openPos = netStr.find('[');
    auto closePos = netStr.find_last_of(']');
    if (openPos == std::string::npos || closePos == std::string::npos || openPos + 1 >= closePos) {
        LOG_WARNING(sLogger, ("Invalid argument in line: ", netStr));
        return 0;
    }
    uint32_t inodeEntry = 0;
    if (!StringTo(netStr.data() + openPos + 1, netStr.data() + closePos, inodeEntry)) {
        LOG_WARNING(sLogger, ("Invalid argument in line: ", netStr));
        return 0;
    }
    return inodeEntry;
}

int ProcParser::FillStatus(uint32_t pid, Status& status) const {
    const auto path = ProcPidPath(pid, "status");

    std::ifstream f(path);

    if (!f) {
        LOG_WARNING(sLogger, ("open failed, path", path));
        return -1;
    }

    std::string line;
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::vector<std::string> fields;
        fields.reserve(256);
        std::string field;
        while (iss >> field) {
            fields.push_back(field);
        }
        if (fields.size() < 2) {
            continue;
        }
        if (fields[0] == "Uid:") {
            if (fields.size() != 5) {
                LOG_WARNING(sLogger, ("Reading Uid failed: malformed input, path", path));
                return -1;
            }
            status.uids = {fields[1], fields[2], fields[3], fields[4]};
        }
        if (fields[0] == "Gid:") {
            if (fields.size() != 5) {
                LOG_WARNING(sLogger, ("Reading Gid failed: malformed input, path", path));
                return -1;
            }
            status.gids = {fields[1], fields[2], fields[3], fields[4]};
        }
        if (!status.uids.empty() && !status.gids.empty()) {
            break;
        }
    }
    return 0;
}

int ProcParser::FillLoginUid(uint32_t pid, Status& status) const {
    try {
        std::string loginUid = ReadPIDFile(pid, "loginuid");
        status.loginUid = loginUid;
    } catch (std::runtime_error& error) {
        return -1;
    }
    return 0;
}

// TODO @qianlu.kk
int ProcParser::GetStatus(uint32_t pid, Status& status) const {
    if (FillStatus(pid, status) != 0 || FillLoginUid(pid, status) != 0) {
        return -1;
    }
    return 0;
}

std::tuple<std::string, std::string> ProcParser::ProcsFilename(const std::string& args) {
    std::string filename;
    std::string cmds;
    size_t idx = args.find('\0');

    if (idx == std::string::npos) {
        filename = args;
    } else {
        cmds = args.substr(idx);
        filename = args.substr(0, idx);
    }

    return std::make_tuple(cmds, filename);
}

int ProcParser::GetProcStatStrings(uint32_t pid, ProcStat& stat) const {
    std::string path = mProcPath / std::to_string(pid) / "stat";
    std::ifstream file(path);
    if (!file) {
        return -1;
    }
    try {
        stat.buffer.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    } catch (const std::ios_base::failure& e) {
        return -1;
    }
    auto& statline = stat.buffer;
    auto& output = stat.stats;

    size_t oldIndex = statline.length();
    size_t index = statline.find_last_of(' ');

    // Build list of strings in reverse order
    while (index != std::string::npos) {
        output.emplace_back(statline.data() + index + 1, oldIndex - index - 1);
        if (statline[index - 1] == ')') {
            break;
        }
        oldIndex = index;
        index = statline.find_last_of(' ', oldIndex - 1);
    }

    if (index == std::string::npos) {
        output.emplace_back(statline.data(), oldIndex);
    } else {
        size_t commIndex = statline.find_first_of(' ');
        output.emplace_back(statline.data() + commIndex + 1, index - commIndex - 1);
        output.emplace_back(statline.data(), commIndex);
    }

    // Reverse the array
    std::reverse(output.begin(), output.end());
    return 0;
}
} // namespace logtail


// ProcParser::GetPIDStartTimeTicks(uint32_t pid) const {
// return ::px::system::GetPIDStartTimeTicks(ProcPidPath(pid));
// }

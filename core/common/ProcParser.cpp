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

#include <charconv>
#include <climits>
#include <coolbpf/security/bpf_process_event_type.h>
#include <cstddef>
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

#include "FileSystemUtil.h"
#include "StringView.h"
#include "common/TimeUtil.h"
#if defined(__linux__)
#include <pwd.h>
#endif

#include "Logger.h"
#include "ProcParser.h"
#include "common/StringTools.h"

namespace logtail {

std::filesystem::path ProcParser::procPidPath(uint32_t pid, const std::string& subpath) const {
    return mProcPath / std::to_string(pid) / subpath;
}

std::string ProcParser::readPidLink(uint32_t pid, const std::string& filename) const {
    const auto fpath = procPidPath(pid, filename);
    std::error_code ec;
    std::string netStr = std::filesystem::read_symlink(fpath, ec).string();
    if (ec) {
        LOG_DEBUG(sLogger, ("[ReadPIDLink] failed pid", pid)("filename", filename)("e", ec.message()));
        return "";
    }
    return netStr;
}

std::string ProcParser::readPidFile(uint32_t pid, const std::string& filename) const {
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

    proc.flags = GetPIDCWD(pid, proc.cwd);
    proc.flags |= static_cast<uint32_t>(EVENT_PROCFS | EVENT_NEEDS_CWD | EVENT_NEEDS_AUID);

    ProcessStat stats;
    if (!ReadProcessStat(pid, stats)) {
        LOG_WARNING(sLogger, ("GetProcStatStrings", "failed"));
        return false;
    }

    proc.ppid = stats.parentPid;
    proc.ktime = GetStatsKtime(stats);

    ProcessStatus status;
    if (!ReadProcessStatus(pid, status)) {
        LOG_WARNING(sLogger, ("GetStatus failed", "failed"));
        return false;
    }
    proc.realUid = status.realUid;
    proc.effectiveUid = status.effectiveUid;
    proc.savedUid = status.savedUid;
    proc.fsUid = status.fsUid;
    proc.realGid = status.realGid;
    proc.effectiveGid = status.effectiveGid;
    proc.savedGid = status.savedGid;
    proc.fsGid = status.fsGid;
    proc.nspid = status.nstgid[0];
    proc.permitted = status.capPrm;
    proc.effective = status.capEff;
    proc.inheritable = status.capInh;

    proc.auid = GetLoginUid(pid);

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
        ProcessStat parentStats;
        ReadProcessStat(proc.ppid, parentStats);
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
    return readPidFile(pid, "cmdline");
}

std::string ProcParser::GetPIDComm(uint32_t pid) const {
    return readPidFile(pid, "comm");
}

std::string ProcParser::GetPIDEnviron(uint32_t pid) const {
    return readPidFile(pid, "environ");
}

bool ProcParser::isValidContainerId(const StringView& id) const {
    // 检查长度是否匹配
    if (id.size() != kContainerIdLength) {
        return false;
    }
    // 这里假设合法 container id 只包含十六进制字符（即 0-9 和 a-f / A-F)
    for (char ch : id) {
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) {
            return false;
        }
    }
    return true;
}

StringView ProcParser::lookupContainerId(const StringView& cgroupline) const {
    if (cgroupline.length() <= kContainerIdLength
        || (cgroupline.find("pods") == std::string::npos && cgroupline.find("docker") == std::string::npos
            && cgroupline.find("containerd") == std::string::npos && cgroupline.find("libpod") == std::string::npos
            && cgroupline.find("lxc") == std::string::npos && cgroupline.find("podman") == std::string::npos
            && cgroupline.find("cri-") == std::string::npos)) {
        return kEmptyStringView;
    }
    auto lastSlash = cgroupline.rfind('/');
    if (lastSlash != std::string::npos && lastSlash + 1 < cgroupline.size()) {
        auto potentialId = cgroupline.substr(lastSlash + 1);
        auto lastDash = potentialId.rfind('-');
        if (lastDash != std::string::npos && lastDash + 1 < potentialId.size()) {
            potentialId = potentialId.substr(lastDash + 1);
        }
        // 如果末尾有".scope"则去除
        if (potentialId.size() > kContainerIdLength && potentialId.find("scope") != std::string::npos) {
            potentialId = potentialId.substr(0, kContainerIdLength);
        }
        if (isValidContainerId(potentialId)) {
            return potentialId;
        }
    }
    return kEmptyStringView;
}

std::string ProcParser::GetPIDDockerId(uint32_t pid) const {
    std::string cgroups = readPidFile(pid, "cgroup");
    StringViewSplitter splitter(cgroups, "\n");
    for (const auto& line : splitter) {
        auto container = lookupContainerId(line);
        if (!container.empty()) {
            LOG_DEBUG(sLogger, ("[ProcsFindDockerId] containerid", container));
            return container.to_string();
        }
    }
    return "";
}

std::string ProcParser::GetPIDExePath(uint32_t pid) const {
    return readPidLink(pid, "exe");
}

uint32_t ProcParser::GetPIDCWD(uint32_t pid, std::string& cwd) const {
    cwd.clear();
    uint32_t flags = EVENT_UNKNOWN;
    if (pid == 0) {
        return flags;
    }

    try {
        cwd = readPidLink(pid, "cwd");
        if (cwd == "/") {
            flags |= EVENT_ROOT_CWD;
        }
        return flags;
    } catch (const std::filesystem::filesystem_error&) { // possibly kernel thread
        flags |= EVENT_ROOT_CWD | EVENT_ERROR_CWD;
        return flags;
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
        if (sUserNameCache.size() > 10000) { // If we have too many entries, reset the cache.
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

int64_t ProcParser::GetStatsKtime(ProcessStat& procStat) const {
    return procStat.startTicks * kNanoPerSeconds / GetTicksPerSecond();
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
        LOG_WARNING(sLogger, ("Invalid NsInode: ", netStr));
        return 0;
    }
    uint32_t inodeEntry = 0;
    if (!StringTo(netStr.data() + openPos + 1, netStr.data() + closePos, inodeEntry)) {
        LOG_WARNING(sLogger, ("Invalid NsInode: ", netStr));
        return 0;
    }
    return inodeEntry;
}

uid_t ProcParser::GetLoginUid(uint32_t pid) const {
    uid_t loginUid = 0;
    std::string loginStr = readPidFile(pid, "loginuid");
    if (!StringTo(loginStr, loginUid)) {
        LOG_WARNING(sLogger, ("Invalid loginuid: ", loginStr));
    }
    return loginUid;
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

bool ProcParser::ReadProcessStat(pid_t pid, ProcessStat& ps) const {
    LOG_DEBUG(sLogger, ("read process stat", pid));
    auto processStat = mProcPath / std::to_string(pid) / "stat";

    std::string line;
    if (!ReadFileContent(processStat.string(), line)) {
        LOG_ERROR(sLogger, ("read process stat", "fail")("file", processStat));
        return false;
    }
    return ParseProcessStat(pid, line, ps);
}

// 数据样例: /proc/1/stat
// 1 (cat) R 0 1 1 34816 1 4194560 1110 0 0 0 1 1 0 0 20 0 1 0 18938584 4505600 171 18446744073709551615 4194304 4238788
// 140727020025920 0 0 0 0 0 0 0 0 0 17 3 0 0 0 0 0 6336016 6337300 21442560 140727020027760 140727020027777
// 140727020027777 140727020027887 0
bool ProcParser::ParseProcessStat(pid_t pid, const std::string& line, ProcessStat& ps) const {
    ps.pid = pid;
    auto nameStartPos = line.find_first_of('(');
    auto nameEndPos = line.find_last_of(')');
    if (nameStartPos == std::string::npos || nameEndPos == std::string::npos || nameStartPos >= nameEndPos) {
        LOG_ERROR(sLogger, ("can't find process name", pid)("stat", line));
        return false;
    }
    nameStartPos++; // 跳过左括号
    ps.name = line.substr(nameStartPos, nameEndPos - nameStartPos);
    StringView lineview = StringView(line).substr(nameEndPos + 2); // 跳过右括号及空格

    std::array<StringView, size_t(EnumProcessStat::_count)> words{};
    StringViewSplitter splitter(lineview, " ");
    size_t i = 0;
    for (const auto& word : splitter) {
        if (i >= words.size()) {
            break;
        }
        words[i++] = word;
    }

    constexpr const EnumProcessStat offset = EnumProcessStat::state; // 跳过pid, comm
    constexpr const int minCount = EnumProcessStat::processor - offset + 1; // 37
    if (words.size() < minCount) {
        LOG_ERROR(sLogger, ("unexpected item count", pid)("stat", line));
        return false;
    }

    if (!words[EnumProcessStat::state - offset].empty()) {
        ps.state = words[EnumProcessStat::state - offset].front();
    }
    if (!StringTo(words[EnumProcessStat::ppid - offset], ps.parentPid)) {
        LOG_WARNING(sLogger, ("Invalid ppid:", words[EnumProcessStat::ppid - offset]));
    }
    if (!StringTo(words[EnumProcessStat::tty_nr - offset], ps.tty)) {
        LOG_WARNING(sLogger, ("Invalid tty_nr:", words[EnumProcessStat::tty_nr - offset]));
    }
    if (!StringTo(words[EnumProcessStat::minflt - offset], ps.minorFaults)) {
        LOG_WARNING(sLogger, ("Invalid minflt:", words[EnumProcessStat::minflt - offset]));
    }
    if (!StringTo(words[EnumProcessStat::majflt - offset], ps.majorFaults)) {
        LOG_WARNING(sLogger, ("Invalid majflt:", words[EnumProcessStat::majflt - offset]));
    }
    if (!StringTo(words[EnumProcessStat::utime - offset], ps.utimeTicks)) {
        LOG_WARNING(sLogger, ("Invalid utime:", words[EnumProcessStat::utime - offset]));
    }
    if (!StringTo(words[EnumProcessStat::stime - offset], ps.stimeTicks)) {
        LOG_WARNING(sLogger, ("Invalid stime:", words[EnumProcessStat::stime - offset]));
    }
    if (!StringTo(words[EnumProcessStat::cutime - offset], ps.cutimeTicks)) {
        LOG_WARNING(sLogger, ("Invalid cutime:", words[EnumProcessStat::cutime - offset]));
    }
    if (!StringTo(words[EnumProcessStat::cstime - offset], ps.cstimeTicks)) {
        LOG_WARNING(sLogger, ("Invalid cstime:", words[EnumProcessStat::cstime - offset]));
    }
    if (!StringTo(words[EnumProcessStat::priority - offset], ps.priority)) {
        LOG_WARNING(sLogger, ("Invalid priority:", words[EnumProcessStat::priority - offset]));
    }
    if (!StringTo(words[EnumProcessStat::nice - offset], ps.nice)) {
        LOG_WARNING(sLogger, ("Invalid nice:", words[EnumProcessStat::nice - offset]));
    }
    if (!StringTo(words[EnumProcessStat::num_threads - offset], ps.numThreads)) {
        LOG_WARNING(sLogger, ("Invalid num_threads:", words[EnumProcessStat::num_threads - offset]));
    }
    if (!StringTo(words[EnumProcessStat::starttime - offset], ps.startTicks)) {
        LOG_WARNING(sLogger, ("Invalid starttime:", words[EnumProcessStat::starttime - offset]));
    }
    if (!StringTo(words[EnumProcessStat::vsize - offset], ps.vSize)) {
        LOG_WARNING(sLogger, ("Invalid vsize:", words[EnumProcessStat::vsize - offset]));
    }
    if (!StringTo(words[EnumProcessStat::rss - offset], ps.rss)) {
        LOG_WARNING(sLogger, ("Invalid rss:", words[EnumProcessStat::rss - offset]));
    } else {
        ps.rss <<= getpagesize();
    }
    if (!StringTo(words[EnumProcessStat::processor - offset], ps.processor)) {
        LOG_WARNING(sLogger, ("Invalid processor:", words[EnumProcessStat::processor - offset]));
    }
    return true;
}

// 读取 /proc/<pid>/status 文件
bool ProcParser::ReadProcessStatus(pid_t pid, ProcessStatus& ps) const {
    LOG_DEBUG(sLogger, ("read process status", pid));
    auto processStatus = mProcPath / std::to_string(pid) / "status";

    std::string content;
    if (!ReadFileContent(processStatus.string(), content)) {
        LOG_ERROR(sLogger, ("read process status", "fail")("file", processStatus));
        return false;
    }
    return ParseProcessStatus(pid, content, ps);
}

// 解析/proc/<pid>/status 文件内容
// 数据样例:
// Name:	bash
// Umask:	0022
// State:	S (sleeping)
// Tgid:	17248
// ...
// Uid:	1000	10001000	1000
// Gid:	100	100	100	100
// ...
// NStgid:	17248	1
// ...
// CapInh:	0000000000000000
// CapPrm:	0000000000000000
// CapEff:	0000000000000000
// ...
bool ProcParser::ParseProcessStatus(pid_t pid, const std::string& content, ProcessStatus& ps) const {
    ps.pid = pid;

    StringViewSplitter lineSplitter(StringView(content), "\n");
    for (const auto& line : lineSplitter) {
        auto colonPos = line.find(':');
        if (colonPos == StringView::npos || colonPos == line.size() - 1) {
            continue;
        }

        StringView key = line.substr(0, colonPos);
        StringView value = line.substr(colonPos + 1);

        // 去除前导空格和制表符
        while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
            value.remove_prefix(1);
        }

        if (key == "Uid") {
            // Uid: real_uid effective_uid saved_uid fs_uid
            StringViewSplitter uidSplitter(value, "\t");
            size_t index = 0;
            for (const auto& part : uidSplitter) {
                if (part.empty()) {
                    continue;
                }

                switch (index) {
                    case 0:
                        if (!StringTo(part, ps.realUid)) {
                            LOG_WARNING(sLogger, ("Invalid real_uid:", part));
                        }
                        break;
                    case 1:
                        if (!StringTo(part, ps.effectiveUid)) {
                            LOG_WARNING(sLogger, ("Invalid effective_uid:", part));
                        }
                        break;
                    case 2:
                        if (!StringTo(part, ps.savedUid)) {
                            LOG_WARNING(sLogger, ("Invalid saved_uid:", part));
                        }
                        break;
                    case 3:
                        if (!StringTo(part, ps.fsUid)) {
                            LOG_WARNING(sLogger, ("Invalid fs_uid:", part));
                        }
                        break;
                    default:
                        break;
                }
                ++index;
            }
        } else if (key == "Gid") {
            // Gid: real_gid effective_gid saved_gid fs_gid
            StringViewSplitter gidSplitter(value, "\t");
            size_t index = 0;
            for (const auto& part : gidSplitter) {
                if (part.empty()) {
                    continue;
                }

                switch (index) {
                    case 0:
                        if (!StringTo(part, ps.realGid)) {
                            LOG_WARNING(sLogger, ("Invalid real_gid:", part));
                        }
                        break;
                    case 1:
                        if (!StringTo(part, ps.effectiveGid)) {
                            LOG_WARNING(sLogger, ("Invalid effective_gid:", part));
                        }
                        break;
                    case 2:
                        if (!StringTo(part, ps.savedGid)) {
                            LOG_WARNING(sLogger, ("Invalid saved_gid:", part));
                        }
                        break;
                    case 3:
                        if (!StringTo(part, ps.fsGid)) {
                            LOG_WARNING(sLogger, ("Invalid fs_gid:", part));
                        }
                        break;
                    default:
                        break;
                }
                ++index;
            }
        } else if (key == "NStgid") {
            // NStgid: namespace_tgid [namespace_tgid ...]
            ps.nstgid.clear();
            StringViewSplitter nstgidSplitter(value, "\t");
            for (const auto& part : nstgidSplitter) {
                if (!part.empty()) {
                    pid_t nstgid = 0;
                    if (!StringTo(part, nstgid)) {
                        LOG_WARNING(sLogger, ("Invalid nstgid:", value));
                    }
                    ps.nstgid.push_back(nstgid);
                }
            }
        } else if (key == "CapPrm") { // CapPrm: 16进制表示的权限掩码
            if (!StringTo(value, ps.capPrm, 16)) {
                LOG_WARNING(sLogger, ("Invalid CapPrm:", value));
            }
        } else if (key == "CapEff") { // CapEff: 16进制表示的权限掩码
            if (!StringTo(value, ps.capEff, 16)) {
                LOG_WARNING(sLogger, ("Invalid CapEff:", value));
            }
        } else if (key == "CapInh") { // CapInh: 16进制表示的权限掩码
            if (!StringTo(value, ps.capInh, 16)) {
                LOG_WARNING(sLogger, ("Invalid CapInh:", value));
            }
        }
        // 可以根据需要解析更多字段...
    }
    return true;
}

} // namespace logtail

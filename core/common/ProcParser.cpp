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

#include <cstring>
#include <limits.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>

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
    try {
        return std::filesystem::path(proc_path_) / std::to_string(pid) / subpath;
    } catch (std::exception& exception) {
        return std::filesystem::path();
    }
}

std::string ProcParser::ReadPIDLink(uint32_t pid, const std::string& filename) const {
    const auto fpath = ProcPidPath(pid, filename);
    try {
        return std::filesystem::read_symlink(fpath).string();
    } catch (std::filesystem::filesystem_error& e) {
        LOG_DEBUG(sLogger, ("[ReadPIDLink] failed pid", pid)("filename", filename)("e", e.what()));
        return "";
    }
}

std::string ProcParser::ReadPIDFile(uint32_t pid, const std::string& filename, const std::string& delimiter) const {
    //  const auto fpath = ProcPidPath(pid, filename);
    std::filesystem::path procRoot = proc_path_;
    std::filesystem::path fpath = procRoot / std::to_string(pid) / filename;
    std::ifstream ifs(fpath);
    if (!ifs) {
        return "";
    }
    std::string line;
    size_t fileSize = ifs.tellg();
    std::string res;
    res.reserve(fileSize << 1);
    while (std::getline(ifs, line)) {
        if (delimiter == "") {
            res += line;
        } else {
            res += delimiter + line;
        }
    }
    // Strip out extra null character at the end of the string.
    if (!res.empty() && res[res.size() - 1] == 0) {
        res.pop_back();
    }
    // Replace all nulls with spaces. Sometimes the command line has
    // null to separate arguments and others it has spaces. We just make them all spaces
    // and leave it to upstream code to tokenize properly.
    std::replace(res.begin(), res.end(), static_cast<char>(0), ' ');
    return res;
}

std::string ProcParser::GetPIDCmdline(uint32_t pid) const {
    return ReadPIDFile(pid, "cmdline", "");
}

std::string ProcParser::GetPIDComm(uint32_t pid) const {
    return ReadPIDFile(pid, "comm", "");
}

std::string ProcParser::GetPIDEnviron(uint32_t pid) const {
    return ReadPIDFile(pid, "environ", "");
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

    if (container.size() >= ContainerIdLength || (idTruncated && container.size() >= BpfContainerIdLength)) {
        return {container.substr(0, BpfContainerIdLength), i};
    }

    if (cgroup.find("libpod") != std::string::npos && container == "container") {
        walkParent = true;
    }

    if (!walkParent) {
        return {"", 0};
    }

    for (int j = subDirs.size() - 2; j > 1; --j) {
        auto [container, i] = ProcsContainerIdOffset(subDirs[j]);
        if (container.size() == ContainerIdLength
            || (container.size() > ContainerIdLength && container.find("scope") != std::string::npos)) {
            return {container.substr(0, BpfContainerIdLength), i};
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
    std::string cgroups = ReadPIDFile(pid, "cgroup", "\n");
    auto [dockerId, offset] = ProcsFindDockerId(cgroups);
    LOG_DEBUG(sLogger, ("[GetPIDDockerId] failed, pid:", pid)("containerid", dockerId));
    return dockerId;
}

std::string ProcParser::GetPIDExePath(uint32_t pid) const {
    return ReadPIDLink(pid, "exe");
}

std::pair<std::string, uint32_t> ProcParser::GetPIDCWD(uint32_t pid) const {
    ApiEventFlag flags = ApiEventFlag::Unknown;
    if (pid == 0) {
        return {"", static_cast<uint32_t>(flags)};
    }

    try {
        std::string cwd = ReadPIDLink(pid, "cwd");
        if (cwd == "/") {
            flags |= ApiEventFlag::RootCWD;
        }
        return {cwd, static_cast<uint32_t>(flags)};
    } catch (const std::filesystem::filesystem_error&) {
        flags |= (ApiEventFlag::RootCWD | ApiEventFlag::ErrorCWD);
        return {"", static_cast<uint32_t>(flags)};
    }
}

std::string ProcParser::GetUserNameByUid(uid_t uid) {
#if defined(__linux__)
    passwd* pw = getpwuid(uid);
    if (pw)
        return std::string(pw->pw_name);
    else
        return "";
#elif defined(_MSC_VER)
    return "";
#endif
}

std::tuple<uint32_t, uint64_t, uint64_t, uint64_t> ProcParser::GetPIDCaps(uint32_t origin_pid) const {
    uint32_t pid = 0;
    uint64_t permitted = 0;
    uint64_t effective = 0;
    uint64_t inheritable = 0;

    auto getValue64Hex = [](const std::string& line) -> std::tuple<uint64_t, std::string> {
        std::istringstream stream(line);
        std::string field;
        uint64_t value;

        while (stream >> field)
            ;
        try {
            value = std::stoull(field, nullptr, 16);
        } catch (const std::invalid_argument&) {
            return {0, "Invalid argument in line: " + line};
        } catch (const std::out_of_range&) {
            return {0, "Out of range in line: " + line};
        }
        return {value, ""};
    };

    auto getValue32Int = [](const std::string& line) -> std::tuple<uint32_t, std::string> {
        std::istringstream stream(line);
        std::string field;
        uint32_t value;

        while (stream >> field)
            ;
        try {
            value = std::stoul(field);
        } catch (const std::invalid_argument&) {
            return {0, "Invalid argument in line: " + line};
        } catch (const std::out_of_range&) {
            return {0, "Out of range in line: " + line};
        }
        return {value, ""};
    };

    std::string filename = proc_path_ + "/" + std::to_string(origin_pid) + "/status";
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

uint64_t ProcParser::GetStatsKtime(std::vector<std::string>& proc_stat) const {
    if (proc_stat.size() <= 21) {
        throw std::out_of_range("Index 21 is out of range for the input vector");
    }

    try {
        uint64_t ktime = std::stoull(proc_stat[21]);
        return ktime * (nanoPerSeconds / clktck);
    } catch (const std::invalid_argument& e) {
        LOG_WARNING(sLogger, ("Invalid argument, e", e.what()));
        return 0;
    } catch (const std::out_of_range& e) {
        LOG_WARNING(sLogger, ("Out of range, proc stat size", proc_stat.size()));
        return 0;
    }
}

uint32_t ProcParser::GetPIDNsInode(uint32_t pid, const std::string& ns_str) const {
    std::string pidStr = std::to_string(pid);
    std::filesystem::path netns = std::filesystem::path(proc_path_) / pidStr / "ns" / ns_str;

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

    std::string inode = fields[1];
    inode = inode.substr(1, inode.size() - 2); // Remove [ and ]
    uint64_t inodeEntry;
    try {
        inodeEntry = std::stoull(inode);
    } catch (const std::invalid_argument& e) {
        LOG_WARNING(sLogger, ("Invalid argument, e", e.what()));
        return 0;
    } catch (const std::out_of_range& e) {
        LOG_WARNING(sLogger, ("Out of range:, e", e.what()));
        return 0;
    }

    return static_cast<uint32_t>(inodeEntry);
}

int ProcParser::FillStatus(uint32_t pid, std::shared_ptr<Status> status) const {
    const auto path = ProcPidPath(pid, "status");

    std::ifstream f(path);

    if (!f.is_open()) {
        LOG_WARNING(sLogger, ("open failed, path", path));
        return -1;
    }

    std::string line;
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::vector<std::string> fields;
        std::string field;
        while (iss >> field) {
            fields.push_back(field);
        }
        if (fields.size() < 2)
            continue;
        if (fields[0] == "Uid:") {
            if (fields.size() != 5) {
                LOG_WARNING(sLogger, ("Reading Uid failed: malformed input, path", path));
                return -1;
            }
            status->uids = {fields[1], fields[2], fields[3], fields[4]};
        }
        if (fields[0] == "Gid:") {
            if (fields.size() != 5) {
                LOG_WARNING(sLogger, ("Reading Gid failed: malformed input, path", path));
                return -1;
            }
            status->gids = {fields[1], fields[2], fields[3], fields[4]};
        }
        if (!status->uids.empty() && !status->gids.empty()) {
            break;
        }
    }
    return 0;
}

int ProcParser::FillLoginUid(uint32_t pid, std::shared_ptr<Status> status) const {
    try {
        std::string login_uid = ReadPIDFile(pid, "loginuid", "");
        status->login_uid = login_uid;
    } catch (std::runtime_error& error) {
        return -1;
    }
    return 0;
}

// TODO @qianlu.kk
std::shared_ptr<Status> ProcParser::GetStatus(uint32_t pid) const {
    auto status = std::make_shared<Status>();

    if (FillStatus(pid, status) != 0 || FillLoginUid(pid, status) != 0) {
        return nullptr;
    }
    return status;
}

std::tuple<std::string, std::string> ProcParser::ProcsFilename(const std::string& args) {
    std::string filename = args;
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

std::vector<std::string> ProcParser::GetProcStatStrings(uint32_t pid) const {
    std::string path = proc_path_ + "/" + std::to_string(pid) + "/stat";
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("ReadFile: " + path + "/stat error");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string statline = buffer.str();

    std::vector<std::string> output;
    size_t oldIndex = statline.length();
    size_t index = statline.find_last_of(' ');

    // Build list of strings in reverse order
    while (index != std::string::npos) {
        output.push_back(statline.substr(index + 1, oldIndex - index - 1));
        if (statline[index - 1] == ')') {
            break;
        }
        oldIndex = index;
        index = statline.find_last_of(' ', oldIndex - 1);
    }

    if (index == std::string::npos) {
        output.push_back(statline.substr(0, oldIndex));
    } else {
        size_t commIndex = statline.find_first_of(' ');
        output.push_back(statline.substr(commIndex + 1, index - commIndex - 1));
        output.push_back(statline.substr(0, commIndex));
    }

    // Reverse the array
    std::reverse(output.begin(), output.end());

    return output;
}
} // namespace logtail


// ProcParser::GetPIDStartTimeTicks(uint32_t pid) const {
// return ::px::system::GetPIDStartTimeTicks(ProcPidPath(pid));
// }

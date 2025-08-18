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

#include <string>
#include <fstream>
#include "boost/filesystem.hpp"

struct ProcessEntry {
    std::string mCmdline;
    uint32_t mPid;

    ProcessEntry(std::string cmdline, uint32_t pid)
        : mCmdline(std::move(cmdline)), mPid(pid) {}
};

inline void ListAllProcesses(std::vector<ProcessEntry> &proc_out) {
    assert(proc_out.empty());
    boost::filesystem::path procPath("/proc");
    for (const auto &entry : boost::filesystem::directory_iterator(procPath)) {
        std::string pidStr = entry.path().filename().string();
        assert(!pidStr.empty());
        if (!std::all_of(pidStr.begin(), pidStr.end(), ::isdigit)) {
            continue;
        }
        uint32_t pid = std::stoi(pidStr);
        boost::filesystem::path cmdlinePath = entry.path() / "cmdline";
        std::ifstream cmdlineFile(cmdlinePath.string());
        if (!cmdlineFile.is_open()) {
            continue;
        }

        std::string cmdline;
        std::getline(cmdlineFile, cmdline);
        if (cmdline.empty()) {
            continue;
        }

        // /proc/<pid>/cmdline use '\0' as separator, replace it with space
        std::replace(cmdline.begin(), cmdline.end(), '\0', ' ');

        proc_out.emplace_back(cmdline, pid);
    }
}
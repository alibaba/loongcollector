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

#include "common/ProcParser.h"

namespace logtail {
namespace ebpf {

struct ProcessEntry {
    uint32_t mPid;
    std::string mCmdline;
    std::string mContainerId;

    ProcessEntry(uint32_t pid, std::string cmdline, std::string containerId)
        : mPid(pid), mCmdline(std::move(cmdline)), mContainerId(containerId) {}
};

inline void ListAllProcesses(ProcParser &procParser, std::vector<ProcessEntry> &proc_out) {
    assert(proc_out.empty());

    auto pids = procParser.GetAllPids();
    for (auto& pid : pids) {
        auto cmdline = procParser.GetPIDCmdline(pid);
        if (cmdline.empty()) {
            continue; // process exit or no perm
        }
        // /proc/<pid>/cmdline use '\0' as separator, replace it with space
        std::replace(cmdline.begin(), cmdline.end(), '\0', ' ');

        std::string containerId;
        // ok if containerId is empty
        procParser.GetPIDDockerId(pid, containerId);

        proc_out.emplace_back(pid, std::move(cmdline), std::move(containerId));
    }
}

} // namespace ebpf
} // namespace logtail
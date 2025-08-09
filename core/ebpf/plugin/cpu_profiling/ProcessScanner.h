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

#include <atomic>
#include <functional>
#include <future>
#include <string>
#include <vector>

#include "boost/regex.hpp"

namespace logtail {
namespace ebpf {

struct ProcessScanOption {
    using Callback = std::function<void(std::vector<uint32_t> pids)>;

    std::string mName;
    std::vector<std::string> mRegexs;
    Callback mCallback;
};

class ProcessScanner {
public:
    ProcessScanner(const ProcessScanner &) = delete;
    ProcessScanner &operator=(const ProcessScanner &) = delete;
    ProcessScanner(ProcessScanner &&) = delete;
    ProcessScanner &operator=(ProcessScanner &&) = delete;

    ~ProcessScanner() { Stop(); }

    static ProcessScanner *GetInstance() {
        static ProcessScanner instance;
        return &instance;
    }

    void Start();
    void Stop();

    void Pause();
    void Resume();

    void RegisterScan(const ProcessScanOption &option);
    void RemoveScan(const std::string &name);

private:
    ProcessScanner() = default;

    void scannerThreadFunc();

    struct ProcessEntry {
        std::string cmdline;
        uint32_t pid;

        ProcessEntry(std::string cmdline, uint32_t pid)
            : cmdline(std::move(cmdline)), pid(pid) {}
    };

    static void listAllProcesses(std::vector<ProcessEntry> &proc_out);

    struct ScanState {
        std::vector<boost::regex> mRegexs;
        ProcessScanOption::Callback mCallback;
        std::vector<uint32_t> mPrevPids;
    };

    static void findMatchedProcesses(const std::vector<ProcessEntry> &procs,
                                     const ScanState &state,
                                     std::vector<uint32_t> &matchedPids);

    std::atomic_bool mRunning = false;
    std::future<void> mThreadRes;
    bool mPaused = false;
    std::mutex mLock;
    std::condition_variable mCond;

    std::unordered_map<std::string, ScanState> mScanStates;
};

} // namespace ebpf
} // namespace logtail

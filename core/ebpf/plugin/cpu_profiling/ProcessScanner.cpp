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

#include "boost/filesystem.hpp"
#include <cassert>
#include <thread>

#include "ebpf/plugin/cpu_profiling/ProcessScanner.h"
#include "logger/Logger.h"

namespace logtail {
namespace ebpf {

void ProcessScanner::Start() {
    if (mRunning) {
        return;
    }
    mRunning = true;
    mThreadRes = std::async(std::launch::async,
                            &ProcessScanner::scannerThreadFunc, this);
}

void ProcessScanner::Stop() {
    if (mRunning == false) {
        return;
    }
    if (!mThreadRes.valid()) {
        return;
    }
    mRunning = false;
    Resume();
    mThreadRes.wait();
    LOG_INFO(sLogger, ("ProcessScanner", "stop"));
}

void ProcessScanner::Pause() {
    std::lock_guard<std::mutex> guard(mLock);
    if (mPaused) {
        return;
    }
    mPaused = true;
    LOG_INFO(sLogger, ("ProcessScanner", "pause"));
}

void ProcessScanner::Resume() {
    std::lock_guard<std::mutex> guard(mLock);
    if (mPaused) {
        mPaused = false;
        mCond.notify_one();
        LOG_INFO(sLogger, ("ProcessScanner", "resume"));
    }
}

int ProcessScanner::RegisterScan(const ProcessScanOption &option) {
    std::lock_guard<std::mutex> guard(mLock);
    auto it = mScanStates.emplace(option.mName, ScanState{}).first;
    auto &state = it->second;
    state.mRegexs.clear();
    for (auto &regexStr : option.mRegexs) {
        try {
            state.mRegexs.emplace_back(regexStr);
        } catch (const boost::regex_error &e) {
            LOG_ERROR(sLogger,
                      ("Failed to compile regex", regexStr)("error", e.what()));
            mScanStates.erase(it);
            return -1;
        }
    }
    state.mCallback = option.mCallback;
    return 0;
}

void ProcessScanner::RemoveScan(const std::string &name) {
    std::lock_guard<std::mutex> guard(mLock);
    mScanStates.erase(name);
};

void ProcessScanner::listAllProcesses(std::vector<ProcessEntry> &proc_out) {
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

void ProcessScanner::findMatchedProcesses(
    const std::vector<ProcessEntry> &procs, const ScanState &state,
    std::vector<uint32_t> &matchedPids) {
    assert(matchedPids.empty());
    if (state.mRegexs.empty()) {
        // If no regexes are provided, return all process PIDs
        matchedPids.reserve(procs.size());
        for (const auto &proc : procs) {
            matchedPids.push_back(proc.pid);
        }
        return;
    }
    for (const auto &proc : procs) {
        for (const auto &regex : state.mRegexs) {
            if (boost::regex_match(proc.cmdline, regex)) {
                matchedPids.push_back(proc.pid);
                break; // No need to check other regexes for this process
            }
        }
    }
}

static bool isSame(const std::vector<uint32_t> &a,
                   const std::vector<uint32_t> &b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

void ProcessScanner::scannerThreadFunc() {
    while (mRunning) {
        std::vector<ProcessEntry> procs;
        listAllProcesses(procs);

        std::vector<
            std::pair<ProcessScanOption::Callback, std::vector<uint32_t>>>
            results;

        {
            std::unique_lock<std::mutex> lock(mLock);
            if (mPaused) {
                mCond.wait(lock, [&] { return !mPaused; });
                continue;
            }

            for (auto &[_, state] : mScanStates) {
                std::vector<uint32_t> matchedPids;
                findMatchedProcesses(procs, state, matchedPids);
                if (matchedPids.empty()) {
                    continue;
                }
                std::sort(matchedPids.begin(), matchedPids.end());
                if (isSame(matchedPids, state.mPrevPids)) {
                    continue; // No change in matched PIDs
                }
                state.mPrevPids = matchedPids;
                // Store the callback and matched PIDs for later processing
                results.emplace_back(state.mCallback, std::move(matchedPids));
            }
        }

        for (auto &[callback, pids] : results) {
            assert(!pids.empty());
            callback(std::move(pids));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

} // namespace ebpf
} // namespace logtail

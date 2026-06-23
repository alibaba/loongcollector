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

#include "ebpf/plugin/cpu_profiling/ProcessDiscoveryManager.h"

#include <cassert>

#include <chrono>
#include <thread>

#include "logger/Logger.h"

namespace logtail {
namespace ebpf {

void ProcessDiscoveryManager::Start(NotifyFn fn,
                                    size_t milliseconds,
                                    const std::string& hostRootPath,
                                    StatsFn statsFn) {
    if (mRunning) {
        return;
    }
    mProcParser.emplace(hostRootPath);
    mRunning = true;
    mCallback = std::move(fn);
    mStatsCallback = std::move(statsFn);
    mSleepMilliseconds = milliseconds;
    mThreadRes = std::async(std::launch::async, &ProcessDiscoveryManager::run, this);
    LOG_INFO(sLogger, ("ProcessDiscoveryManager", "start"));
}

void ProcessDiscoveryManager::Stop() {
    if (mRunning == false) {
        return;
    }
    mRunning = false;
    mSleepCv.notify_all();
    if (mThreadRes.valid()) {
        mThreadRes.wait();
    }
    mCallback = nullptr;
    mStatsCallback = nullptr;
    LOG_INFO(sLogger, ("ProcessDiscoveryManager", "stop"));
}

void ProcessDiscoveryManager::AddDiscovery(const std::string& configName, ProcessDiscoveryConfig config) {
    std::lock_guard<std::mutex> guard(mLock);
    auto it = mStates.emplace(configName, InnerState{}).first;
    auto& state = it->second;
    state.mConfig = std::move(config);
}

bool ProcessDiscoveryManager::UpdateDiscovery(const std::string& configName, const ContainerDiff& diff) {
    std::lock_guard<std::mutex> guard(mLock);
    auto it = mStates.find(configName);
    if (it == mStates.end()) {
        return false;
    }
    auto& state = it->second;
    auto& config = state.mConfig;
    for (const auto& containerId : diff.mRemoved) {
        config.mContainerIds.erase(containerId);
    }
    for (const auto& container : diff.mAdded) {
        config.mContainerIds.insert(container->mID);
    }
    return true;
}

void ProcessDiscoveryManager::RemoveDiscovery(const std::string& configName) {
    std::lock_guard<std::mutex> guard(mLock);
    mStates.erase(configName);
}

bool ProcessDiscoveryManager::CheckDiscoveryExist(const std::string& configName) {
    std::lock_guard<std::mutex> guard(mLock);
    return mStates.find(configName) != mStates.end();
}

void ProcessDiscoveryManager::run() {
    if (!mProcParser.has_value()) {
        LOG_ERROR(sLogger, ("ProcessDiscoveryManager", "ProcParser is not initialized"));
        return;
    }
    while (mRunning) {
        auto pidsUnorder = mProcParser->GetAllPids();
        std::vector<uint32_t> pids(pidsUnorder.begin(), pidsUnorder.end());
        std::sort(pids.begin(), pids.end());

        std::vector<DiscoverEntry> result;
        size_t pidMatchCacheSize = 0;

        {
            std::lock_guard<std::mutex> guard(mLock);

            for (auto& [_, state] : mStates) {
                state.FindAllMatch(pids, *mProcParser, result, mIsContainerMode);
                pidMatchCacheSize += state.mPidMatchCache.size();
            }
        }

        if (mStatsCallback) {
            mStatsCallback(pidMatchCacheSize);
        }

        if (!result.empty()) {
            mCallback(std::move(result));
        }

        std::unique_lock<std::mutex> cvLock(mSleepCvMutex);
        mSleepCv.wait_for(cvLock, std::chrono::milliseconds(mSleepMilliseconds), [this]() { return !mRunning.load(); });
    }
}


void ProcessDiscoveryManager::InnerState::FindAllMatch(const std::vector<uint32_t>& pids,
                                                       ProcParser& procParser,
                                                       std::vector<DiscoverEntry>& results,
                                                       bool isContainerMode) {
    std::set<uint32_t> matchedPids;
    auto it = pids.begin();
    auto cacheIt = mPidMatchCache.begin();

    while (it != pids.end() && cacheIt != mPidMatchCache.end()) {
        if (*it == cacheIt->first) {
            // Cache hit, use the cached result
            if (cacheIt->second) {
                matchedPids.insert(*it);
            }
            ++it;
            ++cacheIt;
        } else if (*it < cacheIt->first) {
            // New process, fetch cmdline and containerId, then check
            std::string cmdline = procParser.GetPIDCmdline(*it);
            if (!cmdline.empty()) { // Empty means process exit or no permission
                // /proc/<pid>/cmdline use '\0' as separator, replace it with space
                std::replace(cmdline.begin(), cmdline.end(), '\0', ' ');

                std::string containerId;
                procParser.GetPIDDockerId(*it, containerId);

                bool isMatch = mConfig.IsMatch(cmdline, containerId, isContainerMode);
                mPidMatchCache[*it] = isMatch;
                if (isMatch) {
                    matchedPids.insert(*it);
                }
            }
            ++it;
        } else { // *it > cacheIt->first
            // Process disappeared, remove from cache
            cacheIt = mPidMatchCache.erase(cacheIt);
        }
    }

    while (it != pids.end()) {
        // New processes after the last cached one
        std::string cmdline = procParser.GetPIDCmdline(*it);
        if (cmdline.empty()) {
            // Process exit or no permission
            ++it;
            continue;
        }
        // /proc/<pid>/cmdline use '\0' as separator, replace it with space
        std::replace(cmdline.begin(), cmdline.end(), '\0', ' ');

        std::string containerId;
        procParser.GetPIDDockerId(*it, containerId);

        bool isMatch = mConfig.IsMatch(cmdline, containerId, isContainerMode);
        mPidMatchCache[*it] = isMatch;
        if (isMatch) {
            matchedPids.insert(*it);
        }
        ++it;
    }

    while (cacheIt != mPidMatchCache.end()) {
        // Processes disappeared after the last one in the current list
        cacheIt = mPidMatchCache.erase(cacheIt);
    }

    if (mPrevPids == matchedPids) {
        return;
    }
    results.emplace_back(mConfig.mConfigKey, matchedPids);
    mPrevPids = std::move(matchedPids);
}

} // namespace ebpf
} // namespace logtail

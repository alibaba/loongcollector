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
#include <optional>
#include <string>
#include <vector>

#include "app_config/AppConfig.h"
#include "common/LogtailCommonFlags.h"
#include "common/ProcParser.h"
#include "common/StringTools.h"
#include "container_manager/ContainerDiff.h"
#include "ebpf/plugin/cpu_profiling/ProcessEntry.h"
#include "logger/Logger.h"

namespace logtail {
namespace ebpf {

struct ProcessDiscoveryConfig {
    size_t mConfigKey;
    std::vector<boost::regex> mRegexs;
    std::unordered_set<std::string> mContainerIds;
    bool mFullDiscovery = false;

    bool IsMatch(const std::string& cmdline, const std::string& containerId, bool isContainerMode) {
        if (isContainerMode) {
            return mContainerIds.find(containerId) != mContainerIds.end() && checkCmdlines(cmdline);
        }
        return checkCmdlines(cmdline);
    }

private:
    bool checkCmdlines(const std::string& cmdline) {
        if (mFullDiscovery) {
            return true;
        }
        for (auto& regex : mRegexs) {
            std::string exception;
            if (BoostRegexMatch(cmdline.c_str(), cmdline.size(), regex, exception)) {
                return true;
            } else {
                if (!exception.empty()) {
                    LOG_ERROR(sLogger, ("regex_match in ProcessDiscoveryManager fail", exception));
                }
            }
        }
        return false;
    }
};

inline std::optional<std::string> GetContainerHostPath() {
    if (AppConfig::GetInstance()->IsPurageContainerMode()) {
        return STRING_FLAG(default_container_host_path);
    }
    return std::nullopt;
}

class ProcessDiscoveryManager {
public:
    using DiscoverEntry = std::pair<size_t, std::set<uint32_t>>;
    using DiscoverResult = std::vector<DiscoverEntry>;
    using NotifyFn = std::function<void(DiscoverResult)>;

    ProcessDiscoveryManager()
        : mIsContainerMode(AppConfig::GetInstance()->IsPurageContainerMode()),
          mProcParser(GetContainerHostPath().value_or("/")) {}

    ProcessDiscoveryManager(const ProcessDiscoveryManager&) = delete;
    ProcessDiscoveryManager& operator=(const ProcessDiscoveryManager&) = delete;
    ProcessDiscoveryManager(ProcessDiscoveryManager&&) = delete;
    ProcessDiscoveryManager& operator=(ProcessDiscoveryManager&&) = delete;

    ~ProcessDiscoveryManager() { Stop(); }

    static ProcessDiscoveryManager* GetInstance() {
        static ProcessDiscoveryManager sInstance;
        return &sInstance;
    }

    void Start(NotifyFn fn, size_t milliseconds = 15000);
    void Stop();

    void AddDiscovery(const std::string& configName, ProcessDiscoveryConfig config);

    bool UpdateDiscovery(const std::string& configName, const ContainerDiff& diff);

    void RemoveDiscovery(const std::string& configName);

    bool CheckDiscoveryExist(const std::string& configName);

private:
    void run();

    struct InnerState {
        ProcessDiscoveryConfig mConfig;
        std::set<uint32_t> mPrevPids;
        std::map<uint32_t, bool> mPidMatchCache;

        void FindAllMatch(const std::vector<ProcessEntry>& procsOrdered,
                          std::vector<DiscoverEntry>& results,
                          bool isContainerMode);
    };

    std::atomic_bool mRunning = false;
    std::future<void> mThreadRes;
    std::mutex mLock;
    std::unordered_map<std::string, InnerState> mStates;
    NotifyFn mCallback;

    bool mIsContainerMode;
    ProcParser mProcParser;
    size_t mSleepMilliseconds;
};

} // namespace ebpf
} // namespace logtail

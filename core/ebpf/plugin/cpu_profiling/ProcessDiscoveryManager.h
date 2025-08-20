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

#include "app_config/AppConfig.h"
#include "common/LogtailCommonFlags.h"
#include "common/ProcParser.h"
#include "boost/regex.hpp"

namespace logtail {
namespace ebpf {

struct ProcessDiscoveryConfig {
    size_t mConfigKey;
    std::vector<boost::regex> mRegexs;
    bool mFullDiscovery = false;

    bool IsMatch(const std::string& cmdline) {
        if (mFullDiscovery) {
            return true;
        }
        for (auto& regex : mRegexs) {
            if (boost::regex_match(cmdline, regex)) {
                return true;
            }
        }
        return false;
    }
};

class ProcessDiscoveryManager {
public:
    using DiscoverEntry = std::pair<size_t, std::set<uint32_t>>;
    using DiscoverResult = std::vector<DiscoverEntry>;
    using NotifyFn = std::function<void(DiscoverResult)>;
    using UpdateFn = std::function<void(ProcessDiscoveryConfig&)>;

    ProcessDiscoveryManager() : mProcParser(getProcParserPrefix()) {}

    ProcessDiscoveryManager(const ProcessDiscoveryManager &) = delete;
    ProcessDiscoveryManager &operator=(const ProcessDiscoveryManager &) = delete;
    ProcessDiscoveryManager(ProcessDiscoveryManager &&) = delete;
    ProcessDiscoveryManager &operator=(ProcessDiscoveryManager &&) = delete;

    ~ProcessDiscoveryManager() { Stop(); }

    static ProcessDiscoveryManager *GetInstance() {
        static ProcessDiscoveryManager instance;
        return &instance;
    }

    void Start(NotifyFn fn);
    void Stop();

    void AddOrUpdateDiscovery(const std::string &configName, UpdateFn updater);

    bool UpdateDiscovery(const std::string &configName, UpdateFn updater);

    void RemoveDiscovery(const std::string &configName);

    bool CheckDiscoveryExist(const std::string &configName);

private:
    static std::string getProcParserPrefix() {
        if (AppConfig::GetInstance()->IsPurageContainerMode()) {
            return STRING_FLAG(default_container_host_path);
        }
        return "/";
    }

    void run();

    struct InnerState {
        ProcessDiscoveryConfig mConfig;
        std::set<uint32_t> mPrevPids;
    };

    std::atomic_bool mRunning = false;
    std::future<void> mThreadRes;
    std::mutex mLock;
    std::unordered_map<std::string, InnerState> mStates;
    NotifyFn mCallback;
    
    ProcParser mProcParser;
};

} // namespace ebpf
} // namespace logtail

/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "../common/JournalCommon.h"
#include "../common/JournalConfig.h"
#include "../reader/JournalFilter.h"
#include "logger/Logger.h"

namespace logtail {

class JournalConnection {
public:
    static JournalConnection& GetInstance();
    bool Initialize();
    void Cleanup();
    bool AddConfig(const std::string& configName, const JournalConfig& config);
    void RemoveConfig(const std::string& configName);
    bool RefreshConnection(const std::string& configName);
    bool ShouldRefreshConnection(const std::string& configName) const;
    JournalConfig GetConfig(const std::string& configName) const;
    std::map<std::string, JournalConfig> GetAllConfigs() const;
    std::vector<std::string> GetAllConfigNames() const;
    std::shared_ptr<JournalReader> GetConnection(const std::string& configName) const;
    struct Stats {
        size_t totalConfigs;
        size_t activeConnections;
        size_t invalidConnections;
        std::vector<std::string> connectionKeys;
        size_t totalConnections;
    };
    Stats GetStats() const;
    size_t GetConnectionCount() const;

private:
    JournalConnection() = default;
    ~JournalConnection() = default;

    JournalConnection(const JournalConnection&) = delete;
    JournalConnection& operator=(const JournalConnection&) = delete;
    JournalConnection(JournalConnection&&) = delete;
    JournalConnection& operator=(JournalConnection&&) = delete;

    bool initializeReader(const std::shared_ptr<JournalReader>& reader,
                          const JournalConfig& config,
                          const std::string& configName);
    bool setupReaderPosition(const std::shared_ptr<JournalReader>& reader,
                             const JournalConfig& config,
                             const std::string& configName,
                             const std::string& savedCursor = "");
    static JournalFilter::FilterConfig buildFilterConfig(const JournalConfig& config, const std::string& configName);
    struct ConfigInfo {
        std::string mConfigName;
        JournalConfig config;
        std::shared_ptr<JournalReader> reader; // Independent reader/connection per configuration
        std::chrono::steady_clock::time_point lastOpenTime; // Last connection open time, used for scheduled refresh
    };

    std::map<std::string, ConfigInfo> mConfigs; // key: configName
    mutable std::mutex mMutex;
    bool mInitialized{false};
};

} // namespace logtail

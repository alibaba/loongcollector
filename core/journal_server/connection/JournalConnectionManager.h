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

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "../common/JournalConfig.h"

namespace logtail {

// Forward declarations
struct JournalEntry;
class JournalReader;

/**
 * @brief Journal连接管理器
 *
 * 负责管理journal配置和连接的生命周期：
 * 1. 每个配置拥有独立的journal reader/连接
 * 2. 管理配置到连接的映射关系
 * 3. 提供连接池统计和管理接口
 */
class JournalConnectionManager {
public:
    static JournalConnectionManager& GetInstance();

    bool Initialize();

    void Cleanup();

    bool AddConfig(const std::string& configName, const JournalConfig& config);

    void RemoveConfig(const std::string& configName);

    struct Stats {
        size_t totalConfigs;
        size_t activeConnections;
        size_t invalidConnections;
        std::vector<std::string> connectionKeys;
        size_t totalConnections;
    };
    Stats GetStats() const;

    std::shared_ptr<JournalReader> GetConnection(const std::string& configName) const;

    JournalConfig GetConfig(const std::string& configName) const;

    std::map<std::string, JournalConfig> GetAllConfigs() const;

    std::vector<std::string> GetConfigsUsingConnection(const std::shared_ptr<JournalReader>& reader) const;

    size_t GetConnectionCount() const;

private:
    JournalConnectionManager() = default;
    ~JournalConnectionManager() = default;

    JournalConnectionManager(const JournalConnectionManager&) = delete;
    JournalConnectionManager& operator=(const JournalConnectionManager&) = delete;
    JournalConnectionManager(JournalConnectionManager&&) = delete;
    JournalConnectionManager& operator=(JournalConnectionManager&&) = delete;

    struct ConfigInfo {
        std::string mConfigName;
        JournalConfig config;
        std::shared_ptr<JournalReader> reader; // 每个配置独立的reader/连接
    };

    std::map<std::string, ConfigInfo> mConfigs; // key: configName
    mutable std::mutex mMutex;
    bool mInitialized{false};
};

} // namespace logtail

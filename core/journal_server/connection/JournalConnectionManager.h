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

    // 初始化管理器
    bool Initialize();

    // 清理资源
    void Cleanup();

    // 添加配置（创建独立的连接）
    bool AddConfig(const std::string& configName, const JournalConfig& config);

    // 移除配置（关闭并删除连接）
    void RemoveConfig(const std::string& configName);

    // 获取统计信息
    struct Stats {
        size_t totalConfigs;
        size_t activeConnections;
        size_t invalidConnections;
        std::vector<std::string> connectionKeys;
        size_t totalConnections; // 总连接数（与totalConfigs相同）
    };
    Stats GetStats() const;

    // 获取指定配置的连接（reader）
    std::shared_ptr<JournalReader> GetConnection(const std::string& configName) const;

    // 获取指定配置
    JournalConfig GetConfig(const std::string& configName) const;

    // 获取所有配置（用于遍历）
    std::map<std::string, JournalConfig> GetAllConfigs() const;

    // 获取使用指定连接的配置名（每个连接只对应一个配置）
    std::vector<std::string> GetConfigsUsingConnection(const std::shared_ptr<JournalReader>& reader) const;

    // 获取当前连接数量
    size_t GetConnectionCount() const;

private:
    JournalConnectionManager() = default;
    ~JournalConnectionManager() = default;

    // 禁用拷贝和移动
    JournalConnectionManager(const JournalConnectionManager&) = delete;
    JournalConnectionManager& operator=(const JournalConnectionManager&) = delete;
    JournalConnectionManager(JournalConnectionManager&&) = delete;
    JournalConnectionManager& operator=(JournalConnectionManager&&) = delete;

    // 配置信息结构
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

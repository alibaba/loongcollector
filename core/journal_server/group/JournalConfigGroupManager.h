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

#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <string>

#include "../common/JournalConfig.h"

namespace logtail {

// Forward declarations
struct JournalEntry;
class SystemdJournalReader;

/**
 * @brief Journal配置分组管理器
 * 
 * 设计目标：
 * 1. 根据filter hash对配置进行分组
 * 2. 相同filter的配置共享inotify实例
 * 3. 减少系统资源使用，提高性能
 */
class JournalConfigGroupManager {
public:
    using ConfigHandler = std::function<void(const std::string& configName, size_t idx, const JournalEntry& entry)>;
    
    static JournalConfigGroupManager& GetInstance();
    
    // 初始化分组管理器
    bool Initialize();
    
    // 清理资源
    void Cleanup();
    
    // 添加配置到分组
    bool AddConfig(const std::string& configName, size_t idx, const JournalConfig& config, const ConfigHandler& handler);
    
    // 移除配置
    void RemoveConfig(const std::string& configName, size_t idx);
    
    // 获取统计信息
    struct Stats {
        int totalGroups;
        int totalConfigs;
        int sharedInotifyInstances;
        std::vector<std::string> groupKeys;
        // 增强的连接池统计信息
        size_t totalConnections;
        size_t activeConnections;
        size_t invalidConnections;
        std::vector<std::string> connectionKeys;
    };
    Stats GetStats() const;
    
    // 获取指定配置的连接信息
    std::shared_ptr<SystemdJournalReader> GetConnectionInfo(const std::string& configName, size_t idx) const;
    
    // 获取使用指定reader的所有配置
    std::vector<std::pair<std::string, size_t>> GetConfigsUsingReader(const std::shared_ptr<SystemdJournalReader>& reader) const;
    
    // 强制重置指定连接（手动重置接口）
    bool ForceResetConnection(const std::string& configName, size_t idx);
    
    // 获取当前连接数量
    size_t GetConnectionCount() const;

private:
    JournalConfigGroupManager() = default;
    ~JournalConfigGroupManager() = default;
    
    // 禁用拷贝和移动
    JournalConfigGroupManager(const JournalConfigGroupManager&) = delete;
    JournalConfigGroupManager& operator=(const JournalConfigGroupManager&) = delete;
    JournalConfigGroupManager(JournalConfigGroupManager&&) = delete;
    JournalConfigGroupManager& operator=(JournalConfigGroupManager&&) = delete;
    
    // 计算配置的filter hash
    std::string calculateFilterHash(const JournalConfig& config) const;
    
    // 配置信息
    struct ConfigInfo {
        std::string configName;
        size_t idx;
        JournalConfig config;
        ConfigHandler handler;
        std::string lastCursor;  // 用于检查点管理
    };
    
    // 分组信息
    struct GroupInfo {
        std::string filterHash;
        std::shared_ptr<SystemdJournalReader> sharedReader;
        std::map<std::string, ConfigInfo> configs;  // key: "configName:idx"
        int epollFD = -1;  // 共享的epoll FD
    };
    
    std::map<std::string, std::shared_ptr<GroupInfo>> mGroups;  // key: filterHash
    mutable std::mutex mMutex;
    bool mInitialized{false};
};

} // namespace logtail

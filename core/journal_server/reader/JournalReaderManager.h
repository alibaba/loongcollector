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
 * @brief Journal Reader管理器
 * 
 * 设计目标：
 * 1. 每个配置拥有独立的journal reader
 * 2. 每个reader有独立的fd，不共享
 * 3. 简化管理逻辑，提高隔离性
 */
class JournalReaderManager {
public:
    using ConfigHandler = std::function<void(const std::string& configName, size_t idx, const JournalEntry& entry)>;
    
    static JournalReaderManager& GetInstance();
    
    // 初始化管理器
    bool Initialize();
    
    // 清理资源
    void Cleanup();
    
    // 添加配置（创建独立的reader）
    bool AddConfig(const std::string& configName, size_t idx, const JournalConfig& config, const ConfigHandler& handler);
    
    // 移除配置（关闭并删除reader）
    void RemoveConfig(const std::string& configName, size_t idx);
    
    // 获取统计信息
    struct Stats {
        size_t totalConfigs;
        size_t activeConnections;
        size_t invalidConnections;
        std::vector<std::string> connectionKeys;
        
        // 保持兼容性的字段（已废弃）
        int totalGroups;
        int sharedInotifyInstances;
        std::vector<std::string> groupKeys;
        size_t totalConnections;
    };
    Stats GetStats() const;
    
    // 获取指定配置的reader
    std::shared_ptr<SystemdJournalReader> GetConnectionInfo(const std::string& configName, size_t idx) const;
    
    // 获取使用指定reader的配置（现在每个reader只对应一个配置）
    std::vector<std::pair<std::string, size_t>> GetConfigsUsingReader(const std::shared_ptr<SystemdJournalReader>& reader) const;
    
    // 强制重置指定连接
    bool ForceResetConnection(const std::string& configName, size_t idx);
    
    // 获取当前连接数量
    size_t GetConnectionCount() const;

private:
    JournalReaderManager() = default;
    ~JournalReaderManager() = default;
    
    // 禁用拷贝和移动
    JournalReaderManager(const JournalReaderManager&) = delete;
    JournalReaderManager& operator=(const JournalReaderManager&) = delete;
    JournalReaderManager(JournalReaderManager&&) = delete;
    JournalReaderManager& operator=(JournalReaderManager&&) = delete;
    
    // 生成配置的唯一key
    std::string makeConfigKey(const std::string& configName, size_t idx) const;
    
    // 配置信息结构
    struct ConfigInfo {
        std::string configName;
        size_t idx;
        JournalConfig config;
        ConfigHandler handler;
        std::shared_ptr<SystemdJournalReader> reader;  // 每个配置独立的reader
    };
    
    std::map<std::string, ConfigInfo> mConfigs;  // key: "configName:idx"
    mutable std::mutex mMutex;
    bool mInitialized{false};
};

} // namespace logtail

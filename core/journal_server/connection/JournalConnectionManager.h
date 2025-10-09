/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "../common/JournalConfig.h"
#include "JournalConnectionInstance.h"

namespace logtail {


/**
 * @brief Journal连接管理器
 * 
 * 负责管理所有config的journal连接，提供：
 * - 连接的创建和复用
 * - 定期重置连接以释放内存
 * - 连接的清理和资源管理
 */
class JournalConnectionManager {
public:
    static JournalConnectionManager* GetInstance() {
        static JournalConnectionManager sInstance;
        return &sInstance;
    }

    JournalConnectionManager(const JournalConnectionManager&) = delete;
    JournalConnectionManager& operator=(const JournalConnectionManager&) = delete;
    JournalConnectionManager(JournalConnectionManager&&) = delete;
    JournalConnectionManager& operator=(JournalConnectionManager&&) = delete;

    /**
     * @brief 获取或创建journal连接
     * @param configName 配置名称
     * @param idx 配置索引
     * @param config journal配置
     * @return journal reader智能指针，失败返回nullptr
     */
    std::shared_ptr<SystemdJournalReader> GetOrCreateConnection(
        const std::string& configName,
        size_t idx,
        const JournalConfig& config);

    /**
     * @brief 移除指定配置的连接
     * @param configName 配置名称
     * @param idx 配置索引
     */
    void RemoveConnection(const std::string& configName, size_t idx);

    /**
     * @brief 清理无效连接（移除自动重置逻辑，连接永远不重建）
     * @return 清理的连接数量
     */
    size_t CleanupExpiredConnections();
    
    /**
     * @brief 清理无效连接（移除自动重置逻辑，连接永远不重建）
     * 
     * 只清理无效的连接，有效连接永远不重建。
     * 
     * @return 清理的连接数量
     */
    size_t ResetExpiredConnections();

    /**
     * @brief 获取连接池统计信息
     * @return 连接池统计信息
     */
    struct ConnectionPoolStats {
        size_t totalConnections;
        size_t activeConnections;
        size_t invalidConnections;
        std::vector<std::string> connectionKeys;
    };
    ConnectionPoolStats GetConnectionPoolStats() const;

    /**
     * @brief 获取指定配置的连接信息
     * @param configName 配置名称
     * @param idx 配置索引
     * @return 连接信息，如果不存在返回nullptr
     */
    std::shared_ptr<JournalConnectionInstance> GetConnectionInfo(const std::string& configName, size_t idx) const;

    /**
     * @brief 强制重置指定连接（手动重置接口）
     * @param configName 配置名称
     * @param idx 配置索引
     * @return true 如果重置成功
     */
    bool ForceResetConnection(const std::string& configName, size_t idx);

    /**
     * @brief 获取当前连接数量
     */
    size_t GetConnectionCount() const;

    /**
     * @brief 清理所有连接（用于测试和关闭）
     */
    void Clear();

private:
    JournalConnectionManager() = default;
    ~JournalConnectionManager() = default;

    using ConnectionKey = std::pair<std::string, size_t>;
    using ConnectionPtr = std::shared_ptr<JournalConnectionInstance>;
    
    mutable std::mutex mConnectionsMutex;
    std::unordered_map<std::string, ConnectionPtr> mConnections; // key: configName_idx
    
    // 生成连接的唯一key
    std::string makeConnectionKey(const std::string& configName, size_t idx) const;
};

} // namespace logtail 
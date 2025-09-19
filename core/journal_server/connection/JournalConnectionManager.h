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

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <atomic>

#include "../common/JournalConfig.h"
#include "JournalConnectionInstance.h"
#include "JournalConnectionGuard.h"

namespace logtail {

// Forward declarations
class JournalConnectionGuard;


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
     * @brief 获取带守护的journal连接（推荐使用）
     * @param configName 配置名称
     * @param idx 配置索引
     * @param config journal配置
     * @return 连接守护对象，自动管理使用计数
     */
    std::unique_ptr<JournalConnectionGuard> GetGuardedConnection(
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
     * @brief 清理过期连接
     * @param resetIntervalSec 重置间隔秒数，默认3600秒（1小时）
     * @return 清理的连接数量
     */
    size_t CleanupExpiredConnections(int resetIntervalSec = 3600);

    /**
     * @brief 智能清理策略：根据重置周期自动调整清理频率
     * @param resetIntervalSec 重置间隔秒数
     * @return 建议的清理检查间隔秒数
     */
    static int GetRecommendedCleanupInterval(int resetIntervalSec) {
        // 清理频率 = 重置周期 / 4，但不少于5分钟，不多于30分钟
        int interval = resetIntervalSec / 4;
        return std::max(300, std::min(interval, 1800));  // [5分钟, 30分钟]
    }

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
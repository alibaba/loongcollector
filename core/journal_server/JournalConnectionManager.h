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
#include <vector>
#include <atomic>

#include "journal_server/JournalReader.h"
#include "journal_server/JournalServer.h"
#include "journal_server/JournalFilter.h"

namespace logtail {

// Forward declarations
class JournalConnectionInfo;
class JournalConnectionGuard;

/**
 * @brief 管理单个config的journal连接信息
 * 
 * 参考Go版本的ServiceJournal设计，每个连接包含：
 * - 独立的journal reader实例
 * - 创建时间（用于重置周期判断）
 * - 配置信息的缓存
 * - checkpoint状态管理
 */
class JournalConnectionInfo {
public:
    JournalConnectionInfo(const std::string& configName, 
                         size_t idx, 
                         const JournalConfig& config);
    ~JournalConnectionInfo();

    // 获取journal reader，如果连接断开会尝试重连
    std::shared_ptr<SystemdJournalReader> GetReader();
    
    // 检查连接是否需要重置（基于时间）
    bool ShouldReset(int resetIntervalSec) const;
    
    // 强制重置连接
    bool ResetConnection();
    
    // 检查连接是否有效
    bool IsValid() const;
    
    // Checkpoint management - 集成checkpoint管理
    void SaveCheckpoint(const std::string& cursor);
    std::string GetCheckpoint() const;
    void ClearCheckpoint();
    bool HasCheckpoint() const;
    
    // 连接使用状态管理 - 防止在使用中的连接被重置
    void IncrementUsageCount();
    void DecrementUsageCount();
    bool IsInUse() const;
    
    // 获取连接信息
    const std::string& GetConfigName() const { return mConfigName; }
    size_t GetIndex() const { return mIndex; }
    std::chrono::steady_clock::time_point GetCreateTime() const { return mCreateTime; }

private:
    bool initializeConnection();
    void loadCheckpointFromDisk();
    void saveCheckpointToDisk();
    
    std::string mConfigName;
    size_t mIndex;
    JournalConfig mConfig;
    
    std::shared_ptr<SystemdJournalReader> mReader;
    std::chrono::steady_clock::time_point mCreateTime;
    std::chrono::steady_clock::time_point mLastResetTime;
    
    // Checkpoint状态
    std::string mCurrentCheckpoint;
    bool mCheckpointChanged;
    std::chrono::steady_clock::time_point mLastCheckpointSaveTime;
    
    // 连接使用计数 - 防止重置正在使用的连接
    std::atomic<int> mUsageCount{0};
    
    mutable std::mutex mMutex;
    bool mIsValid;
};

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
        static JournalConnectionManager instance;
        return &instance;
    }

    JournalConnectionManager(const JournalConnectionManager&) = delete;
    JournalConnectionManager& operator=(const JournalConnectionManager&) = delete;

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

    /**
     * @brief Checkpoint管理接口 - 统一管理所有config的checkpoint
     */
    
    /**
     * @brief 保存checkpoint
     * @param configName 配置名称
     * @param idx 配置索引  
     * @param cursor journal cursor位置
     */
    void SaveCheckpoint(const std::string& configName, size_t idx, const std::string& cursor);
    
    /**
     * @brief 获取checkpoint
     * @param configName 配置名称
     * @param idx 配置索引
     * @return checkpoint cursor，如果不存在返回空字符串
     */
    std::string GetCheckpoint(const std::string& configName, size_t idx) const;
    
    /**
     * @brief 清除checkpoint
     * @param configName 配置名称
     * @param idx 配置索引
     */
    void ClearCheckpoint(const std::string& configName, size_t idx);
    
    /**
     * @brief 定期保存所有变更的checkpoint到磁盘
     * @param forceAll 是否强制保存所有checkpoint
     * @return 保存的checkpoint数量
     */
    size_t FlushAllCheckpoints(bool forceAll = false);

private:
    JournalConnectionManager() = default;
    ~JournalConnectionManager() = default;

    using ConnectionKey = std::pair<std::string, size_t>;
    using ConnectionPtr = std::shared_ptr<JournalConnectionInfo>;
    
    mutable std::mutex mConnectionsMutex;
    std::unordered_map<std::string, ConnectionPtr> mConnections; // key: configName_idx
    
    // 生成连接的唯一key
    std::string makeConnectionKey(const std::string& configName, size_t idx) const;
};

/**
 * @brief RAII连接守护类，自动管理连接使用计数
 * 
 * 确保在连接使用期间不会被意外重置或清理
 */
class JournalConnectionGuard {
public:
    JournalConnectionGuard(std::shared_ptr<JournalConnectionInfo> connection)
        : mConnection(connection) {
        if (mConnection) {
            mConnection->IncrementUsageCount();
        }
    }
    
    ~JournalConnectionGuard() {
        if (mConnection) {
            mConnection->DecrementUsageCount();
        }
    }
    
    // 禁用拷贝和移动
    JournalConnectionGuard(const JournalConnectionGuard&) = delete;
    JournalConnectionGuard& operator=(const JournalConnectionGuard&) = delete;
    JournalConnectionGuard(JournalConnectionGuard&&) = delete;
    JournalConnectionGuard& operator=(JournalConnectionGuard&&) = delete;
    
    std::shared_ptr<SystemdJournalReader> GetReader() {
        return mConnection ? mConnection->GetReader() : nullptr;
    }
    
    bool IsValid() const {
        return mConnection && mConnection->IsValid();
    }
    
private:
    std::shared_ptr<JournalConnectionInfo> mConnection;
};

} // namespace logtail 
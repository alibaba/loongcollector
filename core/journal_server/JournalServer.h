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

#include <atomic>
#include <future>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>

#include "runner/InputRunner.h"
#include "common/JournalConfig.h"

namespace logtail {

// Forward declarations
class JournalConnectionManager;
class JournalConnectionInstance;
class SystemdJournalReader;
class PipelineEventGroup;
struct JournalEntry;

// MonitoredReader struct definition
struct MonitoredReader {
    std::shared_ptr<SystemdJournalReader> reader;
    std::string configName;
    size_t idx;
};

/**
 * @brief JournalServer manages all journal input plugins
 * 
 * It follows the same pattern as FileServer and StaticFileServer:
 * 1. Manages journal configurations from registered plugins
 * 2. Runs in a separate thread to process journal entries
 * 3. Sends data to processing queues
 * 4. Provides registration/unregistration interface for plugins
 */
class JournalServer : public InputRunner {
public:
    JournalServer(const JournalServer&) = delete;
    JournalServer& operator=(const JournalServer&) = delete;
    JournalServer(JournalServer&&) = delete;
    JournalServer& operator=(JournalServer&&) = delete;

    static JournalServer* GetInstance() {
        static JournalServer sInstance;
        return &sInstance;
    }

    ~JournalServer() = default;

    // =============================================================================
    // 生命周期管理 - Lifecycle Management
    // =============================================================================
    
    // InputRunner interface implementation
    void Init() override;
    void Stop() override;
    bool HasRegisteredPlugins() const override;
    
    // =============================================================================
    // 配置管理 - Configuration Management
    // =============================================================================
    
    // Plugin registration interface
    void AddJournalInput(const std::string& configName,
                        size_t idx,
                        const JournalConfig& config);
    void RemoveJournalInput(const std::string& configName, size_t idx);
    void RemoveJournalInputWithoutCleanup(const std::string& configName, size_t idx);

    // Update configuration needsSeek status
    void UpdateJournalConfigNeedsSeek(const std::string& configName, size_t idx, bool needsSeek);
    
    /**
     * @brief 获取所有配置
     * @return 所有配置的映射
     */
    std::map<std::pair<std::string, size_t>, JournalConfig> GetAllJournalConfigs() const;

    // =============================================================================
    // 连接池管理 - Connection Pool Management
    // =============================================================================
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
    std::shared_ptr<SystemdJournalReader> GetConnectionInfo(const std::string& configName, size_t idx) const;

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
    
    // =============================================================================
    // Epoll 管理 - Epoll Management
    // =============================================================================
    
    /**
     * @brief 获取全局 epoll FD
     * @return 全局 epoll FD，如果未初始化返回 -1
     */
    int GetGlobalEpollFD() const;
    
    /**
     * @brief 清理指定配置的 epoll 监控
     * @param configName 配置名称
     * @param idx 配置索引
     */
    void CleanupEpollMonitoring(const std::string& configName, size_t idx);
    

#ifdef APSARA_UNIT_TEST_MAIN
    void Clear();
#endif

private:
    JournalServer() = default;

    // =============================================================================
    // 事件驱动主循环 - Event-driven Main Loop
    // =============================================================================
    void run();
    
    // =============================================================================
    // 事件驱动辅助方法 - Event-driven Helper Methods
    // =============================================================================
    void updateReaderMonitoring(int epollFD, std::map<int, MonitoredReader>& monitoredReaders);
    void processSpecificJournalConfig(const std::string& configName, size_t idx);
    bool validateAndGetQueueKey(const std::string& configName, size_t idx, const JournalConfig& config, QueueKey& queueKey);

    // =============================================================================
    // 成员变量 - Member Variables
    // =============================================================================
    
    // 线程管理 - Thread Management
    std::future<void> mThreadRes;
    std::atomic<bool> mIsThreadRunning{true};
    
    // 全局 epoll FD 管理 - Global Epoll FD Management
    int mGlobalEpollFD{-1};
    mutable std::mutex mEpollMutex;

    // 初始化状态管理 - Initialization State Management
    bool mIsInitialized = false;
    mutable std::mutex mInitMux;

    // 配置存储已移至 JournalConnectionManager
    mutable std::mutex mUpdateMux;
};

} // namespace logtail 
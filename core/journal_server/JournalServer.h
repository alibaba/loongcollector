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

#include "common/JournalConfig.h"
#include "runner/InputRunner.h"

namespace logtail {

// Forward declarations
class JournalConnectionManager;
class JournalConnectionInstance;
class JournalReader;
class PipelineEventGroup;
struct JournalEntry;

// Epoll timeout constants
inline constexpr int kJournalEpollTimeoutMS = 200;

// MonitoredReader struct definition
struct MonitoredReader {
    std::shared_ptr<JournalReader> reader;
    std::string configName;
    bool hasPendingData{true}; // 上次读取是否还有数据，用于优化跳过 NOP 事件时的无效读取
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

    // InputRunner interface implementation
    void Init() override;
    void Stop() override;
    bool HasRegisteredPlugins() const override;

    // Plugin registration interface
    void AddJournalInput(const std::string& configName, const JournalConfig& config);
    void RemoveJournalInput(const std::string& configName);
    void RemoveConfigOnly(const std::string& configName);

    /**
     * @brief 获取所有配置
     * @return 所有配置的映射
     */
    std::map<std::string, JournalConfig> GetAllJournalConfigs() const;

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
     * @return 连接信息，如果不存在返回nullptr
     */
    std::shared_ptr<JournalReader> GetConnectionInfo(const std::string& configName) const;

    /**
     * @brief 获取当前连接数量
     */

    size_t GetConnectionCount() const;
    /**
     * @brief 获取全局 epoll FD
     * @return 全局 epoll FD，如果未初始化返回 -1
     */
    int GetGlobalEpollFD() const;


#ifdef APSARA_UNIT_TEST_MAIN
    void Clear();
#endif

private:
    JournalServer() = default;

    void run();
    bool handlePendingDataReaders(std::map<int, MonitoredReader>& monitoredReaders);
    void syncMonitors(int epollFD, std::map<int, MonitoredReader>& monitoredReaders);
    
    // 同步监控的辅助方法
    void cleanupStaleReaders(int epollFD, std::map<int, MonitoredReader>& monitoredReaders);
    void refreshConnections(int epollFD, std::map<int, MonitoredReader>& monitoredReaders);
    void addNewReaders(int epollFD, std::map<int, MonitoredReader>& monitoredReaders);
    void syncReaderAfterRefresh(const std::string& configName, std::map<int, MonitoredReader>& monitoredReaders);
    
    bool validateAndGetCurrentReader(const MonitoredReader& monitoredReader,
                                      std::shared_ptr<JournalReader>& currentReaderOut) const;
    
    bool validateQueueKey(const std::string& configName, const JournalConfig& config, QueueKey& queueKey);


    std::future<void> mThreadRes;
    std::atomic<bool> mIsThreadRunning{true};

    // 全局 epoll FD 管理 - Global Epoll FD Management
    int mGlobalEpollFD{-1};
    mutable std::mutex mEpollMutex;

    std::atomic<bool> mIsInitialized{false};
};

} // namespace logtail

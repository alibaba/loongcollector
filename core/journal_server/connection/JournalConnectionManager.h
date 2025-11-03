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

#include "../common/JournalConfig.h"
#include "../reader/JournalFilter.h"
#include "logger/Logger.h"

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

    /**
     * @brief 移除配置并清理epoll监控
     * @param configName 配置名称
     * @param epollFD epoll文件描述符，如果 >= 0 则清理epoll监控
     */
    void RemoveConfigWithEpollCleanup(const std::string& configName, int epollFD);

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

    /**
     * @brief 刷新指定配置的journal连接（关闭并重新打开）
     * @param configName 配置名称
     * @param epollFD epoll文件描述符，用于从epoll移除和添加
     * @return 刷新是否成功
     */
    bool RefreshConnection(const std::string& configName, int epollFD);

    /**
     * @brief 检查指定配置的连接是否需要刷新（基于时间间隔）
     * @param configName 配置名称
     * @return 是否需要刷新
     */
    bool ShouldRefreshConnection(const std::string& configName) const;

    /**
     * @brief 获取所有有效配置的名称集合
     * @return 有效配置名称集合
     */
    std::set<std::string> GetValidConfigNames() const;

    /**
     * @brief 获取所有应该被监控的连接（已打开且有效）
     * @return 配置名称到连接的映射（只包含已打开的连接）
     */
    std::map<std::string, std::shared_ptr<JournalReader>> GetOpenConnections() const;

    /**
     * @brief 清理无效的监控reader（从epoll监控中移除已删除的配置对应的reader）
     * @param epollFD epoll文件描述符
     * @param monitoredReaders 监控reader的map（key: fd, value: MonitoredReader结构）
     * @return 清理的reader数量
     * 
     * @note MonitoredReader 结构需要有以下字段：
     *       - std::shared_ptr<JournalReader> reader
     *       - std::string configName
     */
    template<typename MonitoredReader>
    int CleanupInvalidMonitoredReaders(int epollFD, std::map<int, MonitoredReader>& monitoredReaders) const {
        std::set<std::string> validConfigNames = GetValidConfigNames();
        int cleanedCount = 0;

        for (auto it = monitoredReaders.begin(); it != monitoredReaders.end();) {
            auto& monitoredReader = it->second;

            // 检查配置是否仍然存在
            if (validConfigNames.find(monitoredReader.configName) == validConfigNames.end()) {
                LOG_DEBUG(sLogger,
                          ("journal connection manager removing reader from monitoring",
                           "config no longer exists")("config", monitoredReader.configName)("fd", it->first));

                // 从epoll移除
                if (monitoredReader.reader && monitoredReader.reader->IsOpen()) {
                    monitoredReader.reader->RemoveFromEpoll(epollFD);
                }

                it = monitoredReaders.erase(it);
                cleanedCount++;
            } else {
                ++it;
            }
        }

        return cleanedCount;
    }

private:
    JournalConnectionManager() = default;
    ~JournalConnectionManager() = default;

    JournalConnectionManager(const JournalConnectionManager&) = delete;
    JournalConnectionManager& operator=(const JournalConnectionManager&) = delete;
    JournalConnectionManager(JournalConnectionManager&&) = delete;
    JournalConnectionManager& operator=(JournalConnectionManager&&) = delete;

    // 辅助方法：初始化reader（设置路径、打开、应用过滤器）
    bool InitializeReader(const std::shared_ptr<JournalReader>& reader,
                          const JournalConfig& config,
                          const std::string& configName);

    // 辅助方法：设置reader的读取位置（支持cursor恢复）
    bool SetupReaderPosition(const std::shared_ptr<JournalReader>& reader,
                             const JournalConfig& config,
                             const std::string& configName,
                             const std::string& savedCursor = "");

    // 辅助方法：构建过滤器配置
    static JournalFilter::FilterConfig BuildFilterConfig(const JournalConfig& config, const std::string& configName);

    struct ConfigInfo {
        std::string mConfigName;
        JournalConfig config;
        std::shared_ptr<JournalReader> reader; // 每个配置独立的reader/连接
        std::chrono::steady_clock::time_point lastOpenTime; // 上次打开连接的时间，用于定时刷新
    };

    std::map<std::string, ConfigInfo> mConfigs; // key: configName
    mutable std::mutex mMutex;
    bool mInitialized{false};
};

} // namespace logtail

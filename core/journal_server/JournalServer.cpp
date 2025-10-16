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

#include "JournalServer.h"

#include <chrono>
#include <utility>

#ifdef __linux__
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#endif

#include "group/JournalConfigGroupManager.h"
#include "group/JournalConnectionSetup.h"
#include "reader/JournalReader.h"
#include "JournalEntryProcessor.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "logger/Logger.h"


using namespace std;

namespace logtail {

// =============================================================================
// 1. 生命周期管理 - Lifecycle Management
// =============================================================================

void JournalServer::Init() {
    lock_guard<mutex> lock(mInitMux);
    if (mIsInitialized) {
        LOG_INFO(sLogger, ("JournalServer already initialized", "skipping duplicate Init() call"));
        return;
    }
    
    mThreadRes = async(launch::async, &JournalServer::run, this);
    
    // 初始化配置分组管理器
    JournalConfigGroupManager::GetInstance().Initialize();
    
    // 启用配置分组优化
    EnableConfigGrouping();
    
    mIsInitialized = true;
    LOG_INFO(sLogger, ("JournalServer initialized", ""));
}

void JournalServer::Stop() {
    lock_guard<mutex> initLock(mInitMux);
    if (!mIsInitialized || !mThreadRes.valid()) {
        return;
    }
    mIsThreadRunning.store(false);
    if (mThreadRes.valid()) {
        mThreadRes.get();
    }
    
    // 清理配置分组管理器
    JournalConfigGroupManager::GetInstance().Cleanup();
    
    mIsInitialized = false;
    LOG_INFO(sLogger, ("JournalServer stopped", ""));
}

bool JournalServer::HasRegisteredPlugins() const {
    lock_guard<mutex> lock(mUpdateMux);
    return !mPipelineNameJournalConfigsMap.empty();
}
// =============================================================================
// 2. 配置管理 - Configuration Management
// =============================================================================

void JournalServer::AddJournalInput(const string& configName, size_t idx, const JournalConfig& config) {
    // 首先验证配置
    QueueKey queueKey = 0;
    if (!validateJournalConfig(configName, idx, config, queueKey)) {
        LOG_ERROR(sLogger, ("journal input validation failed", "config not added")("config", configName)("idx", idx));
        return;
    }
    
    // 验证成功后，缓存queueKey并添加配置
    JournalConfig validatedConfig = config;
    validatedConfig.queueKey = queueKey;
    
    {
        lock_guard<mutex> lock(mUpdateMux);
        mPipelineNameJournalConfigsMap[configName][idx] = validatedConfig;
        
        LOG_INFO(sLogger, ("journal input added after validation", "")("config", configName)("idx", idx)("ctx_valid", config.ctx != nullptr)("queue_key", queueKey)("total_pipelines", mPipelineNameJournalConfigsMap.size()));
    }
    
    // 检查是否启用配置分组优化
    {
        std::lock_guard<std::mutex> groupingLock(mGroupingMutex);
        if (mConfigGroupingEnabled) {
            // 使用配置分组优化
            auto groupManager = &JournalConfigGroupManager::GetInstance();
            
            // 创建配置处理器
            auto handler = [this, configName, idx](const std::string&, size_t, const JournalEntry&) {
                // 处理 journal 条目
                processSpecificJournalConfig(configName, idx);
            };
            
            if (groupManager->AddConfig(configName, idx, validatedConfig, handler)) {
                LOG_INFO(sLogger, ("config added to grouping manager", "")("config", configName)("idx", idx));
                
                // 记录分组统计信息
                auto stats = groupManager->GetStats();
                LOG_INFO(sLogger, ("grouping stats", "")("total_groups", stats.totalGroups)("total_configs", stats.totalConfigs)("shared_inotify", stats.sharedInotifyInstances));
                
                return;
            } else {
                LOG_ERROR(sLogger, ("failed to add config to grouping manager", "")("config", configName)("idx", idx));
                return;
            }
        } else {
            LOG_ERROR(sLogger, ("configuration grouping optimization is disabled", "")("config", configName)("idx", idx));
            return;
        }
    }
}

void JournalServer::RemoveJournalInput(const string& configName, size_t idx) {
    {
        lock_guard<mutex> lock(mUpdateMux);
        auto configItr = mPipelineNameJournalConfigsMap.find(configName);
        // 如果配置存在，则移除
        if (configItr != mPipelineNameJournalConfigsMap.end()) {
            configItr->second.erase(idx);
            // 如果配置为空，则移除整个配置
            if (configItr->second.empty()) {
                // 移除整个配置
                mPipelineNameJournalConfigsMap.erase(configItr);
            }
        }
    }
    
    // 清理 epoll 监控
    CleanupEpollMonitoring(configName, idx);
    
    // 移除config对应的连接
    JournalConfigGroupManager::GetInstance().RemoveConfig(configName, idx);
    
    LOG_INFO(sLogger, ("journal input removed with automatic connection cleanup", "")("config", configName)("idx", idx));
}

void JournalServer::RemoveJournalInputWithoutCleanup(const string& configName, size_t idx) {
    {
        lock_guard<mutex> lock(mUpdateMux);
        auto configItr = mPipelineNameJournalConfigsMap.find(configName);
        // 如果配置存在，则移除
        if (configItr != mPipelineNameJournalConfigsMap.end()) {
            configItr->second.erase(idx);
            // 如果配置为空，则移除整个配置
            if (configItr->second.empty()) {
                // 移除整个配置
                mPipelineNameJournalConfigsMap.erase(configItr);
            }
        }
    }
    
    // 移除config对应的连接
    JournalConfigGroupManager::GetInstance().RemoveConfig(configName, idx);
    
    LOG_INFO(sLogger, ("journal input removed without cleanup", "")("config", configName)("idx", idx));
}

JournalConfig JournalServer::GetJournalConfig(const string& name, size_t idx) const {
    lock_guard<mutex> lock(mUpdateMux);
    auto configItr = mPipelineNameJournalConfigsMap.find(name);
    if (configItr != mPipelineNameJournalConfigsMap.end()) {
        // 如果配置存在，则获取idx对应的配置
        auto idxItr = configItr->second.find(idx);
        if (idxItr != configItr->second.end()) {
            return idxItr->second;
        }
    }
    // 如果配置不存在，则返回空配置
    return JournalConfig();
}

void JournalServer::UpdateJournalConfigNeedsSeek(const std::string& configName, size_t idx, bool needsSeek) {
    lock_guard<mutex> lock(mUpdateMux);
    auto configItr = mPipelineNameJournalConfigsMap.find(configName);
    if (configItr != mPipelineNameJournalConfigsMap.end()) {
        auto idxItr = configItr->second.find(idx);
        if (idxItr != configItr->second.end()) {
            idxItr->second.needsSeek = needsSeek;
            // Updated needsSeek status for config: configName[idx]
        }
    }
}

// =============================================================================
// 3. 连接管理 - Connection Management
// =============================================================================

JournalServer::ConnectionPoolStats JournalServer::GetConnectionPoolStats() const {
    auto stats = JournalConfigGroupManager::GetInstance().GetStats();
    ConnectionPoolStats result;
    result.totalConnections = stats.totalConnections;
    result.activeConnections = stats.activeConnections;
    result.invalidConnections = stats.invalidConnections;
    result.connectionKeys = stats.connectionKeys;
    return result;
}

std::shared_ptr<SystemdJournalReader> JournalServer::GetConnectionInfo(const std::string& configName, size_t idx) const {
    return JournalConfigGroupManager::GetInstance().GetConnectionInfo(configName, idx);
}

bool JournalServer::ForceResetConnection(const std::string& configName, size_t idx) {
    // 先清理 epoll 监控
    CleanupEpollMonitoring(configName, idx);
    
    // 然后重置连接
    bool result = JournalConfigGroupManager::GetInstance().ForceResetConnection(configName, idx);
    
    if (result) {
        LOG_INFO(sLogger, ("connection reset completed with epoll cleanup", "")("config", configName)("idx", idx));
    }
    
    return result;
}

size_t JournalServer::GetConnectionCount() const {
    return JournalConfigGroupManager::GetInstance().GetConnectionCount();
}

// =============================================================================
// 4. 事件驱动辅助方法 - Event-driven Helper Methods
// =============================================================================

int JournalServer::GetGlobalEpollFD() const {
    std::lock_guard<std::mutex> lock(mEpollMutex);
    return mGlobalEpollFD;
}

void JournalServer::CleanupEpollMonitoring(const std::string& configName, size_t idx) {
    int epollFD = GetGlobalEpollFD();
    if (epollFD < 0) {
        LOG_WARNING(sLogger, ("cannot cleanup epoll monitoring", "epoll not initialized")("config", configName)("idx", idx));
        return;
    }
    
    auto reader = JournalConfigGroupManager::GetInstance().GetConnectionInfo(configName, idx);
    if (reader && reader->IsOpen()) {
        // Cleaning up epoll monitoring for config: configName[idx]
        reader->RemoveFromEpoll(epollFD);
    }
}

// =============================================================================
// 5. 配置分组管理 - Configuration Grouping Management
// =============================================================================

void JournalServer::EnableConfigGrouping() {
    std::lock_guard<std::mutex> lock(mGroupingMutex);
    mConfigGroupingEnabled = true;
    LOG_INFO(sLogger, ("configuration grouping optimization enabled", ""));
}

void JournalServer::DisableConfigGrouping() {
    std::lock_guard<std::mutex> lock(mGroupingMutex);
    mConfigGroupingEnabled = false;
    LOG_INFO(sLogger, ("configuration grouping optimization disabled", ""));
}

// =============================================================================
// 6. 事件驱动主循环 - Event-driven Main Loop
// =============================================================================

void JournalServer::run() {
    LOG_INFO(sLogger, ("JournalServer event-driven thread", "started"));
    
#ifdef __linux__
    // 创建全局 epoll 实例
    {
        std::lock_guard<std::mutex> lock(mEpollMutex);
        mGlobalEpollFD = epoll_create1(EPOLL_CLOEXEC);
        if (mGlobalEpollFD == -1) {
            LOG_ERROR(sLogger, ("failed to create epoll", "")("error", strerror(errno))("errno", errno));
            return;
        }
    }
    
    constexpr int kMaxEvents = 64;
    LOG_INFO(sLogger, ("global epoll instance created", "")("epoll_fd", mGlobalEpollFD)("max_events", kMaxEvents));
    
    struct epoll_event events[kMaxEvents];
    
    // 存储已监听的 reader 及其对应的配置信息
    std::map<int, MonitoredReader> monitoredReaders;
    
    while (mIsThreadRunning.load()) {
        try {
            int timeoutMs = 100; // 默认100ms超时
            
            // 更新连接监听状态
            updateReaderMonitoring(mGlobalEpollFD, monitoredReaders);
            
            // 等待事件
            int nfds = epoll_wait(mGlobalEpollFD, events, kMaxEvents, timeoutMs);
            
            if (nfds == -1) {
                if (errno == EINTR) {
                    continue; // 被信号中断，继续等待
                }
                LOG_ERROR(sLogger, ("epoll_wait failed", "")("error", strerror(errno)));
                break;
            }
            
            // 处理文件描述符事件
            for (int i = 0; i < nfds; i++) {
                int fd = events[i].data.fd;
                // Received epoll event for fd
                auto it = monitoredReaders.find(fd);
                if (it != monitoredReaders.end()) {
                    const auto& monitoredReader = it->second;
                    // Processing journal event for config: configName[idx]
                    if (monitoredReader.reader && monitoredReader.reader->ProcessJournalEvent()) {
                        // 有新数据，需要分发给所有使用相同journal reader的配置
                        processJournalEventForAllConfigs(monitoredReader.reader);
                    }
                    // Note: No new data available or event processed without data
                }
                // Note: Unknown fd in epoll event (might be already cleaned up)
            }
            
        } catch (const exception& e) {
            LOG_ERROR(sLogger, ("exception in JournalServer event loop", e.what()));
            this_thread::sleep_for(chrono::milliseconds(1000)); // 异常时等待1秒
        } catch (...) {
            LOG_ERROR(sLogger, ("unknown exception in JournalServer event loop", ""));
            this_thread::sleep_for(chrono::milliseconds(1000)); // 异常时等待1秒
        }
    }
    
    // 清理所有 reader 的 epoll 监控
    LOG_INFO(sLogger, ("cleaning up epoll monitoring", "")("monitored_readers", monitoredReaders.size()));
    for (auto& pair : monitoredReaders) {
        if (pair.second.reader) {
            // Removing reader from epoll: config[idx], fd
            pair.second.reader->RemoveFromEpoll(mGlobalEpollFD);
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(mEpollMutex);
        close(mGlobalEpollFD);
        mGlobalEpollFD = -1;
    }
    LOG_INFO(sLogger, ("JournalServer event-driven thread", "stopped"));
    
#else
    // 非Linux平台不支持事件驱动模式
    LOG_ERROR(sLogger, ("event-driven journal collection not supported on this platform", ""));
    return;
#endif
}

// =============================================================================
// 7. 事件驱动辅助方法 - Event-driven Helper Methods
// =============================================================================

void JournalServer::updateReaderMonitoring(int epollFD, std::map<int, MonitoredReader>& monitoredReaders) {
    // 获取当前配置
    const auto& allConfigs = GetAllJournalConfigs();
    
    // 检查需要添加监听的配置（只处理已经准备好的连接）
    for (const auto& pipelineConfig : allConfigs) {
        const std::string& configName = pipelineConfig.first;
        
        for (const auto& idxConfig : pipelineConfig.second) {
            size_t idx = idxConfig.first;
            
            // 只处理不需要seek的配置（即已经完成初始化的配置）
            if (idxConfig.second.needsSeek) {
                // Config needs seek, attempting to initialize: configName[idx]
                
                // 尝试获取连接并执行seek操作
                auto connection = JournalConfigGroupManager::GetInstance().GetConnectionInfo(configName, idx);
                if (connection && connection->IsOpen()) {
                    // 获取配置的副本并执行seek
                    JournalConfig config = idxConfig.second;
                    if (logtail::impl::PerformJournalSeek(configName, idx, config, connection, false)) {
                        // Seek成功，更新配置状态
                        UpdateJournalConfigNeedsSeek(configName, idx, false);
                        LOG_INFO(sLogger, ("journal seek completed, config ready for monitoring", "")("config", configName)("idx", idx));
                        
                        // 继续处理这个配置，将其添加到监控中
                        // 检查是否已经监听
                        bool alreadyMonitored = false;
                        for (const auto& pair : monitoredReaders) {
                            if (pair.second.reader == connection) {
                                alreadyMonitored = true;
                                break;
                            }
                        }
                        
                        if (!alreadyMonitored) {
                            // Attempting to add reader to epoll for config: configName[idx]
                            
                            // 检查 reader 状态
                            if (!connection->IsOpen()) {
                                LOG_WARNING(sLogger, ("reader is not open", "")("config", configName)("idx", idx));
                                continue;
                            }
                            
                            // Reader is open, checking journal fd
                            
                            // 检查 journal fd
                            int journalFD = connection->GetJournalFD();
                            if (journalFD < 0) {
                                LOG_WARNING(sLogger, ("journal fd is invalid", "")("config", configName)("idx", idx)("fd", journalFD)("errno", errno));
                                continue;
                            }
                            
                            // Journal fd obtained successfully
                            
                            // 添加 reader 到全局 epoll
                            if (connection->AddToEpoll(epollFD)) {
                                MonitoredReader monitoredReader;
                                monitoredReader.reader = connection;
                                monitoredReader.configName = configName;
                                monitoredReader.idx = idx;
                                monitoredReaders[journalFD] = monitoredReader;
                                LOG_INFO(sLogger, ("journal reader added to epoll monitoring", "")("config", configName)("idx", idx)("fd", journalFD));
                            } else {
                                LOG_WARNING(sLogger, ("failed to add reader to epoll", "")("config", configName)("idx", idx)("fd", journalFD)("epoll_fd", epollFD));
                            }
                        } else {
                            // Reader already monitored for config: configName[idx]
                        }
                    } else {
                        LOG_WARNING(sLogger, ("journal seek failed, config will be retried later", "")("config", configName)("idx", idx));
                    }
                } else {
                    LOG_WARNING(sLogger, ("no connection available for seek operation", "")("config", configName)("idx", idx));
                }
                continue; // 跳过未完成初始化的配置
            }
            
            auto connection = JournalConfigGroupManager::GetInstance().GetConnectionInfo(configName, idx);
            if (connection && connection->IsOpen()) {
                // Checking reader for epoll monitoring: configName[idx]
                
                // 检查是否已经监听
                bool alreadyMonitored = false;
                for (const auto& pair : monitoredReaders) {
                    if (pair.second.reader == connection) {
                        alreadyMonitored = true;
                        break;
                    }
                }
                
                if (!alreadyMonitored) {
                    // Attempting to add reader to epoll for config: configName[idx]
                    
                    // 检查 reader 状态
                    if (!connection->IsOpen()) {
                        LOG_WARNING(sLogger, ("reader is not open", "")("config", configName)("idx", idx));
                        continue;
                    }
                    
                    // Reader is open, checking journal fd
                    
                    // 检查 journal fd
                    int journalFD = connection->GetJournalFD();
                    if (journalFD < 0) {
                        LOG_WARNING(sLogger, ("journal fd is invalid", "")("config", configName)("idx", idx)("fd", journalFD)("errno", errno));
                        continue;
                    }
                    
                    // Journal fd obtained successfully
                    
                    // 添加 reader 到全局 epoll
                    if (connection->AddToEpoll(epollFD)) {
                        MonitoredReader monitoredReader;
                        monitoredReader.reader = connection;
                        monitoredReader.configName = configName;
                        monitoredReader.idx = idx;
                        monitoredReaders[journalFD] = monitoredReader;
                        LOG_INFO(sLogger, ("journal reader added to epoll monitoring", "")("config", configName)("idx", idx)("fd", journalFD));
                    } else {
                        LOG_WARNING(sLogger, ("failed to add reader to epoll", "")("config", configName)("idx", idx)("fd", journalFD)("epoll_fd", epollFD));
                    }
                } else {
                    // Reader already monitored for config: configName[idx]
                }
            }
        }
    }
}

void logtail::JournalServer::processSpecificJournalConfig(const std::string& configName, size_t idx) {
    // 获取指定配置
    JournalConfig config = GetJournalConfig(configName, idx);
    // Processing journal config: configName[idx], needsSeek=config.needsSeek, queueKey=config.queueKey
    
    if (config.queueKey == -1) {
        LOG_ERROR(sLogger, ("invalid config for specific processing", "")("config", configName)("idx", idx));
        return;
    }
    
    // 如果配置需要seek，跳过处理（等待下次updateFDMonitoring时重新初始化）
    if (config.needsSeek) {
        LOG_WARNING(sLogger, ("config needs seek, skipping event processing", "")("config", configName)("idx", idx)("needsSeek", config.needsSeek));
        return;
    }
    
    // 直接读取和处理journal条目（连接和seek已经完成）
    auto connection = JournalConfigGroupManager::GetInstance().GetConnectionInfo(configName, idx);
    if (!connection || !connection->IsOpen()) {
        LOG_ERROR(sLogger, ("connection not available for event processing", "")("config", configName)("idx", idx));
        return;
    }
    
    auto reader = connection;
    if (!reader || !reader->IsOpen()) {
        LOG_ERROR(sLogger, ("reader not available for event processing", "")("config", configName)("idx", idx));
        return;
    }
    
    // 直接读取和处理journal条目
    logtail::impl::ReadJournalEntries(configName, idx, config, reader, config.queueKey);
}

void logtail::JournalServer::processJournalEventForAllConfigs(const std::shared_ptr<SystemdJournalReader>& reader) {
    // 获取所有使用这个reader的配置
    auto configs = JournalConfigGroupManager::GetInstance().GetConfigsUsingReader(reader);
    
    // Processing journal event for all configs sharing this reader (count: configs.size())
    
    // 为每个配置处理journal条目
    for (const auto& [configName, idx] : configs) {
        // Processing journal event for config: configName[idx]
        processSpecificJournalConfig(configName, idx);
    }
}

bool logtail::JournalServer::validateJournalConfig(const std::string& configName, size_t idx, const JournalConfig& config, QueueKey& queueKey) {
    // 基本验证
    if (!config.ctx) {
        LOG_ERROR(sLogger, ("CRITICAL: no context available for journal config", "this indicates initialization problem")("config", configName)("idx", idx));
        return false;
    }
    
    // 从pipeline context获取queue key
    queueKey = config.ctx->GetProcessQueueKey();
    if (queueKey == -1) {
        LOG_WARNING(sLogger, ("no queue key available for journal config", "skip")("config", configName)("idx", idx));
        return false;
    }
    
    // 检查队列是否有效
    if (!ProcessQueueManager::GetInstance()->IsValidToPush(queueKey)) {
        // 队列无效，跳过该journal配置的处理
        return false;
    }
    
    return true;
}

// =============================================================================
// 5. 测试和调试支持 - Test and Debug Support
// =============================================================================

#ifdef APSARA_UNIT_TEST_MAIN
void JournalServer::Clear() {
    lock_guard<mutex> lock(mUpdateMux);
    mPipelineNameJournalConfigsMap.clear();
}
#endif

} // namespace logtail 
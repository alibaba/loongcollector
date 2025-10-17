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

#include <utility>

#ifdef __linux__
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#endif

#include "connection/JournalConnectionManager.h"
#include "reader/JournalReader.h"
#include "processor/JournalEntryProcessor.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "logger/Logger.h"


using namespace std;

namespace logtail {

// =============================================================================
// 生命周期管理 - Lifecycle Management
// =============================================================================

void JournalServer::Init() {
    lock_guard<mutex> lock(mInitMux);
    if (mIsInitialized) {
        LOG_INFO(sLogger, ("JournalServer already initialized", "skipping duplicate Init() call"));
        return;
    }
    
    mThreadRes = async(launch::async, &JournalServer::run, this);
    
    // 初始化连接管理器
    JournalConnectionManager::GetInstance().Initialize();
    
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
    
    // 清理连接管理器
    JournalConnectionManager::GetInstance().Cleanup();
    
    mIsInitialized = false;
    LOG_INFO(sLogger, ("JournalServer stopped", ""));
}

bool JournalServer::HasRegisteredPlugins() const {
    return JournalConnectionManager::GetInstance().GetConnectionCount() > 0;
}
// =============================================================================
// 配置管理 - Configuration Management
// =============================================================================

void JournalServer::AddJournalInput(const string& configName, size_t idx, const JournalConfig& config) {
    // 首先验证配置并获取队列键
    QueueKey queueKey = 0;
    if (!validateQueueKey(configName, idx, config, queueKey)) {
        LOG_ERROR(sLogger, ("journal input validation failed", "config not added")("config", configName)("idx", idx));
        return;
    }
    
    // 验证成功后，缓存queueKey并添加配置
    JournalConfig validatedConfig = config;
    validatedConfig.queueKey = queueKey;
    
    LOG_INFO(sLogger, ("journal input validated", "")("config", configName)("idx", idx)("ctx_valid", config.ctx != nullptr)("queue_key", queueKey));
    
    // 使用配置管理器添加配置
    auto connectionManager = &JournalConnectionManager::GetInstance();
    
    // 创建配置处理器
    auto handler = [this, configName, idx](const std::string&, size_t, const JournalEntry&) {
        // 处理 journal 条目
        processJournal(configName, idx);
    };
    
    if (connectionManager->AddConfig(configName, idx, validatedConfig, handler)) {
        LOG_INFO(sLogger, ("config added to manager", "")("config", configName)("idx", idx));
        
        // 记录统计信息
        auto stats = connectionManager->GetStats();
        LOG_INFO(sLogger, ("manager stats", "")("total_configs", stats.totalConfigs)("active_connections", stats.activeConnections));
    } else {
        LOG_ERROR(sLogger, ("failed to add config to manager", "")("config", configName)("idx", idx));
    }
}

void JournalServer::RemoveJournalInput(const string& configName, size_t idx) {
    // 清理 epoll 监控
    CleanupEpollMonitoring(configName, idx);
    
    // 移除config对应的连接（同时会移除配置）
    JournalConnectionManager::GetInstance().RemoveConfig(configName, idx);
    
    LOG_INFO(sLogger, ("journal input removed with automatic connection cleanup", "")("config", configName)("idx", idx));
}

void JournalServer::RemoveConfigOnly(const string& configName, size_t idx) {
    // 移除config对应的连接（不清理 epoll）
    JournalConnectionManager::GetInstance().RemoveConfig(configName, idx);
    
    LOG_INFO(sLogger, ("journal input removed without cleanup", "")("config", configName)("idx", idx));
}

std::map<std::pair<std::string, size_t>, JournalConfig> JournalServer::GetAllJournalConfigs() const {
    return JournalConnectionManager::GetInstance().GetAllConfigs();
}

// =============================================================================
// 连接管理 - Connection Management
// =============================================================================

JournalServer::ConnectionPoolStats JournalServer::GetConnectionPoolStats() const {
    auto stats = JournalConnectionManager::GetInstance().GetStats();
    ConnectionPoolStats result;
    result.totalConnections = stats.totalConnections;
    result.activeConnections = stats.activeConnections;
    result.invalidConnections = stats.invalidConnections;
    result.connectionKeys = stats.connectionKeys;
    return result;
}

std::shared_ptr<SystemdJournalReader> JournalServer::GetConnectionInfo(const std::string& configName, size_t idx) const {
    return JournalConnectionManager::GetInstance().GetConnection(configName, idx);
}

size_t JournalServer::GetConnectionCount() const {
    return JournalConnectionManager::GetInstance().GetConnectionCount();
}

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
    
    auto reader = JournalConnectionManager::GetInstance().GetConnection(configName, idx);
    if (reader && reader->IsOpen()) {
        // Cleaning up epoll monitoring for config: configName[idx]
        reader->RemoveFromEpoll(epollFD);
    }
}

// =============================================================================
// 事件驱动主循环 - Event-driven Main Loop
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
            refreshMonitors(mGlobalEpollFD, monitoredReaders);
            
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
                    
                    // 调用 ProcessJournalEvent() 处理 epoll 事件
                    // 返回值含义：
                    // - true: SD_JOURNAL_NOP, SD_JOURNAL_APPEND, SD_JOURNAL_INVALIDATE（都可以继续处理）
                    // - false: 错误情况（sd_journal_process 返回负值）
                    if (monitoredReader.reader && monitoredReader.reader->ProcessJournalEvent()) {
                        // 处理该配置的journal事件（每个reader对应一个独立的配置）
                        processJournal(monitoredReader.configName, monitoredReader.idx);
                    } else {
                        // ProcessJournalEvent 返回 false 表示遇到错误
                        // 记录警告日志，但不中断循环（继续处理其他配置）
                        if (monitoredReader.reader) {
                            LOG_WARNING(sLogger, 
                                       ("ProcessJournalEvent failed", 
                                        "sd_journal_process returned error, skipping this event")
                                       ("config", monitoredReader.configName)
                                       ("idx", monitoredReader.idx)
                                       ("fd", fd));
                        }
                    }
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

void JournalServer::refreshMonitors(int epollFD, std::map<int, MonitoredReader>& monitoredReaders) {
    // 获取当前配置
    auto allConfigs = GetAllJournalConfigs();
    
    // 检查需要添加监听的配置
    for (const auto& [configKey, config] : allConfigs) {
        const std::string& configName = configKey.first;
        size_t idx = configKey.second;
        
        auto connection = JournalConnectionManager::GetInstance().GetConnection(configName, idx);
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

void logtail::JournalServer::processJournal(const std::string& configName, size_t idx) {
    // 从 JournalConnectionManager 获取配置
    JournalConfig config = JournalConnectionManager::GetInstance().GetConfig(configName, idx);
    
    if (config.queueKey == -1) {
        LOG_ERROR(sLogger, ("invalid config for specific processing", "")("config", configName)("idx", idx));
        return;
    }
    
    // 读取和处理journal条目
    auto connection = JournalConnectionManager::GetInstance().GetConnection(configName, idx);
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
    ReadJournalEntries(configName, idx, config, reader, config.queueKey);
}

bool logtail::JournalServer::validateQueueKey(const std::string& configName, size_t idx, const JournalConfig& config, QueueKey& queueKey) {
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
// 测试和调试支持 - Test and Debug Support
// =============================================================================

#ifdef APSARA_UNIT_TEST_MAIN
void JournalServer::Clear() {
    // 配置已移至 JournalConnectionManager，通过 Cleanup() 清理
    JournalConnectionManager::GetInstance().Cleanup();
}
#endif

} // namespace logtail 
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

#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <set>
#include <utility>

#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "connection/JournalConnectionManager.h"
#include "logger/Logger.h"
#include "processor/JournalEntryProcessor.h"
#include "reader/JournalReader.h"


using namespace std;

namespace logtail {

void JournalServer::Init() {
    bool expected = false;
    if (!mIsInitialized.compare_exchange_strong(expected, true)) {
        LOG_INFO(sLogger, ("journal server already initialized", "skipping duplicate Init() call"));
        return;
    }

    mThreadRes = async(launch::async, &JournalServer::run, this);

    JournalConnectionManager::GetInstance().Initialize();

    LOG_INFO(sLogger, ("journal server initialized", ""));
}

void JournalServer::Stop() {
    bool expected = true;
    if (!mIsInitialized.compare_exchange_strong(expected, false)) {
        return;
    }

    if (!mThreadRes.valid()) {
        return;
    }
    // 设置停止标志·
    mIsThreadRunning.store(false);
    // 等待线程退出
    mThreadRes.get();

    JournalConnectionManager::GetInstance().Cleanup();

    LOG_INFO(sLogger, ("journal server stopped", ""));
}

bool JournalServer::HasRegisteredPlugins() const {
    return JournalConnectionManager::GetInstance().GetConnectionCount() > 0;
}

void JournalServer::AddJournalInput(const string& configName, const JournalConfig& config) {
    QueueKey queueKey = 0;
    if (!validateQueueKey(configName, config, queueKey)) {
        LOG_ERROR(sLogger, ("journal server input validation failed", "config not added")("config", configName));
        return;
    }

    JournalConfig validatedConfig = config;
    validatedConfig.mQueueKey = queueKey;

    LOG_INFO(sLogger,
             ("journal server input validated",
              "")("config", configName)("ctx_valid", config.mCtx != nullptr)("queue_key", queueKey));

    auto connectionManager = &JournalConnectionManager::GetInstance();

    if (connectionManager->AddConfig(configName, validatedConfig)) {
        auto stats = connectionManager->GetStats();
        LOG_INFO(sLogger,
                 ("journal server manager stats", "")("total_configs", stats.totalConfigs)("active_connections",
                                                                                           stats.activeConnections));
    } else {
        LOG_ERROR(sLogger, ("journal server failed to add config to manager", "")("config", configName));
    }
}

void JournalServer::RemoveJournalInput(const string& configName) {
    int epollFD = GetGlobalEpollFD();
    // remove config with epoll cleanup
    JournalConnectionManager::GetInstance().RemoveConfigWithEpollCleanup(configName, epollFD);
    LOG_INFO(sLogger, ("journal server input removed with automatic connection cleanup", "")("config", configName));
}

void JournalServer::RemoveConfigOnly(const string& configName) {
    // remove config withour epoll cleanup
    JournalConnectionManager::GetInstance().RemoveConfig(configName);

    LOG_INFO(sLogger, ("journal server input removed without cleanup", "")("config", configName));
}

std::map<std::string, JournalConfig> JournalServer::GetAllJournalConfigs() const {
    auto allConfigs = JournalConnectionManager::GetInstance().GetAllConfigs();

    // 过滤掉未验证的配置（mQueueKey == -1 表示未通过 validateQueueKey 验证）
    std::map<std::string, JournalConfig> validatedConfigs;
    for (const auto& [configName, config] : allConfigs) {
        if (config.mQueueKey != -1) {
            validatedConfigs[configName] = config;
        } else {
            LOG_DEBUG(sLogger, ("journal server filtering unvalidated config", "")("config", configName));
        }
    }

    return validatedConfigs;
}

JournalServer::ConnectionPoolStats JournalServer::GetConnectionPoolStats() const {
    auto stats = JournalConnectionManager::GetInstance().GetStats();
    ConnectionPoolStats result;
    result.totalConnections = stats.totalConnections;
    result.activeConnections = stats.activeConnections;
    result.invalidConnections = stats.invalidConnections;
    result.connectionKeys = stats.connectionKeys;
    return result;
}

std::shared_ptr<JournalReader> JournalServer::GetConnectionInfo(const std::string& configName) const {
    return JournalConnectionManager::GetInstance().GetConnection(configName);
}

size_t JournalServer::GetConnectionCount() const {
    return JournalConnectionManager::GetInstance().GetConnectionCount();
}

int JournalServer::GetGlobalEpollFD() const {
    std::lock_guard<std::mutex> lock(mEpollMutex);
    return mGlobalEpollFD;
}

void JournalServer::run() {
    LOG_INFO(sLogger, ("journal server event-driven thread", "started"));

#ifdef __linux__
    std::map<int, MonitoredReader> monitoredReaders;

    {
        std::lock_guard<std::mutex> lock(mEpollMutex);
        mGlobalEpollFD = epoll_create1(EPOLL_CLOEXEC);
        if (mGlobalEpollFD == -1) {
            LOG_ERROR(sLogger, ("journal server failed to create epoll", "")("error", strerror(errno))("errno", errno));
            return;
        }
    }

    constexpr int kMaxEvents = 64;
    LOG_INFO(
        sLogger,
        ("journal server global epoll instance created", "")("epoll_fd", mGlobalEpollFD)("max_events", kMaxEvents));

    struct epoll_event events[kMaxEvents];

    while (mIsThreadRunning.load()) {
        try {
           
            syncMonitors(mGlobalEpollFD, monitoredReaders);

            int nfds = epoll_wait(mGlobalEpollFD, events, kMaxEvents, kJournalEpollTimeoutMS);

            if (nfds == -1) {
                if (errno == EINTR) {
                    continue; // 被信号中断，继续等待
                }
                LOG_ERROR(sLogger, ("journal server epoll_wait failed", "")("error", strerror(errno)));
                break;
            }


            // 兜底逻辑：用于无事件更新，但是有pending data没处理完的场景
            if (nfds == 0 && handlePendingDataReaders(monitoredReaders)) {
                continue;
            }

            // 如果有epoll事件（nfds > 0）
            for (auto& pair : monitoredReaders) {
                int fd = pair.first;
                auto& monitoredReader = pair.second;

                if (!monitoredReader.reader) {
                    continue;
                }
                // 防止refresh connection期间使用已关闭的reader
                if (!monitoredReader.reader->IsOpen()) {
                    continue;
                }
                
                std::shared_ptr<JournalReader> currentReader;
                if (!validateAndGetCurrentReader(monitoredReader, currentReader)) {
                    continue;
                }
                
                JournalStatusType status = currentReader->CheckJournalStatus();

                if (status == JournalStatusType::kNop && !monitoredReader.hasPendingData && nfds == 0) {
                    continue; // 跳过无效读取
                }

                if (status != JournalStatusType::kError) {
                    // 正常状态（NOP/APPEND/INVALIDATE），处理该配置的journal事件
                    bool hasPendingData = false;

                    auto connectionManager = &JournalConnectionManager::GetInstance();
                    JournalConfig config = connectionManager->GetConfig(monitoredReader.configName);

                    HandleJournalEntries(
                        monitoredReader.configName, config, currentReader, config.mQueueKey, &hasPendingData);

                    monitoredReader.hasPendingData = hasPendingData;
                } else {
                    LOG_WARNING(sLogger,
                                ("journal server CheckJournalStatus failed",
                                 "sd_journal_process returned error, skipping this event")(
                                    "config", monitoredReader.configName)("fd", fd));
                    monitoredReader.hasPendingData = false;
                }
            }

        } catch (const exception& e) {
            LOG_ERROR(sLogger, ("journal server exception in event loop", e.what()));
            this_thread::sleep_for(chrono::milliseconds(1000)); // 异常时等待1秒
        } catch (...) {
            LOG_ERROR(sLogger, ("journal server unknown exception in event loop", ""));
            this_thread::sleep_for(chrono::milliseconds(1000)); // 异常时等待1秒
        }
    }

    // 清理所有 reader 的 epoll 监控
    LOG_INFO(sLogger,
             ("journal server cleaning up epoll monitoring", "")("monitored_readers", monitoredReaders.size()));
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
    LOG_INFO(sLogger, ("journal server event-driven thread", "stopped"));
#endif
}

void JournalServer::syncMonitors(int epollFD, std::map<int, MonitoredReader>& monitoredReaders) {
    cleanupStaleReaders(epollFD, monitoredReaders);
    refreshConnections(epollFD, monitoredReaders);
    addNewReaders(epollFD, monitoredReaders);
}

void JournalServer::cleanupStaleReaders(int epollFD, std::map<int, MonitoredReader>& monitoredReaders) {
    static int cleanupCounter = 0;
    if (++cleanupCounter >= 50) {
        cleanupCounter = 0;
        auto connectionManager = &JournalConnectionManager::GetInstance();
        connectionManager->CleanupInvalidMonitoredReaders(epollFD, monitoredReaders);
    }
}

void JournalServer::refreshConnections(int epollFD, std::map<int, MonitoredReader>& monitoredReaders) {
    auto connectionManager = &JournalConnectionManager::GetInstance();
    auto allConfigs = GetAllJournalConfigs();

    for (const auto& [configName, config] : allConfigs) {
        if (connectionManager->ShouldRefreshConnection(configName)) {

            if (connectionManager->RefreshConnection(configName, epollFD)) {
                LOG_INFO(sLogger,
                         ("journal server connection refreshed successfully", "")("config", configName));
                syncReaderAfterRefresh(configName, monitoredReaders);
            } else {
                LOG_ERROR(sLogger, ("journal server connection refresh failed", "")("config", configName));
            }
        }
    }
}

void JournalServer::syncReaderAfterRefresh(const std::string& configName,
                                            std::map<int, MonitoredReader>& monitoredReaders) {
    auto connectionManager = &JournalConnectionManager::GetInstance();
    auto connection = connectionManager->GetConnection(configName);

    if (!connection || !connection->IsOpen()) {
        return;
    }

    int newJournalFD = connection->GetJournalFD();
    if (newJournalFD < 0) {
        return;
    }

    for (auto it = monitoredReaders.begin(); it != monitoredReaders.end(); ++it) {
        if (it->second.configName == configName) {
            // 如果fd变化了，需要更新key
            if (it->first != newJournalFD) {
                MonitoredReader mr = it->second;
                monitoredReaders.erase(it);
                monitoredReaders[newJournalFD] = mr;
            } else {
                // FD 没变化，确保 reader 指针正确
                it->second.reader = connection;
            }
            return;
        }
    }
}

void JournalServer::addNewReaders(int epollFD, std::map<int, MonitoredReader>& monitoredReaders) {
    auto connectionManager = &JournalConnectionManager::GetInstance();
    auto openConnections = connectionManager->GetOpenConnections();

    for (const auto& [configName, reader] : openConnections) {
        bool alreadyMonitored = false;
        for (const auto& pair : monitoredReaders) {
            if (pair.second.reader == reader) {
                alreadyMonitored = true;
                break;
            }
        }

        if (!alreadyMonitored) {
            int journalFD = reader->AddToEpollAndGetFD(epollFD);
            if (journalFD < 0) {
                LOG_WARNING(sLogger,
                            ("journal server failed to add reader to epoll",
                             "")("config", configName)("epoll_fd", epollFD));
                continue;
            }

            MonitoredReader monitoredReader;
            monitoredReader.reader = reader;
            monitoredReader.configName = configName;
            monitoredReaders[journalFD] = monitoredReader;

            LOG_INFO(sLogger,
                     ("journal server reader added to epoll monitoring", "")("config", configName)("fd", journalFD));
        }
    }
}

bool JournalServer::handlePendingDataReaders(std::map<int, MonitoredReader>& monitoredReaders) {
    bool hasReadersWithPendingData = false;
    for (const auto& pair : monitoredReaders) {
        if (pair.second.hasPendingData) {
            hasReadersWithPendingData = true;
            break;
        }
    }

    // 只有当存在hasPendingData的reader时，才进行兜底读取
    if (!hasReadersWithPendingData) {
        return false;
    }

    LOG_DEBUG(sLogger,
              ("journal server epoll timeout with pending data, fallback reading", "")("monitored_readers",
                                                                                       monitoredReaders.size()));

    for (auto it = monitoredReaders.begin(); it != monitoredReaders.end(); ++it) {
        auto& monitoredReader = it->second;
        if (!monitoredReader.reader) {
            continue;
        }
        // 防止在RefreshConnection期间使用已关闭的reader
        if (!monitoredReader.reader->IsOpen()) {
            monitoredReader.hasPendingData = false;
            continue;
        }

        // 只对有hasPendingData标志的reader进行读取
        if (monitoredReader.hasPendingData) {
            std::shared_ptr<JournalReader> currentReader;
            if (!validateAndGetCurrentReader(monitoredReader, currentReader)) {
                monitoredReader.hasPendingData = false;
                continue;
            }
            
            bool hasPendingData = false;
            auto connectionManager = &JournalConnectionManager::GetInstance();
            JournalConfig config = connectionManager->GetConfig(monitoredReader.configName);
            
            HandleJournalEntries(
                monitoredReader.configName, config, currentReader, config.mQueueKey, &hasPendingData);
            monitoredReader.hasPendingData = hasPendingData;
        }
    }

    return true;
}

bool JournalServer::validateAndGetCurrentReader(const MonitoredReader& monitoredReader,
                                                 std::shared_ptr<JournalReader>& currentReaderOut) const {
    auto connectionManager = &JournalConnectionManager::GetInstance();
    auto currentReader = connectionManager->GetConnection(monitoredReader.configName);
    
    if (!currentReader || currentReader != monitoredReader.reader) {
        LOG_DEBUG(sLogger,
                  ("journal server reader changed during processing, skipping",
                   "will sync on next iteration")("config", monitoredReader.configName));
        return false;
    }
    
    if (!currentReader->IsOpen()) {
        return false;
    }
    
    currentReaderOut = currentReader;
    return true;
}

bool JournalServer::validateQueueKey(const std::string& configName, const JournalConfig& config, QueueKey& queueKey) {
    if (!config.mCtx) {
        LOG_ERROR(sLogger,
                  ("journal server CRITICAL: no context available for config",
                   "this indicates initialization problem")("config", configName));
        return false;
    }

    // 如果配置中已经有queueKey，直接使用（用于测试环境）
    if (config.mQueueKey != -1) {
        queueKey = config.mQueueKey;
        LOG_INFO(sLogger, ("journal server using pre-set queue key", "")("config", configName)("queue_key", queueKey));
        return true;
    }

    queueKey = config.mCtx->GetProcessQueueKey();
    if (queueKey == -1) {
        LOG_WARNING(sLogger, ("journal server no queue key available for config", "skip")("config", configName));
        return false;
    }

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

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
#include "reader/JournalConnection.h"
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

    JournalConnection::GetInstance().Initialize();

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

    JournalConnection::GetInstance().Cleanup();

    LOG_INFO(sLogger, ("journal server stopped", ""));
}

bool JournalServer::HasRegisteredPlugins() const {
    return JournalConnection::GetInstance().GetConnectionCount() > 0;
}

void JournalServer::AddJournalInput(const string& configName, const JournalConfig& config) {
    QueueKey queueKey = 0;
    if (!getOrValidateQueueKey(configName, config, queueKey)) {
        LOG_ERROR(sLogger, ("journal server input validation failed", "config not added")("config", configName));
        return;
    }

    JournalConfig validatedConfig = config;
    validatedConfig.mQueueKey = queueKey;

    LOG_INFO(sLogger,
             ("journal server input validated",
              "")("config", configName)("ctx_valid", config.mCtx != nullptr)("queue_key", queueKey));

    auto connectionManager = &JournalConnection::GetInstance();

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
    JournalConnection::GetInstance().RemoveConfigWithEpollCleanup(configName, epollFD);
    LOG_INFO(sLogger, ("journal server input removed with automatic connection cleanup", "")("config", configName));
}

void JournalServer::RemoveConfigOnly(const string& configName) {
    JournalConnection::GetInstance().RemoveConfig(configName);

    LOG_INFO(sLogger, ("journal server input removed without cleanup", "")("config", configName));
}

std::map<std::string, JournalConfig> JournalServer::GetAllJournalConfigs() const {
    auto allConfigs = JournalConnection::GetInstance().GetAllConfigs();

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
            syncMonitoredReaders(mGlobalEpollFD, monitoredReaders);

            int nfds = epoll_wait(mGlobalEpollFD, events, kMaxEvents, kJournalEpollTimeoutMS);

            if (nfds == -1) {
                if (errno == EINTR) {
                    continue; // 被信号中断，继续等待
                }
                LOG_ERROR(sLogger, ("journal server epoll_wait failed", "")("error", strerror(errno)));
                break;
            }


            // 兜底逻辑：用于无事件更新，但是有pending data没处理完的场景
            if (nfds == 0 && processPendingDataReaders(monitoredReaders)) {
                continue;
            }

            for (auto& pair : monitoredReaders) {
                auto& monitoredReader = pair.second;

                std::shared_ptr<JournalReader> currentReader;
                if (!getValidatedCurrentReader(monitoredReader, currentReader)) {
                    continue;
                }

                JournalStatusType status = currentReader->CheckJournalStatus();

                if (status == JournalStatusType::kNop && !monitoredReader.hasPendingData && nfds == 0) {
                    continue;
                }

                if (status != JournalStatusType::kError) {
                    // 正常状态（NOP/APPEND/INVALIDATE），处理该配置的journal事件
                    auto connectionManager = &JournalConnection::GetInstance();
                    JournalConfig config = connectionManager->GetConfig(monitoredReader.configName);

                    bool timeoutTrigger = isBatchTimeoutExceeded(monitoredReader, config);

                    HandleJournalEntries(monitoredReader.configName,
                                         config,
                                         currentReader,
                                         config.mQueueKey,
                                         timeoutTrigger,
                                         &monitoredReader.accumulatedEventGroup,
                                         &monitoredReader.accumulatedEntryCount,
                                         &monitoredReader.accumulatedFirstCursor,
                                         &monitoredReader.hasPendingData,
                                         &monitoredReader.lastBatchTime);
                } else {
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

    LOG_INFO(sLogger,
             ("journal server cleaning up epoll monitoring", "")("monitored_readers", monitoredReaders.size()));
    for (auto& pair : monitoredReaders) {
        if (pair.second.reader) {
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

void JournalServer::syncMonitoredReaders(int epollFD, std::map<int, MonitoredReader>& monitoredReaders) {
    lazyCleanupRemovedReadersFromEpoll(epollFD, monitoredReaders);
    refreshReaderConnectionsByInterval(epollFD, monitoredReaders);
    addReadersToEpollMonitoring(epollFD, monitoredReaders);
}

void JournalServer::lazyCleanupRemovedReadersFromEpoll(int epollFD, std::map<int, MonitoredReader>& monitoredReaders) {
    static int cleanupCounter = 0;
    if (++cleanupCounter >= 50) {
        cleanupCounter = 0;
        auto connectionManager = &JournalConnection::GetInstance();
        connectionManager->CleanupRemovedReadersFromEpoll(epollFD, monitoredReaders);
    }
}

void JournalServer::refreshReaderConnectionsByInterval(int epollFD, std::map<int, MonitoredReader>& monitoredReaders) {
    auto connectionManager = &JournalConnection::GetInstance();
    auto allConfigs = GetAllJournalConfigs();

    for (const auto& [configName, config] : allConfigs) {
        if (connectionManager->ShouldRefreshConnection(configName)) {
            if (connectionManager->RefreshConnection(configName, epollFD)) {
                syncReaderFDMappingAfterRefresh(configName, monitoredReaders);
            } else {
                LOG_ERROR(sLogger, ("journal server connection refresh failed", "")("config", configName));
            }
        }
    }
}

void JournalServer::syncReaderFDMappingAfterRefresh(const std::string& configName,
                                                     std::map<int, MonitoredReader>& monitoredReaders) {
    auto connectionManager = &JournalConnection::GetInstance();
    auto connection = connectionManager->GetConnection(configName);

    if (!connection || !connection->IsOpen()) {
        return;
    }

    int newJournalFD = connection->GetJournalFD();
    if (newJournalFD < 0) {
        return;
    }
    // Reader object does not change; it's just reopened after close, but the FD may change
    for (auto it = monitoredReaders.begin(); it != monitoredReaders.end(); ++it) {
        if (it->second.configName == configName) {
            if (it->first != newJournalFD) {
                // Preserve state (pendingData, accumulatedEventGroup) when moving entry to new FD
                MonitoredReader mr = it->second;
                monitoredReaders.erase(it);
                monitoredReaders[newJournalFD] = mr;
            } else {
                it->second.reader = connection;
            }
            return;
        }
    }
}

void JournalServer::addReadersToEpollMonitoring(int epollFD, std::map<int, MonitoredReader>& monitoredReaders) {
    auto connectionManager = &JournalConnection::GetInstance();
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
                LOG_WARNING(
                    sLogger,
                    ("journal server failed to add reader to epoll", "")("config", configName)("epoll_fd", epollFD));
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

bool JournalServer::processPendingDataReaders(std::map<int, MonitoredReader>& monitoredReaders) {
    bool hasReadersWithPendingData = false;
    for (const auto& pair : monitoredReaders) {
        if (pair.second.hasPendingData) {
            hasReadersWithPendingData = true;
            break;
        }
    }

    if (!hasReadersWithPendingData) {
        return false;
    }

    for (auto it = monitoredReaders.begin(); it != monitoredReaders.end(); ++it) {
        auto& monitoredReader = it->second;

        if (monitoredReader.hasPendingData) {
            std::shared_ptr<JournalReader> currentReader;
            if (!getValidatedCurrentReader(monitoredReader, currentReader)) {
                continue;
            }

            auto connectionManager = &JournalConnection::GetInstance();
            JournalConfig config = connectionManager->GetConfig(monitoredReader.configName);

            bool timeoutTrigger = isBatchTimeoutExceeded(monitoredReader, config);

            HandleJournalEntries(monitoredReader.configName,
                                 config,
                                 currentReader,
                                 config.mQueueKey,
                                 timeoutTrigger,
                                 &monitoredReader.accumulatedEventGroup,
                                 &monitoredReader.accumulatedEntryCount,
                                 &monitoredReader.accumulatedFirstCursor,
                                 &monitoredReader.hasPendingData,
                                 &monitoredReader.lastBatchTime);
        }
    }

    return true;
}

bool JournalServer::getValidatedCurrentReader(MonitoredReader& monitoredReader,
                                              std::shared_ptr<JournalReader>& currentReaderOut) const {
    // Validate reader to prevent using a closed reader during connection refresh
    if (!monitoredReader.reader) {
        return false;
    }

    if (!monitoredReader.reader->IsOpen()) {
        clearPendingDataForInvalidReader(monitoredReader);
        return false;
    }

    auto connectionManager = &JournalConnection::GetInstance();
    auto currentReader = connectionManager->GetConnection(monitoredReader.configName);

    if (!currentReader || currentReader != monitoredReader.reader) {
        LOG_DEBUG(sLogger,
                  ("journal server reader changed during processing, skipping",
                   "will sync on next iteration")("config", monitoredReader.configName));
        clearPendingDataForInvalidReader(monitoredReader);
        return false;
    }

    if (!currentReader->IsOpen()) {
        clearPendingDataForInvalidReader(monitoredReader);
        return false;
    }

    currentReaderOut = currentReader;
    return true;
}

void JournalServer::clearPendingDataForInvalidReader(MonitoredReader& monitoredReader) const {
    // Clear pending data and accumulated state when reader validation fails (unavailable, closed, or replaced)
    if (monitoredReader.hasPendingData) {
        monitoredReader.hasPendingData = false;
        monitoredReader.accumulatedEventGroup = nullptr;
        monitoredReader.accumulatedEntryCount = 0;
        monitoredReader.accumulatedFirstCursor.clear();
    }
}

bool JournalServer::isBatchTimeoutExceeded(const MonitoredReader& monitoredReader, const JournalConfig& config) const {
    if (monitoredReader.accumulatedEventGroup == nullptr || monitoredReader.accumulatedEntryCount == 0) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - monitoredReader.lastBatchTime).count();
    bool timeoutTrigger = elapsed >= config.mBatchTimeoutMs;

    if (timeoutTrigger) {
        LOG_DEBUG(sLogger,
                  ("journal server forcing flush accumulated batch due to timeout",
                   "")("config", monitoredReader.configName)("elapsed_ms", elapsed)(
                      "batch_timeout_ms", config.mBatchTimeoutMs)("accumulated_count",
                                                                  monitoredReader.accumulatedEntryCount));
    }

    return timeoutTrigger;
}

bool JournalServer::getOrValidateQueueKey(const std::string& configName, const JournalConfig& config, QueueKey& queueKey) {
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

    return true;
}

// =============================================================================
// 测试和调试支持 - Test and Debug Support
// =============================================================================

#ifdef APSARA_UNIT_TEST_MAIN
void JournalServer::Clear() {
    // 配置已移至 JournalConnection，通过 Cleanup() 清理
    JournalConnection::GetInstance().Cleanup();
}
#endif

} // namespace logtail

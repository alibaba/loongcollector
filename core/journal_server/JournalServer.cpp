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

#include <memory>
#include <set>
#include <utility>

#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "logger/Logger.h"
#include "manager/JournalConnection.h"
#include "manager/JournalMonitor.h"
#include "monitor/AlarmManager.h"
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

    JournalConnection::GetInstance().Initialize();

    if (!JournalMonitor::GetInstance()->Initialize()) {
        LOG_ERROR(sLogger, ("journal server failed to initialize reader monitor", ""));
        mIsInitialized.store(false);
        return;
    }

    mThreadRes = async(launch::async, &JournalServer::run, this);

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

    // Set stop flag
    mIsThreadRunning.store(false);

    // Wait for thread to exit with timeout
    bool alarmOnce = false;
    while (mThreadRes.valid()) {
        std::future_status status = mThreadRes.wait_for(std::chrono::seconds(10));
        if (status == std::future_status::ready) {
            LOG_DEBUG(sLogger, ("journal server thread", "stopped successfully"));
            break;
        }
        if (!alarmOnce) {
            LOG_ERROR(sLogger, ("journal server thread stop", "too slow"));
            AlarmManager::GetInstance()->SendAlarmError(CONFIG_UPDATE_ALARM,
                                                        std::string("JournalServer stop too slow"));
            alarmOnce = true;
        }
    }

    JournalMonitor::GetInstance()->Cleanup();

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

    auto& connectionManager = JournalConnection::GetInstance();

    if (connectionManager.AddConfig(configName, validatedConfig)) {
        auto stats = connectionManager.GetStats();
        LOG_INFO(sLogger,
                 ("journal server manager stats", "")("total_configs", stats.totalConfigs)("active_connections",
                                                                                           stats.activeConnections));
    } else {
        LOG_ERROR(sLogger, ("journal server failed to add config to manager", "")("config", configName));
    }
}

void JournalServer::RemoveJournalInput(const string& configName) {
    JournalMonitor::GetInstance()->MarkReaderAsClosing(configName);
    JournalMonitor::GetInstance()->RemoveReaderFromMonitoring(configName);

    JournalConnection::GetInstance().RemoveConfig(configName);

    LOG_INFO(sLogger, ("journal server input removed", "")("config", configName));
}


std::vector<std::string> JournalServer::GetAllJournalConfigNames() const {
    auto allConfigs = JournalConnection::GetInstance().GetAllConfigs();

    // Filter out unvalidated configs (mQueueKey == -1 means failed validateQueueKey validation)
    std::vector<std::string> validatedConfigNames;
    for (const auto& [configName, config] : allConfigs) {
        if (config.mQueueKey != -1) {
            validatedConfigNames.push_back(configName);
        } else {
            LOG_DEBUG(sLogger, ("journal server filtering unvalidated config", "")("config", configName));
        }
    }

    return validatedConfigNames;
}

void JournalServer::run() {
    LOG_INFO(sLogger, ("journal server event-driven thread", "started"));

#ifdef __linux__
    auto* readerMonitor = JournalMonitor::GetInstance();
    auto& monitoredReaders = readerMonitor->GetMonitoredReaders();
    int epollFD = readerMonitor->GetEpollFD();
    auto& connectionManager = JournalConnection::GetInstance();

    constexpr int kMaxEvents = 64;
    struct epoll_event events[kMaxEvents];

    while (mIsThreadRunning.load()) {
        try {
            // Lazy cleanup: Remove readers that have been marked as closing
            // This ensures any ongoing journal reads from previous iteration have completed
            readerMonitor->CleanupClosedReaders();

            auto configNames = GetAllJournalConfigNames();
            connectionManager.RefreshConnectionsByInterval(configNames, *JournalMonitor::GetInstance());
            readerMonitor->AddReadersToMonitoring(configNames);

            int nfds = epoll_wait(epollFD, events, kMaxEvents, kJournalEpollTimeoutMS);

            if (nfds == -1) {
                if (errno == EINTR) {
                    continue; // Interrupted by signal, continue waiting
                }
                LOG_ERROR(sLogger, ("journal server epoll_wait failed", "")("error", strerror(errno)));
                break;
            }

            // Build set of FDs that have events (when nfds > 0)
            std::set<int> activeFDs;
            if (nfds > 0) {
                for (int i = 0; i < nfds; ++i) {
                    activeFDs.insert(events[i].data.fd);
                }
            }

            // Fallback: process pending data when no events (nfds == 0)
            // Note: When there are events (nfds > 0), pending data is processed in the main loop below
            if (nfds == 0 && processPendingDataWhenNoEvents(monitoredReaders)) {
                continue;
            }

            for (auto& pair : monitoredReaders) {
                auto& monitoredReader = pair.second;
                int readerFD = pair.first;

                // Skip if reader is marked as closing (Use-After-Close prevention)
                // This allows ongoing operations to complete but prevents new operations
                if (monitoredReader.isClosing.load()) {
                    continue;
                }

                // Skip if no event and no pending data (when there are events)
                if (nfds > 0 && activeFDs.find(readerFD) == activeFDs.end() && !monitoredReader.hasPendingData) {
                    continue;
                }

                std::shared_ptr<JournalReader> currentReader;
                if (!readerMonitor->GetValidatedCurrentReader(monitoredReader, currentReader)) {
                    continue;
                }

                // Double-check closing status after getting reader (race condition protection)
                if (monitoredReader.isClosing.load()) {
                    continue;
                }

                JournalStatusType status = currentReader->CheckJournalStatus();

                // Skip if no new data and no pending data (when no events)
                if (status == JournalStatusType::kNop && !monitoredReader.hasPendingData && nfds == 0) {
                    continue;
                }

                if (status != JournalStatusType::kError) {
                    // Normal status (NOP/APPEND/INVALIDATE), process journal events for this config
                    // Note: HandleJournalEntries merges accumulated data (pendingData) with new entries
                    processMonitoredReader(monitoredReader, currentReader);
                } else {
                    monitoredReader.hasPendingData = false;
                }
            }

        } catch (const exception& e) {
            LOG_ERROR(sLogger, ("journal server exception in event loop", e.what()));
            this_thread::sleep_for(chrono::milliseconds(1000)); // Wait 1 second on exception
        } catch (...) {
            LOG_ERROR(sLogger, ("journal server unknown exception in event loop", ""));
            this_thread::sleep_for(chrono::milliseconds(1000)); // Wait 1 second on exception
        }
    }

    LOG_INFO(sLogger, ("journal server event-driven thread", "stopped"));
#endif
}


bool JournalServer::processPendingDataWhenNoEvents(std::map<int, MonitoredReader>& monitoredReaders) {
    // Check if there are any readers with pending data
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

    auto* readerMonitor = JournalMonitor::GetInstance();

    // Process all readers with pending data
    for (auto& pair : monitoredReaders) {
        auto& monitoredReader = pair.second;

        // Skip if reader is marked as closing (Use-After-Close prevention)
        if (monitoredReader.isClosing.load()) {
            continue;
        }

        if (monitoredReader.hasPendingData) {
            std::shared_ptr<JournalReader> currentReader;
            if (!readerMonitor->GetValidatedCurrentReader(monitoredReader, currentReader)) {
                continue;
            }

            // Double-check closing status after getting reader (race condition protection)
            if (monitoredReader.isClosing.load()) {
                continue;
            }

            processMonitoredReader(monitoredReader, currentReader);
        }
    }

    return true;
}

void JournalServer::processMonitoredReader(MonitoredReader& monitoredReader,
                                           const std::shared_ptr<JournalReader>& currentReader) {
    // Get connection manager and config
    auto& connectionManager = JournalConnection::GetInstance();
    JournalConfig config = connectionManager.GetConfig(monitoredReader.configName);

    // Calculate timeout trigger
    bool timeoutTrigger
        = JournalMonitor::GetInstance()->IsBatchTimeoutExceeded(monitoredReader, config.mBatchTimeoutMs);

    // Process journal entries
    // Note: HandleJournalEntries merges accumulated data (pendingData) with new entries
    HandleJournalEntries(monitoredReader.configName,
                         config,
                         currentReader,
                         config.mQueueKey,
                         timeoutTrigger,
                         &monitoredReader.accumulatedEventGroup,
                         &monitoredReader.accumulatedEntryCount,
                         &monitoredReader.hasPendingData,
                         &monitoredReader.lastBatchTime);
}

bool JournalServer::getOrValidateQueueKey(const std::string& configName,
                                          const JournalConfig& config,
                                          QueueKey& queueKey) {
    if (!config.mCtx) {
        LOG_ERROR(sLogger,
                  ("journal server CRITICAL: no context available for config",
                   "this indicates initialization problem")("config", configName));
        return false;
    }

    // If config already has queueKey, use it directly (for test environment)
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
// Test and Debug Support
// =============================================================================

#ifdef APSARA_UNIT_TEST_MAIN
void JournalServer::Clear() {
    // Configs have been moved to JournalConnection, cleaned up via Cleanup()
    JournalConnection::GetInstance().Cleanup();
}
#endif

} // namespace logtail

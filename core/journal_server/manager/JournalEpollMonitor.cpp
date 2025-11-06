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

#include "JournalEpollMonitor.h"

#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <set>

#include "../JournalServer.h"
#include "JournalConnection.h"
#include "../reader/JournalReader.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail {

JournalEpollMonitor::JournalEpollMonitor() = default;

JournalEpollMonitor::~JournalEpollMonitor() {
    Cleanup();
}

bool JournalEpollMonitor::Initialize() {
    if (mInitialized.load()) {
        return true;
    }

#ifdef __linux__
    {
        std::lock_guard<std::mutex> lock(mEpollMutex);
        mEpollFD = epoll_create1(EPOLL_CLOEXEC);
        if (mEpollFD == -1) {
            LOG_ERROR(sLogger, ("journal reader monitor failed to create epoll", "")("error", strerror(errno))("errno", errno));
            return false;
        }
    }

    LOG_INFO(sLogger, ("journal reader monitor initialized", "")("epoll_fd", mEpollFD));
    mInitialized.store(true);
    return true;
#else
    return false;
#endif
}

void JournalEpollMonitor::Cleanup() {
    if (!mInitialized.load()) {
        return;
    }

#ifdef __linux__
    LOG_INFO(sLogger,
             ("journal reader monitor cleaning up epoll monitoring", "")("monitored_readers", mMonitoredReaders.size()));
    
    for (auto& pair : mMonitoredReaders) {
        if (pair.second.reader) {
            pair.second.reader->RemoveFromEpoll(mEpollFD);
        }
    }
    mMonitoredReaders.clear();

    {
        std::lock_guard<std::mutex> lock(mEpollMutex);
        if (mEpollFD >= 0) {
            close(mEpollFD);
            mEpollFD = -1;
        }
    }

    mInitialized.store(false);
    LOG_INFO(sLogger, ("journal reader monitor cleaned up", ""));
#endif
}

int JournalEpollMonitor::GetEpollFD() const {
    std::lock_guard<std::mutex> lock(mEpollMutex);
    return mEpollFD;
}

std::map<int, MonitoredReader>& JournalEpollMonitor::GetMonitoredReaders() {
    return mMonitoredReaders;
}

void JournalEpollMonitor::SyncMonitoredReaders() {
    lazyCleanupRemovedReadersFromEpoll();
    refreshReaderConnectionsByInterval();
    addReadersToEpollMonitoring();
}

void JournalEpollMonitor::lazyCleanupRemovedReadersFromEpoll() {
    static int cleanupCounter = 0;
    if (++cleanupCounter >= 50) {
        cleanupCounter = 0;
        cleanupRemovedReadersFromEpoll();
    }
}

int JournalEpollMonitor::cleanupRemovedReadersFromEpoll() {
    auto& connectionManager = JournalConnection::GetInstance();
    int cleanedCount = 0;

    for (auto it = mMonitoredReaders.begin(); it != mMonitoredReaders.end();) {
        auto& monitoredReader = it->second;

        // Check if configuration still exists by checking if connection exists
        auto connection = connectionManager.GetConnection(monitoredReader.configName);
        if (connection == nullptr) {
            LOG_DEBUG(sLogger,
                      ("journal reader monitor removing reader from monitoring",
                       "config no longer exists")("config", monitoredReader.configName)("fd", it->first));

            // Remove from epoll
            if (monitoredReader.reader && monitoredReader.reader->IsOpen()) {
                monitoredReader.reader->RemoveFromEpoll(mEpollFD);
            }

            it = mMonitoredReaders.erase(it);
            cleanedCount++;
        } else {
            ++it;
        }
    }

    return cleanedCount;
}

void JournalEpollMonitor::refreshReaderConnectionsByInterval() {
    auto connectionManager = &JournalConnection::GetInstance();
    auto server = JournalServer::GetInstance();
    
    // Get all validated configs from JournalServer (only configs with valid queueKey)
    auto allConfigs = server->GetAllJournalConfigs();

    for (const auto& [configName, config] : allConfigs) {
        if (connectionManager->ShouldRefreshConnection(configName)) {
            // Remove reader from epoll before refresh
            RemoveReaderFromEpoll(configName);

            // Refresh connection (without epoll operations)
            if (connectionManager->RefreshConnection(configName)) {
                // Re-add reader to epoll after refresh
                syncReaderFDMappingAfterRefresh(configName);
            } else {
                LOG_ERROR(sLogger, ("journal reader monitor connection refresh failed", "")("config", configName));
            }
        }
    }
}

void JournalEpollMonitor::RemoveReaderFromEpoll(const std::string& configName) {
    for (auto it = mMonitoredReaders.begin(); it != mMonitoredReaders.end(); ++it) {
        if (it->second.configName == configName) {
            // Remove reader from epoll
            if (it->second.reader && it->second.reader->IsOpen()) {
                it->second.reader->RemoveFromEpoll(mEpollFD);
            }
            // Note: Don't erase from mMonitoredReaders here, caller will handle cleanup
            return;
        }
    }
}

void JournalEpollMonitor::syncReaderFDMappingAfterRefresh(const std::string& configName) {
    auto connectionManager = &JournalConnection::GetInstance();
    auto currentReader = connectionManager->GetConnection(configName);

    if (!currentReader || !currentReader->IsOpen()) {
        LOG_WARNING(sLogger,
                    ("journal reader monitor reader not available after refresh", "")("config", configName));
        return;
    }

    // Find and update the monitored reader entry
    for (auto it = mMonitoredReaders.begin(); it != mMonitoredReaders.end();) {
        if (it->second.configName == configName) {
            // Remove old FD mapping
            int oldFD = it->first;
            mMonitoredReaders.erase(it);

            // Add reader back to epoll and get new FD
            int newFD = currentReader->AddToEpollAndGetFD(mEpollFD);
            if (newFD < 0) {
                LOG_WARNING(sLogger,
                            ("journal reader monitor failed to re-add reader to epoll after refresh", "")("config",
                                                                                                                   configName));
                return;
            }

            // Create new monitored reader entry with new FD
            MonitoredReader monitoredReader;
            monitoredReader.reader = currentReader;
            monitoredReader.configName = configName;
            // Note: We lose the previous pending data state, but this is acceptable as refresh is a clean restart
            mMonitoredReaders[newFD] = monitoredReader;

            LOG_DEBUG(sLogger,
                      ("journal reader monitor updated reader FD mapping after refresh", "")("config",
                                                                                                    configName)("old_fd",
                                                                                                                      oldFD)("new_fd", newFD));
            return;
        } else {
            ++it;
        }
    }

    // If not found in monitored readers, it will be handled by addReadersToEpollMonitoring()
}

void JournalEpollMonitor::addReadersToEpollMonitoring() {
    auto connectionManager = &JournalConnection::GetInstance();
    auto configNames = connectionManager->GetAllConfigNames();

    for (const auto& configName : configNames) {
        auto reader = connectionManager->GetConnection(configName);
        if (!reader || !reader->IsOpen()) {
            continue; // Skip closed connections
        }

        // Check if already monitored
        bool alreadyMonitored = false;
        for (const auto& pair : mMonitoredReaders) {
            if (pair.second.reader == reader) {
                alreadyMonitored = true;
                break;
            }
        }

        if (!alreadyMonitored) {
            int journalFD = reader->AddToEpollAndGetFD(mEpollFD);
            if (journalFD < 0) {
                LOG_WARNING(sLogger,
                            ("journal reader monitor failed to add reader to epoll", "")("config", configName)("epoll_fd", mEpollFD));
                continue;
            }

            MonitoredReader monitoredReader;
            monitoredReader.reader = reader;
            monitoredReader.configName = configName;
            mMonitoredReaders[journalFD] = monitoredReader;

            LOG_INFO(sLogger,
                     ("journal reader monitor reader added to epoll monitoring", "")("config", configName)("fd", journalFD));
        }
    }
}

bool JournalEpollMonitor::GetValidatedCurrentReader(MonitoredReader& monitoredReader,
                                                      std::shared_ptr<JournalReader>& currentReaderOut) const {
    // Validate reader to prevent using a closed reader during connection refresh
    if (!monitoredReader.reader) {
        return false;
    }

    if (!monitoredReader.reader->IsOpen()) {
        ClearPendingDataForInvalidReader(monitoredReader);
        return false;
    }

    auto connectionManager = &JournalConnection::GetInstance();
    auto currentReader = connectionManager->GetConnection(monitoredReader.configName);

    if (!currentReader || currentReader != monitoredReader.reader) {
        LOG_DEBUG(sLogger,
                  ("journal reader monitor reader changed during processing, skipping",
                   "will sync on next iteration")("config", monitoredReader.configName));
        ClearPendingDataForInvalidReader(monitoredReader);
        return false;
    }

    if (!currentReader->IsOpen()) {
        ClearPendingDataForInvalidReader(monitoredReader);
        return false;
    }

    currentReaderOut = currentReader;
    return true;
}

void JournalEpollMonitor::ClearPendingDataForInvalidReader(MonitoredReader& monitoredReader) const {
    // Clear pending data and accumulated state when reader validation fails (unavailable, closed, or replaced)
    if (monitoredReader.hasPendingData) {
        monitoredReader.hasPendingData = false;
        monitoredReader.accumulatedEventGroup = nullptr;
        monitoredReader.accumulatedEntryCount = 0;
        monitoredReader.accumulatedFirstCursor.clear();
    }
}

bool JournalEpollMonitor::IsBatchTimeoutExceeded(const MonitoredReader& monitoredReader, int batchTimeoutMs) const {
    if (monitoredReader.accumulatedEventGroup == nullptr || monitoredReader.accumulatedEntryCount == 0) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - monitoredReader.lastBatchTime).count();
    bool timeoutTrigger = elapsed >= batchTimeoutMs;

    if (timeoutTrigger) {
        LOG_DEBUG(sLogger,
                  ("journal reader monitor forcing flush accumulated batch due to timeout",
                   "")("config", monitoredReader.configName)("elapsed_ms", elapsed)(
                      "batch_timeout_ms", batchTimeoutMs)("accumulated_count",
                                                                  monitoredReader.accumulatedEntryCount));
    }

    return timeoutTrigger;
}

} // namespace logtail


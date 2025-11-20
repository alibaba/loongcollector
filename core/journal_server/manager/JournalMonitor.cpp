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

#include "JournalMonitor.h"

#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <set>

#include "../reader/JournalReader.h"
#include "JournalConnection.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail {

JournalMonitor::JournalMonitor() = default;

JournalMonitor::~JournalMonitor() {
    Cleanup();
}

bool JournalMonitor::Initialize() {
    if (mInitialized.load()) {
        return true;
    }

#ifdef __linux__
    {
        std::lock_guard<std::mutex> lock(mEpollMutex);
        mEpollFD = epoll_create1(EPOLL_CLOEXEC);
        if (mEpollFD == -1) {
            LOG_ERROR(sLogger,
                      ("journal monitor failed to create epoll", "")("error", strerror(errno))("errno", errno));
            return false;
        }
    }

    LOG_INFO(sLogger, ("journal monitor initialized", "")("epoll_fd", mEpollFD));
    mInitialized.store(true);
    return true;
#else
    return false;
#endif
}

void JournalMonitor::Cleanup() {
    if (!mInitialized.load()) {
        return;
    }

#ifdef __linux__
    LOG_INFO(sLogger,
             ("journal monitor cleaning up epoll monitoring", "")("monitored_readers", mMonitoredReaders.size()));

    // First, close all readers that have been marked as closing
    // This ensures lazy cleanup happens even if main loop has stopped
    CleanupClosedReaders();

    // Then close and remove all remaining readers
    for (auto& pair : mMonitoredReaders) {
        if (pair.second.reader && pair.second.reader->IsOpen()) {
            pair.second.reader->RemoveFromEpoll(mEpollFD);
            pair.second.reader->Close();
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
    LOG_INFO(sLogger, ("journal monitor cleaned up", ""));
#endif
}

int JournalMonitor::GetEpollFD() const {
    std::lock_guard<std::mutex> lock(mEpollMutex);
    return mEpollFD;
}

std::map<int, MonitoredReader>& JournalMonitor::GetMonitoredReaders() {
    return mMonitoredReaders;
}


int JournalMonitor::addReaderToMonitoring(const std::shared_ptr<JournalReader>& reader, const std::string& configName) {
    if (!reader || !reader->IsOpen()) {
        return -1;
    }

    int journalFD = reader->AddToEpollAndGetFD(mEpollFD);
    if (journalFD < 0) {
        LOG_WARNING(sLogger,
                    ("journal monitor failed to add reader to epoll", "")("config", configName)("epoll_fd", mEpollFD));
        return -1;
    }

    MonitoredReader& monitoredReader = mMonitoredReaders[journalFD];
    monitoredReader.reader = reader;
    monitoredReader.configName = configName;
    monitoredReader.isClosing.store(false);

    return journalFD;
}

void JournalMonitor::AddReadersToMonitoring(const std::vector<std::string>& configNames) {
    auto& connectionManager = JournalConnection::GetInstance();

    for (const auto& configName : configNames) {
        auto reader = connectionManager.GetConnection(configName);
        if (!reader || !reader->IsOpen()) {
            continue;
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
            int journalFD = addReaderToMonitoring(reader, configName);
            if (journalFD >= 0) {
                LOG_INFO(
                    sLogger,
                    ("journal monitor reader added to epoll monitoring", "")("config", configName)("fd", journalFD));
            }
        }
    }
}

void JournalMonitor::RemoveReaderFromMonitoring(const std::string& configName, bool removeFromMap) {
    for (auto it = mMonitoredReaders.begin(); it != mMonitoredReaders.end();) {
        if (it->second.configName == configName) {
            it->second.isClosing.store(true);
            // Only remove from epoll, don't close immediately
            // Close() will be called in CleanupClosedReaders() after all ongoing operations complete
            if (it->second.reader && it->second.reader->IsOpen()) {
                it->second.reader->RemoveFromEpoll(mEpollFD);
            }

            if (removeFromMap) {
                // Marked as closing, will be removed in next loop iteration
                LOG_DEBUG(sLogger,
                          ("journal monitor reader marked for closing, will be removed in next loop",
                           "")("config", configName)("fd", it->first));
            } else {
            }
            return;
        }
        ++it;
    }
}

void JournalMonitor::RefreshReaderFDMapping(const std::string& configName) {
    auto& connectionManager = JournalConnection::GetInstance();
    auto currentReader = connectionManager.GetConnection(configName);

    if (!currentReader || !currentReader->IsOpen()) {
        LOG_WARNING(sLogger, ("journal monitor reader not available after refresh", "")("config", configName));
        return;
    }

    // Save accumulated data before removing old FD mapping
    bool savedHasPendingData = false;
    std::shared_ptr<PipelineEventGroup> savedAccumulatedEventGroup = nullptr;
    int savedAccumulatedEntryCount = 0;
    std::string savedAccumulatedFirstCursor;
    std::chrono::steady_clock::time_point savedLastBatchTime;

    int oldFD = -1;
    for (auto it = mMonitoredReaders.begin(); it != mMonitoredReaders.end(); ++it) {
        if (it->second.configName == configName) {
            oldFD = it->first;
            // Mark old reader as closing to prevent Use-After-Close
            it->second.isClosing.store(true);
            if (it->second.reader && it->second.reader->IsOpen()) {
                it->second.reader->RemoveFromEpoll(mEpollFD);
            }
            // Save accumulated data before the old reader is removed
            savedHasPendingData = it->second.hasPendingData;
            savedAccumulatedEventGroup = it->second.accumulatedEventGroup;
            savedAccumulatedEntryCount = it->second.accumulatedEntryCount;
            savedAccumulatedFirstCursor = it->second.accumulatedFirstCursor;
            savedLastBatchTime = it->second.lastBatchTime;
            break;
        }
    }

    // Add reader back to epoll monitoring
    int newFD = addReaderToMonitoring(currentReader, configName);
    if (newFD < 0) {
        LOG_WARNING(sLogger,
                    ("journal monitor failed to re-add reader to epoll after refresh", "")("config", configName));
        return;
    }

    // Restore accumulated data to the new MonitoredReader
    if (oldFD >= 0) {
        auto& newMonitoredReader = mMonitoredReaders[newFD];
        newMonitoredReader.hasPendingData = savedHasPendingData;
        newMonitoredReader.accumulatedEventGroup = savedAccumulatedEventGroup;
        newMonitoredReader.accumulatedEntryCount = savedAccumulatedEntryCount;
        newMonitoredReader.accumulatedFirstCursor = savedAccumulatedFirstCursor;
        newMonitoredReader.lastBatchTime = savedLastBatchTime;

        LOG_DEBUG(sLogger,
                  ("journal monitor updated reader FD mapping after refresh", "")("config", configName)(
                      "old_fd", oldFD)("new_fd", newFD)("preserved_pending_data", savedHasPendingData)(
                      "preserved_entry_count", savedAccumulatedEntryCount));
    } else {
        // If not found in monitored readers, it will be handled by AddReadersToMonitoring()
    }
}

bool JournalMonitor::GetValidatedCurrentReader(MonitoredReader& monitoredReader,
                                               std::shared_ptr<JournalReader>& currentReaderOut) const {
    // Validate reader to prevent using a closed reader during connection refresh
    if (!monitoredReader.reader) {
        return false;
    }

    if (!monitoredReader.reader->IsOpen()) {
        ClearPendingDataForInvalidReader(monitoredReader);
        return false;
    }

    auto& connectionManager = JournalConnection::GetInstance();
    auto currentReader = connectionManager.GetConnection(monitoredReader.configName);

    if (!currentReader || currentReader != monitoredReader.reader) {
        LOG_DEBUG(sLogger,
                  ("journal monitor reader changed during processing, skipping",
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

void JournalMonitor::ClearPendingDataForInvalidReader(MonitoredReader& monitoredReader) const {
    // Clear pending data and accumulated state when reader validation fails (unavailable, closed, or replaced)
    if (monitoredReader.hasPendingData) {
        monitoredReader.hasPendingData = false;
        monitoredReader.accumulatedEventGroup = nullptr;
        monitoredReader.accumulatedEntryCount = 0;
        monitoredReader.accumulatedFirstCursor.clear();
    }
}

bool JournalMonitor::IsBatchTimeoutExceeded(const MonitoredReader& monitoredReader, int batchTimeoutMs) const {
    if (monitoredReader.accumulatedEventGroup == nullptr || monitoredReader.accumulatedEntryCount == 0) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - monitoredReader.lastBatchTime).count();
    bool timeoutTrigger = elapsed >= batchTimeoutMs;

    if (timeoutTrigger) {
        LOG_DEBUG(sLogger,
                  ("journal monitor forcing flush accumulated batch due to timeout",
                   "")("config", monitoredReader.configName)("elapsed_ms", elapsed)("batch_timeout_ms", batchTimeoutMs)(
                      "accumulated_count", monitoredReader.accumulatedEntryCount));
    }

    return timeoutTrigger;
}

void JournalMonitor::CleanupClosedReaders() {
    // Close and remove readers that have been marked as closing
    for (auto it = mMonitoredReaders.begin(); it != mMonitoredReaders.end();) {
        if (it->second.isClosing.load()) {
            // Now it's safe to close the reader, as all ongoing operations should have completed
            if (it->second.reader && it->second.reader->IsOpen()) {
                it->second.reader->Close();
            }
            LOG_DEBUG(sLogger,
                      ("journal monitor removing closed reader from map", "")("config",
                                                                              it->second.configName)("fd", it->first));
            it = mMonitoredReaders.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace logtail

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


int JournalMonitor::AddReaderToMonitoring(const std::shared_ptr<JournalReader>& reader, const std::string& configName) {
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
            int journalFD = AddReaderToMonitoring(reader, configName);
            if (journalFD >= 0) {
                LOG_INFO(
                    sLogger,
                    ("journal monitor reader added to epoll monitoring", "")("config", configName)("fd", journalFD));
            }
        }
    }
}

void JournalMonitor::MarkReaderAsClosing(const std::string& configName) {
    for (auto it = mMonitoredReaders.begin(); it != mMonitoredReaders.end(); ++it) {
        if (it->second.configName == configName) {
            // Mark as closing FIRST (prevents new operations from using this reader)
            it->second.isClosing.store(true);
            LOG_DEBUG(sLogger,
                      ("journal monitor reader marked as closing, will be removed in CleanupClosedReaders",
                       "")("config", configName)("fd", it->first));
            return;
        }
    }
}

void JournalMonitor::RemoveReaderFromMonitoring(const std::string& configName) {
    for (auto it = mMonitoredReaders.begin(); it != mMonitoredReaders.end(); ++it) {
        if (it->second.configName == configName) {
            // Remove from epoll (prevents new events)
            if (it->second.reader && it->second.reader->IsOpen()) {
                it->second.reader->RemoveFromEpoll(mEpollFD);
            }
            LOG_DEBUG(sLogger,
                      ("journal monitor reader removed from epoll", "")("config", configName)("fd", it->first));
            return;
        }
    }
}

bool JournalMonitor::SaveAccumulatedData(const std::string& configName,
                                         bool& savedHasPendingData,
                                         std::shared_ptr<PipelineEventGroup>& savedAccumulatedEventGroup,
                                         int& savedAccumulatedEntryCount,
                                         std::string& savedAccumulatedFirstCursor,
                                         std::chrono::steady_clock::time_point& savedLastBatchTime) {
    for (auto it = mMonitoredReaders.begin(); it != mMonitoredReaders.end(); ++it) {
        if (it->second.configName == configName) {
            // Save accumulated data
            savedHasPendingData = it->second.hasPendingData;
            savedAccumulatedEventGroup = it->second.accumulatedEventGroup;
            savedAccumulatedEntryCount = it->second.accumulatedEntryCount;
            savedAccumulatedFirstCursor = it->second.accumulatedFirstCursor;
            savedLastBatchTime = it->second.lastBatchTime;
            return true;
        }
    }
    return false;
}

void JournalMonitor::RestoreAccumulatedData(const std::string& configName,
                                            const std::shared_ptr<JournalReader>& reader,
                                            bool savedHasPendingData,
                                            const std::shared_ptr<PipelineEventGroup>& savedAccumulatedEventGroup,
                                            int savedAccumulatedEntryCount,
                                            const std::string& savedAccumulatedFirstCursor,
                                            const std::chrono::steady_clock::time_point& savedLastBatchTime) {
    // Find the MonitoredReader entry for this reader
    for (auto& pair : mMonitoredReaders) {
        if (pair.second.configName == configName && pair.second.reader == reader && !pair.second.isClosing.load()) {
            auto& monitoredReader = pair.second;
            monitoredReader.hasPendingData = savedHasPendingData;
            monitoredReader.accumulatedEventGroup = savedAccumulatedEventGroup;
            monitoredReader.accumulatedEntryCount = savedAccumulatedEntryCount;
            monitoredReader.accumulatedFirstCursor = savedAccumulatedFirstCursor;
            monitoredReader.lastBatchTime = savedLastBatchTime;
            LOG_DEBUG(sLogger,
                      ("journal monitor restored accumulated data", "")("config", configName)("fd", pair.first)(
                          "preserved_pending_data", savedHasPendingData)("preserved_entry_count",
                                                                         savedAccumulatedEntryCount));
            return;
        }
    }
}

void JournalMonitor::RefreshReaderFDMapping(const std::string& configName) {
    auto& connectionManager = JournalConnection::GetInstance();
    auto currentReader = connectionManager.GetConnection(configName);

    if (!currentReader || !currentReader->IsOpen()) {
        LOG_WARNING(sLogger, ("journal monitor reader not available after refresh", "")("config", configName));
        return;
    }

    // Mark reader as closing and remove from epoll
    MarkReaderAsClosing(configName);
    RemoveReaderFromMonitoring(configName);
    // Save accumulated data
    bool savedHasPendingData = false;
    std::shared_ptr<PipelineEventGroup> savedAccumulatedEventGroup = nullptr;
    int savedAccumulatedEntryCount = 0;
    std::string savedAccumulatedFirstCursor;
    std::chrono::steady_clock::time_point savedLastBatchTime;

    bool hadOldReader = SaveAccumulatedData(configName,
                                            savedHasPendingData,
                                            savedAccumulatedEventGroup,
                                            savedAccumulatedEntryCount,
                                            savedAccumulatedFirstCursor,
                                            savedLastBatchTime);

    // Add reader back to epoll monitoring
    int newFD = AddReaderToMonitoring(currentReader, configName);
    if (newFD < 0) {
        LOG_WARNING(sLogger,
                    ("journal monitor failed to re-add reader to epoll after refresh", "")("config", configName));
        return;
    }

    // Restore accumulated data to the new MonitoredReader
    if (hadOldReader) {
        RestoreAccumulatedData(configName,
                               currentReader,
                               savedHasPendingData,
                               savedAccumulatedEventGroup,
                               savedAccumulatedEntryCount,
                               savedAccumulatedFirstCursor,
                               savedLastBatchTime);
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

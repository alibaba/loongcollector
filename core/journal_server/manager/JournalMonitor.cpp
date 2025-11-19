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


int JournalMonitor::addReaderToMonitoring(std::shared_ptr<JournalReader> reader, const std::string& configName) {
    if (!reader || !reader->IsOpen()) {
        return -1;
    }

    int journalFD = reader->AddToEpollAndGetFD(mEpollFD);
    if (journalFD < 0) {
        LOG_WARNING(sLogger,
                    ("journal monitor failed to add reader to epoll", "")("config", configName)("epoll_fd", mEpollFD));
        return -1;
    }

    MonitoredReader monitoredReader;
    monitoredReader.reader = reader;
    monitoredReader.configName = configName;
    mMonitoredReaders[journalFD] = monitoredReader;

    return journalFD;
}

void JournalMonitor::AddReadersToMonitoring(const std::vector<std::string>& configNames) {
    auto& connectionManager = JournalConnection::GetInstance();

    for (const auto& configName : configNames) {
        auto reader = connectionManager.GetConnection(configName);
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
            // Remove from epoll if reader is still open
            if (it->second.reader && it->second.reader->IsOpen()) {
                it->second.reader->RemoveFromEpoll(mEpollFD);
            }

            // Remove from map if requested
            if (removeFromMap) {
                it = mMonitoredReaders.erase(it);
            } else {
                return;
            }
        } else {
            ++it;
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

    // Remove old FD mapping if exists
    int oldFD = -1;
    for (auto it = mMonitoredReaders.begin(); it != mMonitoredReaders.end();) {
        if (it->second.configName == configName) {
            oldFD = it->first;
            mMonitoredReaders.erase(it);
            break;
        }
        ++it;
    }

    // Add reader back to epoll monitoring
    int newFD = addReaderToMonitoring(currentReader, configName);
    if (newFD < 0) {
        LOG_WARNING(sLogger,
                    ("journal monitor failed to re-add reader to epoll after refresh", "")("config", configName));
        return;
    }

    if (oldFD >= 0) {
        LOG_DEBUG(sLogger,
                  ("journal monitor updated reader FD mapping after refresh",
                   "")("config", configName)("old_fd", oldFD)("new_fd", newFD));
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

} // namespace logtail

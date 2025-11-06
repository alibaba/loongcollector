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

#include <atomic>
#include <map>
#include <memory>
#include <mutex>

#include "../common/JournalCommon.h"
#include "../common/JournalConfig.h"
#include "logger/Logger.h"

namespace logtail {

class JournalEpollMonitor {
public:
    JournalEpollMonitor();
    ~JournalEpollMonitor();

    JournalEpollMonitor(const JournalEpollMonitor&) = delete;
    JournalEpollMonitor& operator=(const JournalEpollMonitor&) = delete;
    JournalEpollMonitor(JournalEpollMonitor&&) = delete;
    JournalEpollMonitor& operator=(JournalEpollMonitor&&) = delete;

    bool Initialize();
    void Cleanup();
    int GetEpollFD() const;
    std::map<int, MonitoredReader>& GetMonitoredReaders();
    void SyncMonitoredReaders();
    void RemoveReaderFromEpoll(const std::string& configName);
    bool GetValidatedCurrentReader(MonitoredReader& monitoredReader,
                                   std::shared_ptr<JournalReader>& currentReaderOut) const;

    void ClearPendingDataForInvalidReader(MonitoredReader& monitoredReader) const;
    bool IsBatchTimeoutExceeded(const MonitoredReader& monitoredReader, int batchTimeoutMs) const;

private:

    void lazyCleanupRemovedReadersFromEpoll();
    int cleanupRemovedReadersFromEpoll();
    void refreshReaderConnectionsByInterval();
    void addReadersToEpollMonitoring();
    void syncReaderFDMappingAfterRefresh(const std::string& configName);
    
    int mEpollFD{-1};
    mutable std::mutex mEpollMutex;
    std::map<int, MonitoredReader> mMonitoredReaders;
    std::atomic<bool> mInitialized{false};
};

} // namespace logtail


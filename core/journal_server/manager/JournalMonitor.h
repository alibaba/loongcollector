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
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "logger/Logger.h"

namespace logtail {

// Forward declarations
class JournalReader;
class PipelineEventGroup;

struct MonitoredReader {
    std::shared_ptr<JournalReader> reader;
    std::string configName;
    bool hasPendingData{true};
    std::shared_ptr<PipelineEventGroup> accumulatedEventGroup{nullptr};
    std::chrono::steady_clock::time_point lastBatchTime;
    int accumulatedEntryCount{0};
    std::string accumulatedFirstCursor;
    // Atomic flag to mark reader as closing, preventing Use-After-Close issues
    // When set to true, the reader should not be used for new operations
    // but can still be safely accessed by ongoing operations
    std::atomic<bool> isClosing{false};
};

class JournalMonitor {
public:
    JournalMonitor();
    ~JournalMonitor();

    JournalMonitor(const JournalMonitor&) = delete;
    JournalMonitor& operator=(const JournalMonitor&) = delete;
    JournalMonitor(JournalMonitor&&) = delete;
    JournalMonitor& operator=(JournalMonitor&&) = delete;

    bool Initialize();
    void Cleanup();
    int GetEpollFD() const;
    std::map<int, MonitoredReader>& GetMonitoredReaders();
    void AddReadersToMonitoring(const std::vector<std::string>& configNames);
    void RemoveReaderFromMonitoring(const std::string& configName, bool removeFromMap = false);
    void RefreshReaderFDMapping(const std::string& configName);
    bool GetValidatedCurrentReader(MonitoredReader& monitoredReader,
                                   std::shared_ptr<JournalReader>& currentReaderOut) const;

    void ClearPendingDataForInvalidReader(MonitoredReader& monitoredReader) const;
    bool IsBatchTimeoutExceeded(const MonitoredReader& monitoredReader, int batchTimeoutMs) const;
    void CleanupClosedReaders();

private:
    // Returns the FD if successful, -1 otherwise
    int addReaderToMonitoring(const std::shared_ptr<JournalReader>& reader, const std::string& configName);

    int mEpollFD{-1};
    mutable std::mutex mEpollMutex;
    std::map<int, MonitoredReader> mMonitoredReaders;
    std::atomic<bool> mInitialized{false};
};

} // namespace logtail

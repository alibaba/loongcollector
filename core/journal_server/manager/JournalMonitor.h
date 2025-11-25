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
    std::atomic<bool> isClosing{false};
};

class JournalMonitor {
public:
    static JournalMonitor* GetInstance() {
        static JournalMonitor sInstance;
        return &sInstance;
    }

    JournalMonitor(const JournalMonitor&) = delete;
    JournalMonitor& operator=(const JournalMonitor&) = delete;
    JournalMonitor(JournalMonitor&&) = delete;
    JournalMonitor& operator=(JournalMonitor&&) = delete;

    bool Initialize();
    void Cleanup();
    int GetEpollFD() const;
    std::map<int, MonitoredReader>& GetMonitoredReaders();
    void CleanupClosedReaders();
    void AddReadersToMonitoring(const std::vector<std::string>& configNames);
    int AddReaderToMonitoring(const std::shared_ptr<JournalReader>& reader, const std::string& configName);
    void MarkReaderAsClosing(const std::string& configName);
    void RemoveReaderFromMonitoring(const std::string& configName);
    bool SaveAccumulatedData(const std::string& configName,
                             bool& savedHasPendingData,
                             std::shared_ptr<PipelineEventGroup>& savedAccumulatedEventGroup,
                             int& savedAccumulatedEntryCount,
                             std::string& savedAccumulatedFirstCursor,
                             std::chrono::steady_clock::time_point& savedLastBatchTime);
    void RestoreAccumulatedData(const std::string& configName,
                                const std::shared_ptr<JournalReader>& reader,
                                bool savedHasPendingData,
                                const std::shared_ptr<PipelineEventGroup>& savedAccumulatedEventGroup,
                                int savedAccumulatedEntryCount,
                                const std::string& savedAccumulatedFirstCursor,
                                const std::chrono::steady_clock::time_point& savedLastBatchTime);
    bool GetValidatedCurrentReader(MonitoredReader& monitoredReader,
                                   std::shared_ptr<JournalReader>& currentReaderOut) const;

    void ClearPendingDataForInvalidReader(MonitoredReader& monitoredReader) const;
    bool IsBatchTimeoutExceeded(const MonitoredReader& monitoredReader, int batchTimeoutMs) const;

private:
    JournalMonitor() = default;
    ~JournalMonitor();

    int mEpollFD{-1};
    mutable std::mutex mEpollMutex;
    std::map<int, MonitoredReader> mMonitoredReaders;
    std::atomic<bool> mInitialized{false};
};

} // namespace logtail

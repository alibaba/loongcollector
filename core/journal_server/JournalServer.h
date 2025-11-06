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
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "common/JournalCommon.h"
#include "common/JournalConfig.h"
#include "models/PipelineEventGroup.h"
#include "runner/InputRunner.h"

namespace logtail {

// Forward declarations
class JournalConnection;
class JournalConnectionInstance;
class JournalReader;
class PipelineEventGroup;
struct JournalEntry;

// Epoll timeout constants
inline constexpr int kJournalEpollTimeoutMS = 200;

class JournalServer : public InputRunner {
public:
    // ============================================================================
    // Constructor/Destructor/Singleton
    // ============================================================================
    JournalServer(const JournalServer&) = delete;
    JournalServer& operator=(const JournalServer&) = delete;
    JournalServer(JournalServer&&) = delete;
    JournalServer& operator=(JournalServer&&) = delete;

    static JournalServer* GetInstance() {
        static JournalServer sInstance;
        return &sInstance;
    }

    ~JournalServer() = default;

    // Lifecycle Management (InputRunner interface)
    void Init() override;
    void Stop() override;
    bool HasRegisteredPlugins() const override;

    // Configuration Management
    void AddJournalInput(const std::string& configName, const JournalConfig& config);
    void RemoveJournalInput(const std::string& configName);
    void RemoveConfigOnly(const std::string& configName);

    // Query Interfaces
    std::map<std::string, JournalConfig> GetAllJournalConfigs() const;
    int GetGlobalEpollFD() const;

    // Test Support
#ifdef APSARA_UNIT_TEST_MAIN
    void Clear();
#endif

private:
    JournalServer() = default;

    // Main Execution Flow
    void run();

    // Handle Pending Data
    bool processPendingDataReaders(std::map<int, MonitoredReader>& monitoredReaders);
    void clearPendingDataForInvalidReader(MonitoredReader& monitoredReader) const;

    // Refresh Connection
    void refreshReaderConnectionsByInterval(int epollFD, std::map<int, MonitoredReader>& monitoredReaders);
    void addReadersToEpollMonitoring(int epollFD, std::map<int, MonitoredReader>& monitoredReaders);
    void syncReaderFDMappingAfterRefresh(const std::string& configName, std::map<int, MonitoredReader>& monitoredReaders);

    // Monitor Management
    void syncMonitoredReaders(int epollFD, std::map<int, MonitoredReader>& monitoredReaders);
    void lazyCleanupRemovedReadersFromEpoll(int epollFD, std::map<int, MonitoredReader>& monitoredReaders);

    // Validation/Helper Methods
    bool getValidatedCurrentReader(MonitoredReader& monitoredReader,
                                   std::shared_ptr<JournalReader>& currentReaderOut) const;
    bool getOrValidateQueueKey(const std::string& configName, const JournalConfig& config, QueueKey& queueKey);
    bool isBatchTimeoutExceeded(const MonitoredReader& monitoredReader, const JournalConfig& config) const;

    std::future<void> mThreadRes;
    std::atomic<bool> mIsThreadRunning{true};

    int mGlobalEpollFD{-1};
    mutable std::mutex mEpollMutex;

    std::atomic<bool> mIsInitialized{false};
};

} // namespace logtail

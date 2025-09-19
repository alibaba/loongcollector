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

#include <condition_variable>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "runner/InputRunner.h"
#include "common/JournalConfig.h"
#include "models/LogEvent.h"

namespace logtail {

// Forward declarations
class JournalConnectionManager;
class SystemdJournalReader;
class JournalConnectionGuard;
class PipelineEventGroup;
struct JournalEntry;

/**
 * @brief JournalServer manages all journal input plugins
 * 
 * It follows the same pattern as FileServer and StaticFileServer:
 * 1. Manages journal configurations from registered plugins
 * 2. Runs in a separate thread to process journal entries
 * 3. Sends data to processing queues
 * 4. Provides registration/unregistration interface for plugins
 */
class JournalServer : public InputRunner {
public:
    JournalServer(const JournalServer&) = delete;
    JournalServer& operator=(const JournalServer&) = delete;
    JournalServer(JournalServer&&) = delete;
    JournalServer& operator=(JournalServer&&) = delete;

    static JournalServer* GetInstance() {
        static JournalServer sInstance;
        return &sInstance;
    }

    ~JournalServer() = default;

    // InputRunner interface implementation
    void Init() override;
    void Stop() override;
    bool HasRegisteredPlugins() const override;
    
    // Plugin registration interface
    void AddJournalInput(const std::string& configName,
                        size_t idx,
                        const JournalConfig& config);
    void RemoveJournalInput(const std::string& configName, size_t idx);

    // Configuration management
    JournalConfig GetJournalConfig(const std::string& name, size_t idx) const;
    const std::unordered_map<std::string, std::map<size_t, JournalConfig>>& GetAllJournalConfigs() const {
        return mPipelineNameJournalConfigsMap;
    }

    // Checkpoint management moved to JournalConnectionManager

#ifdef APSARA_UNIT_TEST_MAIN
    void Clear();
#endif

private:
    JournalServer() = default;

    void run();
    void processJournalEntries();
    void processJournalConfig(const std::string& configName, size_t idx, JournalConfig& config);
    
    // Helper functions for processJournalConfig to reduce cognitive complexity
    bool validateJournalConfig(const std::string& configName, size_t idx, const JournalConfig& config, QueueKey& queueKey);
    std::shared_ptr<SystemdJournalReader> setupJournalConnection(const std::string& configName, size_t idx, const JournalConfig& config, std::unique_ptr<JournalConnectionGuard>& connectionGuard, bool& isNewConnection);
    bool performJournalSeek(const std::string& configName, size_t idx, JournalConfig& config, std::shared_ptr<SystemdJournalReader> journalReader, bool forceSeek = false);
    void readJournalEntriesForConfig(const std::string& configName, size_t idx, const JournalConfig& config, const std::shared_ptr<SystemdJournalReader>& journalReader, QueueKey queueKey);
    
    // Helper functions for readJournalEntriesForConfig to reduce cognitive complexity
    bool moveToNextJournalEntry(const std::string& configName, size_t idx, const JournalConfig& config, const std::shared_ptr<SystemdJournalReader>& journalReader, bool isFirstEntry, int entryCount);
    bool readAndValidateEntry(const std::string& configName, size_t idx, const std::shared_ptr<SystemdJournalReader>& journalReader, JournalEntry& entry);
    bool createAndPushEventGroup(const std::string& configName, size_t idx, const JournalConfig& config, const JournalEntry& entry, QueueKey queueKey);
    
    // Low-level helper functions
    bool handleJournalWait(const std::string& configName, size_t idx, const JournalConfig& config, const std::shared_ptr<SystemdJournalReader>& journalReader, int entryCount);
    LogEvent* createLogEventFromJournal(const JournalEntry& entry, const JournalConfig& config, PipelineEventGroup& eventGroup);

    std::future<void> mThreadRes;
    mutable std::mutex mThreadRunningMux;
    bool mIsThreadRunning = true;
    mutable std::condition_variable mStopCV;

    time_t mStartTime = 0;

    // Configuration storage - accessed by main thread and journal runner thread
    mutable std::mutex mUpdateMux;
    std::unordered_map<std::string, std::map<size_t, JournalConfig>> mPipelineNameJournalConfigsMap;
};

} // namespace logtail 
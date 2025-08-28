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
#include <thread>
#include <unordered_map>

#include "collection_pipeline/CollectionPipelineContext.h"
#include "runner/InputRunner.h"

namespace logtail {

// Forward declarations
class JournalConnectionManager;
struct JournalConfig;
class PipelineEventGroup;

/**
 * @brief Configuration for journal input plugin
 */
struct JournalConfig {
    // Journal reading configuration
    std::string seekPosition;
    int cursorFlushPeriodMs;
    std::string cursorSeekFallback;
    std::vector<std::string> units;
    bool kernel;
    std::vector<std::string> identifiers;
    std::vector<std::string> journalPaths;
    std::vector<std::string> matchPatterns;  // Custom match patterns
    int resetIntervalSecond;
    
    // Field parsing options (like Go version)
    bool parsePriority;          // Convert priority numbers to text
    bool parseSyslogFacility;    // Convert facility numbers to text  
    bool useJournalEventTime;    // Use journal timestamp vs current time
    
    // Pipeline context
    const CollectionPipelineContext* ctx;
    
    JournalConfig() : cursorFlushPeriodMs(5000), kernel(false), resetIntervalSecond(3600),
                     parsePriority(false), parseSyslogFacility(false), useJournalEventTime(true), ctx(nullptr) {}
};

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

    static JournalServer* GetInstance() {
        static JournalServer sInstance;
        return &sInstance;
    }

    // InputRunner interface implementation
    void Init() override;
    void Stop() override;
    bool HasRegisteredPlugins() const override;
    void ClearUnusedCheckpoints() override;

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
    ~JournalServer() = default;

    void Run();
    void ProcessJournalEntries();
    void ProcessJournalConfig(const std::string& configName, size_t idx, const JournalConfig& config);

    std::future<void> mThreadRes;
    mutable std::mutex mThreadRunningMux;
    bool mIsThreadRunning = true;
    mutable std::condition_variable mStopCV;

    time_t mStartTime = 0;
    bool mIsUnusedCheckpointsCleared = false;

    // Configuration storage - accessed by main thread and journal runner thread
    mutable std::mutex mUpdateMux;
    std::unordered_map<std::string, std::map<size_t, JournalConfig>> mPipelineNameJournalConfigsMap;
    
    // Added/removed inputs tracking
    std::multimap<std::string, size_t> mAddedInputs;
    std::set<std::pair<std::string, size_t>> mDeletedInputs;
};

} // namespace logtail 
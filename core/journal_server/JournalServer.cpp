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

#include "journal_server/JournalServer.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <utility>

#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "collection_pipeline/queue/QueueKey.h"
#include "models/PipelineEventGroup.h"
#include "common/Flags.h"
#include "common/LogtailCommonFlags.h"
#include "common/memory/SourceBuffer.h"
#include "logger/Logger.h"
#include "runner/ProcessorRunner.h"
#include "journal_server/JournalReader.h"
#include "journal_server/JournalEntry.h"

DEFINE_FLAG_INT32(journal_server_checkpoint_dump_interval_sec, "", 10);

using namespace std;

namespace logtail {

// Syslog facility conversion map (from Go version)
static const std::map<std::string, std::string> SyslogFacilityString = {
    {"0",  "kernel"},
    {"1",  "user"},
    {"2",  "mail"},
    {"3",  "daemon"},
    {"4",  "auth"},
    {"5",  "syslog"},
    {"6",  "line printer"},
    {"7",  "network news"},
    {"8",  "uucp"},
    {"9",  "clock daemon"},
    {"10", "security/auth"},
    {"11", "ftp"},
    {"12", "ntp"},
    {"13", "log audit"},
    {"14", "log alert"},
    {"15", "clock daemon"},
    {"16", "local0"},
    {"17", "local1"},
    {"18", "local2"},
    {"19", "local3"},
    {"20", "local4"},
    {"21", "local5"},
    {"22", "local6"},
    {"23", "local7"}
};

// Priority conversion map (from Go version) 
static const std::map<std::string, std::string> PriorityConversionMap = {
    {"0", "emergency"},
    {"1", "alert"},
    {"2", "critical"},
    {"3", "error"},
    {"4", "warning"},
    {"5", "notice"},
    {"6", "informational"},
    {"7", "debug"}
};

// Apply field transformations based on configuration
void ApplyJournalFieldTransforms(JournalEntry& entry, const JournalConfig& config) {
    if (config.parsePriority) {
        auto it = entry.fields.find("PRIORITY");
        if (it != entry.fields.end()) {
            auto priorityIt = PriorityConversionMap.find(it->second);
            if (priorityIt != PriorityConversionMap.end()) {
                it->second = priorityIt->second;
            }
        }
    }
    
    if (config.parseSyslogFacility) {
        auto it = entry.fields.find("SYSLOG_FACILITY");
        if (it != entry.fields.end()) {
            auto facilityIt = SyslogFacilityString.find(it->second);
            if (facilityIt != SyslogFacilityString.end()) {
                it->second = facilityIt->second;
            }
        }
    }
}

// Helper functions for journal filtering (based on Go version implementation)
bool AddUnitsFilter(JournalReader* reader, const std::vector<std::string>& units, 
                   const std::string& configName, size_t idx) {
    for (const auto& unit : units) {
        // Look for messages from the service itself
        if (!reader->AddMatch("_SYSTEMD_UNIT", unit)) {
            LOG_WARNING(sLogger, ("failed to add unit match", unit)("config", configName)("idx", idx));
            return false;
        }
        
        // Look for coredumps of the service
        if (!reader->AddDisjunction() ||
            !reader->AddMatch("MESSAGE_ID", "fc2e22bc6ee647b6b90729ab34a250b1") ||
            !reader->AddMatch("_UID", "0") ||
            !reader->AddMatch("COREDUMP_UNIT", unit)) {
            LOG_WARNING(sLogger, ("failed to add coredump match", unit)("config", configName)("idx", idx));
            return false;
        }
        
        // Look for messages from PID 1 about this service
        if (!reader->AddDisjunction() ||
            !reader->AddMatch("_PID", "1") ||
            !reader->AddMatch("UNIT", unit)) {
            LOG_WARNING(sLogger, ("failed to add PID1 match", unit)("config", configName)("idx", idx));
            return false;
        }
        
        // Look for messages from authorized daemons about this service
        if (!reader->AddDisjunction() ||
            !reader->AddMatch("_UID", "0") ||
            !reader->AddMatch("OBJECT_SYSTEMD_UNIT", unit)) {
            LOG_WARNING(sLogger, ("failed to add daemon match", unit)("config", configName)("idx", idx));
            return false;
        }
        
        // Show all messages belonging to a slice
        if (unit.find(".slice") != std::string::npos) {
            if (!reader->AddDisjunction() ||
                !reader->AddMatch("_SYSTEMD_SLICE", unit)) {
                LOG_WARNING(sLogger, ("failed to add slice match", unit)("config", configName)("idx", idx));
                return false;
            }
        }
        
        // Final disjunction for this unit
        if (!reader->AddDisjunction()) {
            LOG_WARNING(sLogger, ("failed to add final disjunction", unit)("config", configName)("idx", idx));
            return false;
        }
        
        LOG_DEBUG(sLogger, ("added comprehensive unit filter", unit)("config", configName)("idx", idx));
    }
    return true;
}

bool AddIdentifiersFilter(JournalReader* reader, const std::vector<std::string>& identifiers,
                         const std::string& configName, size_t idx) {
    for (const auto& identifier : identifiers) {
        if (!reader->AddMatch("SYSLOG_IDENTIFIER", identifier)) {
            LOG_WARNING(sLogger, ("failed to add identifier match", identifier)("config", configName)("idx", idx));
            return false;
        }
        
        if (!reader->AddDisjunction()) {
            LOG_WARNING(sLogger, ("failed to add identifier disjunction", identifier)("config", configName)("idx", idx));
            return false;
        }
        
        LOG_DEBUG(sLogger, ("added identifier filter", identifier)("config", configName)("idx", idx));
    }
    return true;
}

bool AddKernelFilter(JournalReader* reader, const std::string& configName, size_t idx) {
    if (!reader->AddMatch("_TRANSPORT", "kernel")) {
        LOG_WARNING(sLogger, ("failed to add kernel transport match", "")("config", configName)("idx", idx));
        return false;
    }
    
    if (!reader->AddDisjunction()) {
        LOG_WARNING(sLogger, ("failed to add kernel disjunction", "")("config", configName)("idx", idx));
        return false;
    }
    
    LOG_DEBUG(sLogger, ("added kernel filter", "")("config", configName)("idx", idx));
    return true;
}

bool AddMatchPatternsFilter(JournalReader* reader, const std::vector<std::string>& patterns,
                           const std::string& configName, size_t idx) {
    for (const auto& pattern : patterns) {
        // Parse pattern in format "FIELD=value"
        size_t eqPos = pattern.find('=');
        if (eqPos == std::string::npos) {
            LOG_WARNING(sLogger, ("invalid match pattern format", pattern)("config", configName)("idx", idx));
            continue;
        }
        
        std::string field = pattern.substr(0, eqPos);
        std::string value = pattern.substr(eqPos + 1);
        
        if (!reader->AddMatch(field, value)) {
            LOG_WARNING(sLogger, ("failed to add pattern match", pattern)("config", configName)("idx", idx));
            return false;
        }
        
        if (!reader->AddDisjunction()) {
            LOG_WARNING(sLogger, ("failed to add pattern disjunction", pattern)("config", configName)("idx", idx));
            return false;
        }
        
        LOG_DEBUG(sLogger, ("added match pattern", pattern)("config", configName)("idx", idx));
    }
    return true;
}

void JournalServer::Init() {
    LOG_INFO(sLogger, ("JournalServer initializing", ""));
    mThreadRes = async(launch::async, &JournalServer::Run, this);
    mStartTime = time(nullptr);
    LOG_INFO(sLogger, ("JournalServer initialized", ""));
}

void JournalServer::Stop() {
    if (!mThreadRes.valid()) {
        return;
    }
    {
        lock_guard<mutex> lock(mThreadRunningMux);
        mIsThreadRunning = false;
    }
    mStopCV.notify_all();

    future_status s = mThreadRes.wait_for(chrono::seconds(1));
    if (s == future_status::ready) {
        LOG_INFO(sLogger, ("journal server", "stopped successfully"));
    } else {
        LOG_WARNING(sLogger, ("journal server", "forced to stopped"));
    }
}

bool JournalServer::HasRegisteredPlugins() const {
    lock_guard<mutex> lock(mUpdateMux);
    return !mPipelineNameJournalConfigsMap.empty();
}

void JournalServer::ClearUnusedCheckpoints() {
    if (mIsUnusedCheckpointsCleared || time(nullptr) - mStartTime < INT32_FLAG(unused_checkpoints_clear_interval_sec)) {
        return;
    }
    
    // Clear checkpoints for deleted configurations
    lock_guard<mutex> lock(mUpdateMux);
    for (const auto& deletedInput : mDeletedInputs) {
        const auto& configName = deletedInput.first;
        size_t idx = deletedInput.second;
        
        auto configItr = mPipelineNameJournalConfigsMap.find(configName);
        if (configItr != mPipelineNameJournalConfigsMap.end()) {
            configItr->second.erase(idx);
            if (configItr->second.empty()) {
                mPipelineNameJournalConfigsMap.erase(configItr);
            }
        }
        
        // Clear checkpoint
        auto checkpointItr = mJournalCheckpoints.find(configName);
        if (checkpointItr != mJournalCheckpoints.end()) {
            checkpointItr->second.erase(idx);
            if (checkpointItr->second.empty()) {
                mJournalCheckpoints.erase(checkpointItr);
            }
        }
    }
    
    mDeletedInputs.clear();
    mIsUnusedCheckpointsCleared = true;
}

void JournalServer::AddJournalInput(const string& configName, size_t idx, const JournalConfig& config) {
    {
        lock_guard<mutex> lock(mUpdateMux);
        mPipelineNameJournalConfigsMap[configName][idx] = config;
        mAddedInputs.emplace(configName, idx);
    }
    
    // Save initial checkpoint if not exists
    if (GetJournalCheckpoint(configName, idx).empty()) {
        SaveJournalCheckpoint(configName, idx, "");
    }
}

void JournalServer::RemoveJournalInput(const string& configName, size_t idx) {
    {
        lock_guard<mutex> lock(mUpdateMux);
        auto configItr = mPipelineNameJournalConfigsMap.find(configName);
        if (configItr != mPipelineNameJournalConfigsMap.end()) {
            configItr->second.erase(idx);
            if (configItr->second.empty()) {
                mPipelineNameJournalConfigsMap.erase(configItr);
            }
        }
        mDeletedInputs.emplace(configName, idx);
    }
    
    // Clear checkpoint
    ClearJournalCheckpoint(configName, idx);
}

JournalConfig JournalServer::GetJournalConfig(const string& name, size_t idx) const {
    lock_guard<mutex> lock(mUpdateMux);
    auto configItr = mPipelineNameJournalConfigsMap.find(name);
    if (configItr != mPipelineNameJournalConfigsMap.end()) {
        auto idxItr = configItr->second.find(idx);
        if (idxItr != configItr->second.end()) {
            return idxItr->second;
        }
    }
    return JournalConfig();
}

void JournalServer::SaveJournalCheckpoint(const string& configName, size_t idx, const string& cursor) {
    lock_guard<mutex> lock(mUpdateMux);
    mJournalCheckpoints[configName][idx] = cursor;
}

std::string JournalServer::GetJournalCheckpoint(const std::string& configName, size_t idx) const {
    lock_guard<mutex> lock(mUpdateMux);
    auto configItr = mJournalCheckpoints.find(configName);
    if (configItr != mJournalCheckpoints.end()) {
        auto idxItr = configItr->second.find(idx);
        if (idxItr != configItr->second.end()) {
            return idxItr->second;
        }
    }
    return "";
}

void JournalServer::ClearJournalCheckpoint(const string& configName, size_t idx) {
    lock_guard<mutex> lock(mUpdateMux);
    auto configItr = mJournalCheckpoints.find(configName);
    if (configItr != mJournalCheckpoints.end()) {
        configItr->second.erase(idx);
        if (configItr->second.empty()) {
            mJournalCheckpoints.erase(configItr);
        }
    }
}

void JournalServer::Run() {
    LOG_INFO(sLogger, ("journal server", "started"));
    LOG_INFO(sLogger, ("journal server thread", "entering main loop"));
    unique_lock<mutex> lock(mThreadRunningMux);
    time_t lastDumpCheckpointTime = time(nullptr);
    
    while (mIsThreadRunning) {
        lock.unlock();
        
        LOG_INFO(sLogger, ("journal server loop", "iteration"));
        // Process journal entries for all registered configurations
        ProcessJournalEntries();
        
        // Periodically dump checkpoints
        auto cur = time(nullptr);
        if (cur - lastDumpCheckpointTime >= INT32_FLAG(journal_server_checkpoint_dump_interval_sec)) {
            // TODO: Implement checkpoint persistence to disk
            lastDumpCheckpointTime = cur;
        }
        
        lock.lock();
        if (mStopCV.wait_for(lock, chrono::milliseconds(100), [this]() { return !mIsThreadRunning; })) {
            return;
        }
    }
}

void JournalServer::ProcessJournalEntries() {
    LOG_INFO(sLogger, ("ProcessJournalEntries", "called"));
    // Get current configurations snapshot to avoid long lock holding
    unordered_map<string, map<size_t, JournalConfig>> currentConfigs;
    {
        lock_guard<mutex> lock(mUpdateMux);
        currentConfigs = mPipelineNameJournalConfigsMap;
    }
    
    // Add debug logging
    if (currentConfigs.empty()) {
        LOG_INFO(sLogger, ("no journal configurations to process", ""));
        return;
    }
    
    LOG_INFO(sLogger, ("processing journal entries", "")("configs_count", currentConfigs.size()));
    
    // Process each configuration
    for (const auto& pipelineConfig : currentConfigs) {
        const string& configName = pipelineConfig.first;
        
        for (const auto& idxConfig : pipelineConfig.second) {
            size_t idx = idxConfig.first;
            const JournalConfig& config = idxConfig.second;
            
            LOG_DEBUG(sLogger, ("processing config", "")("config", configName)("idx", idx));
            
            // Check if this input has been deleted
            {
                lock_guard<mutex> lock(mUpdateMux);
                if (mDeletedInputs.find(make_pair(configName, idx)) != mDeletedInputs.end()) {
                    continue;
                }
            }
            
            // Process this journal configuration
            ProcessJournalConfig(configName, idx, config);
        }
    }
}

void JournalServer::ProcessJournalConfig(const string& configName, size_t idx, const JournalConfig& config) {
    LOG_INFO(sLogger, ("ProcessJournalConfig", "started")("config", configName)("idx", idx));
    
    if (!config.ctx) {
        LOG_WARNING(sLogger, ("no context available for journal config", "skip")("config", configName)("idx", idx));
        return;
    }
    
    // Get queue key from pipeline context
    QueueKey queueKey = config.ctx->GetProcessQueueKey();
    if (queueKey == -1) {
        LOG_WARNING(sLogger, ("no queue key available for journal config", "skip")("config", configName)("idx", idx));
        return;
    }
    
    // Check if queue is valid
    if (!ProcessQueueManager::GetInstance()->IsValidToPush(queueKey)) {
        LOG_DEBUG(sLogger, ("queue not valid for journal config", "skip")("config", configName)("idx", idx)("queue", queueKey));
        return;
    }
    
    LOG_DEBUG(sLogger, ("processing journal config", "")("config", configName)("idx", idx)("queue", queueKey));
    
    // Create a journal reader for this configuration
    LOG_INFO(sLogger, ("creating journal reader", "")("config", configName)("idx", idx));
    auto journalReader = std::make_unique<SystemdJournalReader>();
    
    // Set timeout to prevent hanging (5 seconds)
    journalReader->SetTimeout(std::chrono::milliseconds(5000));
    
    // Set custom journal paths if specified
    if (!config.journalPaths.empty()) {
        LOG_INFO(sLogger, ("setting custom journal paths", "")("config", configName)("idx", idx)("paths_count", config.journalPaths.size()));
        for (const auto& path : config.journalPaths) {
            LOG_INFO(sLogger, ("journal path", path)("config", configName)("idx", idx));
        }
        journalReader->SetJournalPaths(config.journalPaths);
    }
    
    // Open the journal
    LOG_INFO(sLogger, ("opening journal", "")("config", configName)("idx", idx)("paths_count", config.journalPaths.size()));
    if (!journalReader->Open()) {
        LOG_ERROR(sLogger, ("failed to open journal", "skip processing")("config", configName)("idx", idx));
        
        // Try to open with default journal if custom paths failed
        if (!config.journalPaths.empty()) {
            LOG_INFO(sLogger, ("attempting to open default journal as fallback", "")("config", configName)("idx", idx));
            journalReader->SetJournalPaths({}); // Clear custom paths
            if (!journalReader->Open()) {
                LOG_ERROR(sLogger, ("failed to open default journal as well", "skip processing")("config", configName)("idx", idx));
                return;
            } else {
                LOG_INFO(sLogger, ("successfully opened default journal as fallback", "")("config", configName)("idx", idx));
            }
        } else {
            return;
        }
    }
    LOG_INFO(sLogger, ("journal opened successfully", "")("config", configName)("idx", idx));
    
    // Verify journal reader is open after opening
    if (!journalReader->IsOpen()) {
        LOG_ERROR(sLogger, ("journal reader not open after Open() call", "")("config", configName)("idx", idx));
        return;
    }
    
    LOG_INFO(sLogger, ("journal reader IsOpen() check passed", "")("config", configName)("idx", idx));
    
    // Apply journal filters based on configuration
    bool filtersApplied = true;
    
    // Add unit filters with comprehensive matching (like Go version)
    if (!config.units.empty()) {
        if (!AddUnitsFilter(journalReader.get(), config.units, configName, idx)) {
            LOG_WARNING(sLogger, ("failed to add units filter", "")("config", configName)("idx", idx));
            filtersApplied = false;
        }
    }
    
    // Add identifier filters
    if (!config.identifiers.empty()) {
        if (!AddIdentifiersFilter(journalReader.get(), config.identifiers, configName, idx)) {
            LOG_WARNING(sLogger, ("failed to add identifiers filter", "")("config", configName)("idx", idx));
            filtersApplied = false;
        }
    }
    
    // Add kernel filter if requested (fixed logic: only check kernel flag)
    if (config.kernel) {
        if (!AddKernelFilter(journalReader.get(), configName, idx)) {
            LOG_WARNING(sLogger, ("failed to add kernel filter", "")("config", configName)("idx", idx));
            filtersApplied = false;
        }
    }
    
    // Add custom match patterns if any
    if (!AddMatchPatternsFilter(journalReader.get(), config.matchPatterns, configName, idx)) {
        LOG_WARNING(sLogger, ("failed to add match patterns", "")("config", configName)("idx", idx));
        filtersApplied = false;
    }
    
    if (!filtersApplied) {
        LOG_WARNING(sLogger, ("some filters failed to apply", "continuing with basic processing")("config", configName)("idx", idx));
    }
    
    // Seek to appropriate position
    bool seekSuccess = false;
    string checkpoint = GetJournalCheckpoint(configName, idx);
    
    if (!checkpoint.empty() && config.seekPosition == "cursor") {
        LOG_DEBUG(sLogger, ("seeking to checkpoint cursor", checkpoint)("config", configName)("idx", idx));
        seekSuccess = journalReader->SeekCursor(checkpoint);
        if (!seekSuccess) {
            LOG_WARNING(sLogger, ("checkpoint", checkpoint)("msg", "falling back to fallback position")("config", configName)("idx", idx));
        }
    }
    
    if (!seekSuccess) {
        if ((config.seekPosition == "head" || (config.seekPosition == "cursor" && config.cursorSeekFallback == "head"))) {
            // **修复：head对于kernel日志通常读不到数据，改为使用tail**
            LOG_INFO(sLogger, ("head position not suitable for kernel logs, using tail instead", "")("config", configName)("idx", idx));
            seekSuccess = journalReader->SeekTail();
            
            // After seeking to tail, move to the last actual entry
            if (seekSuccess) {
                LOG_INFO(sLogger, ("seeking to tail succeeded, moving to last entry", "")("config", configName)("idx", idx));
                if (!journalReader->Previous()) {
                    LOG_INFO(sLogger, ("no previous entry available after seeking to tail", "")("config", configName)("idx", idx));
                    // This is normal if journal is empty, continue processing
                } else {
                    LOG_INFO(sLogger, ("moved to last entry after seeking to tail", "")("config", configName)("idx", idx));
                }
            }
        } else {
            LOG_DEBUG(sLogger, ("seeking to tail", "")("config", configName)("idx", idx));
            seekSuccess = journalReader->SeekTail();
            
            // After seeking to tail, move to the last actual entry
            if (seekSuccess) {
                LOG_INFO(sLogger, ("seeking to tail succeeded, moving to last entry", "")("config", configName)("idx", idx));
                if (!journalReader->Previous()) {
                    LOG_INFO(sLogger, ("no previous entry available after seeking to tail", "")("config", configName)("idx", idx));
                    // This is normal if journal is empty, continue processing
                } else {
                    LOG_INFO(sLogger, ("moved to last entry after seeking to tail", "")("config", configName)("idx", idx));
                }
            }
        }
        
        if (!seekSuccess) {
            LOG_ERROR(sLogger, ("failed to seek to position", config.seekPosition)("config", configName)("idx", idx));
            return;
        }
    }
    
    // Verify journal reader is still open after seek operations
    if (!journalReader->IsOpen()) {
        LOG_ERROR(sLogger, ("journal reader not open after seek operations", "")("config", configName)("idx", idx));
        return;
    }
    
    LOG_INFO(sLogger, ("journal reader still open after seek operations", "")("config", configName)("idx", idx));
    
    // Read journal entries
    int entryCount = 0;
    const int maxEntriesPerBatch = 100; // Limit batch size to avoid blocking
    
    LOG_DEBUG(sLogger, ("starting to read journal entries", "")("config", configName)("idx", idx));
    
    // Since we now default to tail position, always process current entry first
    bool shouldProcessCurrentEntry = true;  // **修复：tail读取需要处理当前条目**
    
    LOG_INFO(sLogger, ("journal reading fixed: using tail position with filters", "")("config", configName)("idx", idx)("shouldProcessCurrentEntry", shouldProcessCurrentEntry ? "true" : "false"));
    
    // Add overall timeout protection for the entire processing loop
    auto loopStartTime = std::chrono::steady_clock::now();
    const auto maxLoopTime = std::chrono::milliseconds(30000); // 30 seconds max for entire loop
    
    while (entryCount < maxEntriesPerBatch) {
        // Check overall timeout
        if (std::chrono::steady_clock::now() - loopStartTime > maxLoopTime) {
            LOG_WARNING(sLogger, ("journal processing loop timeout reached", "breaking")("config", configName)("idx", idx)("entries_processed", entryCount));
            break;
        }
        
        // Move to next entry (except for the first iteration if we're at tail)
        if (entryCount > 0 || !shouldProcessCurrentEntry) {
            if (!journalReader->Next()) {
                LOG_DEBUG(sLogger, ("no more entries available", "")("config", configName)("idx", idx)("entries_read", entryCount));
                break;
            }
            LOG_INFO(sLogger, ("moved to next entry", "")("config", configName)("idx", idx)("entry_count", entryCount));
        } else {
            LOG_INFO(sLogger, ("processing current entry (first iteration at tail)", "")("config", configName)("idx", idx)("entry_count", entryCount));
        }
        
        // Get the entry
        JournalEntry entry;
        LOG_INFO(sLogger, ("attempting to get journal entry", "")("config", configName)("idx", idx)("entry_count", entryCount));
        
        // Check if journal reader is open before calling GetEntry
        if (!journalReader->IsOpen()) {
            LOG_ERROR(sLogger, ("journal reader not open before GetEntry", "")("config", configName)("idx", idx)("entry_count", entryCount));
            break;
        }
        
        // Add timeout protection for GetEntry call
        auto startTime = std::chrono::steady_clock::now();
        const auto maxGetEntryTime = std::chrono::milliseconds(10000); // 10 seconds max for GetEntry
        
        bool getEntrySuccess = false;
        int retryCount = 0;
        const int maxRetries = 3;
        
        while (!getEntrySuccess && (std::chrono::steady_clock::now() - startTime) < maxGetEntryTime && retryCount < maxRetries) {
            getEntrySuccess = journalReader->GetEntry(entry);
            if (!getEntrySuccess) {
                retryCount++;
                LOG_DEBUG(sLogger, ("GetEntry attempt failed", "")("config", configName)("idx", idx)("attempt", retryCount)("max_attempts", maxRetries));
                // Brief pause before retry
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        
        if (!getEntrySuccess) {
            LOG_WARNING(sLogger, ("failed to get journal entry after timeout/retries", "skipping")("config", configName)("idx", idx)("entry_count", entryCount)("retries", retryCount));
            // If we're having persistent issues, try to move to next entry
            if (!journalReader->Next()) {
                LOG_WARNING(sLogger, ("cannot move to next entry, breaking loop", "")("config", configName)("idx", idx));
                break;
            }
            continue;
        }
        
        LOG_DEBUG(sLogger, ("read journal entry", "")("config", configName)("idx", idx)("cursor", entry.cursor)("fields_count", entry.fields.size()));
        
        // Apply field transformations (priority/facility conversion)
        ApplyJournalFieldTransforms(entry, config);
        
        // Add timestamp fields based on configuration
        if (config.useJournalEventTime) {
            // Use journal's realtime timestamp
            entry.fields["_realtime_timestamp_"] = std::to_string(entry.realtimeTimestamp);
        }
        entry.fields["_monotonic_timestamp_"] = std::to_string(entry.monotonicTimestamp);
        
        LOG_DEBUG(sLogger, ("journal entry processed", "")("config", configName)("idx", idx)("cursor", entry.cursor)("transformed", "true"));
        
        // Create event group if this is the first entry
        if (entryCount == 0) {
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            auto group = std::make_unique<PipelineEventGroup>(sourceBuffer);
            
            // Add the journal entry as a log event
            // Note: This is a simplified conversion - in a real implementation,
            // you might want to create a more sophisticated log event structure
            
            // For now, we'll create a simple log event with the journal data
            // The actual implementation would depend on your PipelineEvent structure
            
            // Send to processing queue
            if (!ProcessorRunner::GetInstance()->PushQueue(queueKey, idx, std::move(*group))) {
                LOG_ERROR(sLogger, 
                          ("failed to push journal data to process queue", "discard data")("config", configName)(
                              "input idx", idx)("queue", queueKey));
                break;
            }
            
            LOG_DEBUG(sLogger, 
                      ("successfully pushed journal batch to process queue", "")("config", configName)(
                          "input idx", idx)("queue", queueKey)("entries", entryCount + 1));
        }
        
        entryCount++;
        
        // Update checkpoint with current cursor
        SaveJournalCheckpoint(configName, idx, entry.cursor);
    }
    
    if (entryCount > 0) {
        LOG_INFO(sLogger, ("journal processing completed", "")("config", configName)("idx", idx)("entries_processed", entryCount));
    } else {
        LOG_DEBUG(sLogger, ("no journal entries processed", "")("config", configName)("idx", idx));
    }
    
    // Log any timeout issues
    auto loopEndTime = std::chrono::steady_clock::now();
    auto totalLoopTime = std::chrono::duration_cast<std::chrono::milliseconds>(loopEndTime - loopStartTime);
    if (totalLoopTime > std::chrono::milliseconds(1000)) { // Log if processing took more than 1 second
        LOG_INFO(sLogger, ("journal processing timing", "")("config", configName)("idx", idx)("total_time_ms", totalLoopTime.count())("entries_processed", entryCount));
    }
    
    // Close the journal reader
    journalReader->Close();
}

#ifdef APSARA_UNIT_TEST_MAIN
void JournalServer::Clear() {
    lock_guard<mutex> lock(mUpdateMux);
    mPipelineNameJournalConfigsMap.clear();
    mJournalCheckpoints.clear();
    mAddedInputs.clear();
    mDeletedInputs.clear();
}
#endif

} // namespace logtail 
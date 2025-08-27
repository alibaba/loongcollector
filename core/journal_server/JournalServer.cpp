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
        return;
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
    
    // Add unit filters
    for (const auto& unit : config.units) {
        if (!journalReader->AddMatch("_SYSTEMD_UNIT", unit)) {
            LOG_WARNING(sLogger, ("failed to add unit filter", unit)("config", configName)("idx", idx));
            filtersApplied = false;
        } else {
            LOG_DEBUG(sLogger, ("added unit filter", unit)("config", configName)("idx", idx));
        }
    }
    
    // Add identifier filters
    for (const auto& identifier : config.identifiers) {
        if (!journalReader->AddMatch("SYSLOG_IDENTIFIER", identifier)) {
            LOG_WARNING(sLogger, ("failed to add identifier filter", identifier)("config", configName)("idx", idx));
            filtersApplied = false;
        } else {
            LOG_DEBUG(sLogger, ("added identifier filter", identifier)("config", configName)("idx", idx));
        }
    }
    
    // Add kernel filter if requested
    if (config.kernel) {
        if (!journalReader->AddMatch("_TRANSPORT", "kernel")) {
            LOG_WARNING(sLogger, ("failed to add kernel filter", "")("config", configName)("idx", idx));
            filtersApplied = false;
        } else {
            LOG_DEBUG(sLogger, ("added kernel filter", "")("config", configName)("idx", idx));
        }
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
        if (config.seekPosition == "head" || (config.seekPosition == "cursor" && config.cursorSeekFallback == "head")) {
            LOG_DEBUG(sLogger, ("seeking to head", "")("config", configName)("idx", idx));
            seekSuccess = journalReader->SeekHead();
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
    
    // If we're at tail position and moved to last entry, process it first
    bool shouldProcessCurrentEntry = (config.seekPosition == "tail" || 
                                    (config.seekPosition == "cursor" && config.cursorSeekFallback == "tail"));
    
    LOG_INFO(sLogger, ("shouldProcessCurrentEntry", shouldProcessCurrentEntry ? "true" : "false")("config", configName)("idx", idx));
    
    while (entryCount < maxEntriesPerBatch) {
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
        
        if (!journalReader->GetEntry(entry)) {
            LOG_WARNING(sLogger, ("failed to get journal entry", "skipping")("config", configName)("idx", idx)("entry_count", entryCount));
            continue;
        }
        
        LOG_DEBUG(sLogger, ("read journal entry", "")("config", configName)("idx", idx)("cursor", entry.cursor)("fields_count", entry.fields.size()));
        
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
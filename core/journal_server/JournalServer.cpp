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
        
        // **恢复：与Go版本保持一致，每个identifier后都调用AddDisjunction**
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
    
    // **恢复：与Go版本保持一致，即使单个条件也调用AddDisjunction**
    if (!reader->AddDisjunction()) {
        LOG_WARNING(sLogger, ("failed to add kernel disjunction", "")("config", configName)("idx", idx));
        return false;
    }
    
    LOG_DEBUG(sLogger, ("added kernel filter with disjunction", "")("config", configName)("idx", idx));
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
    
    // Basic validation
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
    
    // 0. 建立和journal的连接
    LOG_INFO(sLogger, ("establishing journal connection", "")("config", configName)("idx", idx));
    auto journalReader = std::make_unique<SystemdJournalReader>();
    
    // Set timeout
    journalReader->SetTimeout(std::chrono::milliseconds(5000));
    
    // Set custom journal paths if specified
    if (!config.journalPaths.empty()) {
        LOG_INFO(sLogger, ("setting custom journal paths", "")("config", configName)("idx", idx)("paths_count", config.journalPaths.size()));
        journalReader->SetJournalPaths(config.journalPaths);
    }
    
    // Open the journal
    if (!journalReader->Open()) {
        LOG_ERROR(sLogger, ("failed to open journal", "skip processing")("config", configName)("idx", idx));
        return;
    }
    
    if (!journalReader->IsOpen()) {
        LOG_ERROR(sLogger, ("journal reader not open after Open() call", "")("config", configName)("idx", idx));
        return;
    }
    
    LOG_INFO(sLogger, ("journal connection established successfully", "")("config", configName)("idx", idx));
    
    // 1. 根据seekPosition处理定位逻辑
    bool seekSuccess = false;
    string checkpoint = GetJournalCheckpoint(configName, idx);
    
    // 首先尝试使用checkpoint
    if (!checkpoint.empty() && config.seekPosition == "cursor") {
        LOG_INFO(sLogger, ("seeking to checkpoint cursor", checkpoint.substr(0, 50))("config", configName)("idx", idx));
        seekSuccess = journalReader->SeekCursor(checkpoint);
        if (!seekSuccess) {
            LOG_WARNING(sLogger, ("failed to seek to checkpoint, using fallback position", config.cursorSeekFallback)("config", configName)("idx", idx));
        }
    }
    
    // 如果checkpoint失败或者不使用cursor，按seekPosition处理
    if (!seekSuccess) {
        if (config.seekPosition == "head" || (config.seekPosition == "cursor" && config.cursorSeekFallback == "head")) {
            LOG_INFO(sLogger, ("seeking to journal head", "")("config", configName)("idx", idx));
            seekSuccess = journalReader->SeekHead();
        } else {
            LOG_INFO(sLogger, ("seeking to journal tail", "")("config", configName)("idx", idx));
            seekSuccess = journalReader->SeekTail();
            
            // tail定位后需要回退到最后一条实际记录
            if (seekSuccess) {
                if (journalReader->Previous()) {
                    LOG_INFO(sLogger, ("moved to last actual entry after tail seek", "")("config", configName)("idx", idx));
                } else {
                    LOG_INFO(sLogger, ("no entries found after tail seek", "")("config", configName)("idx", idx));
                    journalReader->Close();
                    return;
                }
            }
        }
    }
    
    if (!seekSuccess) {
        LOG_ERROR(sLogger, ("failed to seek to position", config.seekPosition)("config", configName)("idx", idx));
        journalReader->Close();
        return;
    }
    
    // 2. 读取entry并处理
    int entryCount = 0;
    const int maxEntriesPerBatch = 100;
    bool isFirstEntry = true;
    
    LOG_INFO(sLogger, ("starting to read journal entries", "")("config", configName)("idx", idx)("seek_position", config.seekPosition));
    
    while (entryCount < maxEntriesPerBatch) {
        // 对于head模式或者非首次读取，需要调用Next()移动到下一条
        if (config.seekPosition == "head" || !isFirstEntry) {
            bool nextSuccess = journalReader->Next();
            if (!nextSuccess) {
                LOG_INFO(sLogger, ("no more entries available", "")("config", configName)("idx", idx)("entries_read", entryCount));
                break;
            }
            LOG_DEBUG(sLogger, ("moved to next entry", "")("config", configName)("idx", idx)("entry_count", entryCount));
        }
        
        // 读取当前entry
        JournalEntry entry;
        bool getEntrySuccess = journalReader->GetEntry(entry);
        
        if (!getEntrySuccess) {
            LOG_WARNING(sLogger, ("failed to get journal entry", "skipping")("config", configName)("idx", idx)("entry_count", entryCount));
            continue;
        }
        
        // 检查entry是否为空
        if (entry.fields.empty()) {
            LOG_WARNING(sLogger, ("journal entry is empty", "no fields found")("config", configName)("idx", idx)("cursor", entry.cursor));
            isFirstEntry = false;
            entryCount++;
            continue;
        }
        
        LOG_INFO(sLogger, ("successfully read journal entry", "")("config", configName)("idx", idx)("cursor", entry.cursor.substr(0, 50))("fields_count", entry.fields.size()));
        
        // 应用字段转换
        ApplyJournalFieldTransforms(entry, config);
        
        // 添加时间戳字段
        if (config.useJournalEventTime) {
            entry.fields["_realtime_timestamp_"] = std::to_string(entry.realtimeTimestamp);
        }
        entry.fields["_monotonic_timestamp_"] = std::to_string(entry.monotonicTimestamp);
        
        // 这里应该处理entry到pipeline的转换
        // 简化实现：直接记录处理的条目数
        entryCount++;
        isFirstEntry = false;
        
        // 更新checkpoint
        SaveJournalCheckpoint(configName, idx, entry.cursor);
        
        LOG_DEBUG(sLogger, ("journal entry processed", "")("config", configName)("idx", idx)("entry", entryCount)("cursor", entry.cursor.substr(0, 50)));
    }
    
    if (entryCount > 0) {
        LOG_INFO(sLogger, ("journal processing completed", "")("config", configName)("idx", idx)("entries_processed", entryCount));
    } else {
        LOG_WARNING(sLogger, ("no journal entries processed", "")("config", configName)("idx", idx)("seek_position", config.seekPosition));
    }
    
    // 关闭连接
    journalReader->Close();
    LOG_INFO(sLogger, ("journal connection closed", "")("config", configName)("idx", idx));
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
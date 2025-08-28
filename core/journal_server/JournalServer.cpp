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

#include <utility>

#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "journal_server/JournalConnectionManager.h"
#include "collection_pipeline/queue/QueueKey.h"
#include "models/PipelineEventGroup.h"
#include "common/Flags.h"
#include "common/memory/SourceBuffer.h"
#include "common/TimeUtil.h"
#include "app_config/AppConfig.h"
#include "logger/Logger.h"
#include "runner/ProcessorRunner.h"
#include "journal_server/JournalEntry.h"

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

// 过滤函数已移至JournalFilter类中处理

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
    
    // 清理所有journal连接
    JournalConnectionManager::GetInstance()->Clear();
    LOG_INFO(sLogger, ("journal server connections cleared", ""));
}

bool JournalServer::HasRegisteredPlugins() const {
    lock_guard<mutex> lock(mUpdateMux);
    return !mPipelineNameJournalConfigsMap.empty();
}

void JournalServer::ClearUnusedCheckpoints() {
    if (mIsUnusedCheckpointsCleared || time(nullptr) - mStartTime < 3600) {
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
        
        // Note: Checkpoint cleanup is now handled automatically by JournalConnectionManager
    }
    
    mDeletedInputs.clear();
    mIsUnusedCheckpointsCleared = true;
}

void JournalServer::AddJournalInput(const string& configName, size_t idx, const JournalConfig& config) {
    {
        lock_guard<mutex> lock(mUpdateMux);
        mPipelineNameJournalConfigsMap[configName][idx] = config;
        mAddedInputs.emplace(configName, idx);
        
        LOG_INFO(sLogger, ("journal input added", "")("config", configName)("idx", idx)("ctx_valid", config.ctx != nullptr)("total_pipelines", mPipelineNameJournalConfigsMap.size()));
    }
    
    // Note: Checkpoint management is now handled automatically by JournalConnectionManager
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
    
    // 移除对应的连接（会自动清理checkpoint）
    JournalConnectionManager::GetInstance()->RemoveConnection(configName, idx);
    LOG_INFO(sLogger, ("journal input removed with automatic connection and checkpoint cleanup", "")("config", configName)("idx", idx));
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

// Checkpoint methods removed - now handled by JournalConnectionManager

void JournalServer::Run() {
    LOG_INFO(sLogger, ("journal server", "started"));
    LOG_INFO(sLogger, ("journal server thread", "entering main loop"));
    unique_lock<mutex> lock(mThreadRunningMux);
    time_t lastConnectionCleanupTime = time(nullptr);
    
    while (mIsThreadRunning) {
        lock.unlock();
        
        LOG_INFO(sLogger, ("journal server loop", "iteration"));
        // Process journal entries for all registered configurations
        ProcessJournalEntries();
        
        auto cur = time(nullptr);
        
        // 智能清理策略：根据重置周期动态调整清理频率
        // 对于1小时重置周期，建议15分钟检查一次（3600/4 = 900秒）
        const int resetInterval = 3600; // 1小时重置周期
        const int cleanupInterval = JournalConnectionManager::GetRecommendedCleanupInterval(resetInterval);
        
        if (cur - lastConnectionCleanupTime >= cleanupInterval) {
            LOG_INFO(sLogger, ("cleaning up expired journal connections", ""));
            size_t cleanedCount = JournalConnectionManager::GetInstance()->CleanupExpiredConnections(3600);
            if (cleanedCount > 0) {
                LOG_INFO(sLogger, ("expired connections cleaned", "")("count", cleanedCount));
            }
            lastConnectionCleanupTime = cur;
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
        LOG_INFO(sLogger, ("configurations loaded from map", "")("total_pipelines", mPipelineNameJournalConfigsMap.size()));
    }
    
    // Add debug logging
    if (currentConfigs.empty()) {
        LOG_WARNING(sLogger, ("no journal configurations to process", "server may not have any registered journal inputs"));
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
    LOG_INFO(sLogger, ("ProcessJournalConfig", "started")("config", configName)("idx", idx)("ctx_valid", config.ctx != nullptr));
    
    // Basic validation
    if (!config.ctx) {
        LOG_ERROR(sLogger, ("CRITICAL: no context available for journal config", "this indicates initialization problem")("config", configName)("idx", idx));
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
    
    // 0. 获取或创建journal连接（使用连接管理器）
    LOG_INFO(sLogger, ("getting journal connection from manager", "")("config", configName)("idx", idx));
    auto journalReader = JournalConnectionManager::GetInstance()->GetOrCreateConnection(configName, idx, config);
    
    if (!journalReader) {
        LOG_ERROR(sLogger, ("failed to get journal connection", "skip processing")("config", configName)("idx", idx));
        return;
    }
    
    if (!journalReader->IsOpen()) {
        LOG_ERROR(sLogger, ("journal reader not open", "skip processing")("config", configName)("idx", idx));
        return;
    }
    
    LOG_INFO(sLogger, ("journal connection obtained successfully", "")("config", configName)("idx", idx));
    
    // 1. 根据seekPosition处理定位逻辑  
    bool seekSuccess = false;
    string checkpoint = JournalConnectionManager::GetInstance()->GetCheckpoint(configName, idx);
    
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
                    // 注意：使用长连接时不应该关闭reader，只是没有新条目而已
                    return;
                }
            }
        }
    }
    
    if (!seekSuccess) {
        LOG_ERROR(sLogger, ("failed to seek to position", config.seekPosition)("config", configName)("idx", idx));
        // 注意：seek失败时也不应该关闭长连接，连接管理器会在适当时候处理
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
        
        // 创建PipelineEventGroup并添加LogEvent
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        
        // 添加LogEvent并设置字段
        LogEvent* logEvent = eventGroup.AddLogEvent();
        
        // 设置所有journal字段到LogEvent
        for (const auto& field : entry.fields) {
            logEvent->SetContent(field.first, field.second);
        }
        
        // 添加时间戳字段（始终透出）
        logEvent->SetContent("_realtime_timestamp_", std::to_string(entry.realtimeTimestamp));
        logEvent->SetContent("_monotonic_timestamp_", std::to_string(entry.monotonicTimestamp));
        
        // 设置时间戳
        if (config.useJournalEventTime && entry.realtimeTimestamp > 0) {
            // journal的realtimeTimestamp是微秒，需要转换为秒和纳秒
            uint64_t seconds = entry.realtimeTimestamp / 1000000;
            uint64_t nanoseconds = (entry.realtimeTimestamp % 1000000) * 1000;
            logEvent->SetTimestamp(seconds, nanoseconds);
        } else {
            // 使用当前时间（保持纳秒精度，应用秒级时间自动调整）
            auto currentTime = GetCurrentLogtailTime();
            time_t adjustedSeconds = currentTime.tv_sec;
            time_t adjustedNanoSeconds = currentTime.tv_nsec;

            if (AppConfig::GetInstance()->EnableLogTimeAutoAdjust()) {
                adjustedSeconds += GetTimeDelta();
                adjustedNanoSeconds += GetTimeDelta()*1000;
                LOG_DEBUG(sLogger, ("new timestamp", "adjustedSeconds")("new nanoSeconds", adjustedNanoSeconds));

            }
            logEvent->SetTimestamp(adjustedSeconds, adjustedNanoSeconds);
        }
        
        LOG_DEBUG(sLogger, ("created LogEvent", "")("config", configName)("idx", idx)("fields", entry.fields.size())("timestamp", logEvent->GetTimestamp()));
        
        // 推送到处理队列
        if (!ProcessorRunner::GetInstance()->PushQueue(queueKey, idx, std::move(eventGroup))) {
            LOG_ERROR(sLogger, ("failed to push journal data to process queue", "discard data")
                      ("config", configName)("input idx", idx)("queue", queueKey));
        } else {
            LOG_DEBUG(sLogger, ("successfully pushed journal event to process queue", "")
                      ("config", configName)("input idx", idx)("queue", queueKey));
        }
        
        entryCount++;
        isFirstEntry = false;
        
        // 更新checkpoint - 使用ConnectionManager统一管理
        JournalConnectionManager::GetInstance()->SaveCheckpoint(configName, idx, entry.cursor);
        
        LOG_DEBUG(sLogger, ("journal entry processed", "")("config", configName)("idx", idx)("entry", entryCount)("cursor", entry.cursor.substr(0, 50)));
    }
    
    if (entryCount > 0) {
        LOG_INFO(sLogger, ("journal processing completed", "")("config", configName)("idx", idx)("entries_processed", entryCount));
    } else {
        LOG_WARNING(sLogger, ("no journal entries processed", "")("config", configName)("idx", idx)("seek_position", config.seekPosition));
    }
    
    // 注意：不需要关闭连接，因为使用连接管理器维持长连接
    LOG_DEBUG(sLogger, ("journal processing completed, connection remains open", "")("config", configName)("idx", idx));
}

#ifdef APSARA_UNIT_TEST_MAIN
void JournalServer::Clear() {
    lock_guard<mutex> lock(mUpdateMux);
    mPipelineNameJournalConfigsMap.clear();
    mAddedInputs.clear();
    mDeletedInputs.clear();
    // Note: Checkpoint cleanup is now handled by JournalConnectionManager
}
#endif

} // namespace logtail 
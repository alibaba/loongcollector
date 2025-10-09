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

#include <algorithm>
#include <memory>

#include "checkpoint/JournalCheckpointManager.h"
#include "reader/JournalReader.h"
#include "common/JournalConfig.h"
#include "models/PipelineEventGroup.h"
#include "common/memory/SourceBuffer.h"
#include "common/TimeUtil.h"
#include "app_config/AppConfig.h"
#include "runner/ProcessorRunner.h"
#include "common/JournalUtils.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail::impl {

// 前向声明
bool MoveToNextJournalEntry(const string& configName, size_t idx, const JournalConfig& config,
                           const shared_ptr<SystemdJournalReader>& journalReader, 
                           bool isFirstEntry, int entryCount);
bool ReadAndValidateEntry(const string& configName, size_t idx,
                         const shared_ptr<SystemdJournalReader>& journalReader, JournalEntry& entry);
bool CreateAndPushEventGroup(const string& configName, size_t idx, const JournalConfig& config,
                            const JournalEntry& entry, QueueKey queueKey);
void ApplyJournalFieldTransforms(JournalEntry& entry, const JournalConfig& config);

// =============================================================================
// 读取条目
// =============================================================================

void ReadJournalEntries(const string& configName, size_t idx, const JournalConfig& config,
                       const std::shared_ptr<SystemdJournalReader>& journalReader, QueueKey queueKey) {
    int entryCount = 0;
    // 防御性边界检查：确保maxEntriesPerBatch在合理范围内
    const int maxEntriesPerBatch = std::max(1, std::min(config.maxEntriesPerBatch, 10000));
    
    // 如果配置值被修正，记录警告
    if (config.maxEntriesPerBatch != maxEntriesPerBatch) {
        LOG_WARNING(sLogger, ("maxEntriesPerBatch clamped to safe range", "")("config", configName)("idx", idx)("original", config.maxEntriesPerBatch)("clamped", maxEntriesPerBatch));
    }
    bool isFirstEntry = true;
    
    try {
        while (entryCount < maxEntriesPerBatch) {
            // Step 1: 移动到下一个entry，如果需要的话
            if (!MoveToNextJournalEntry(configName, idx, config, journalReader, isFirstEntry, entryCount)) {
                break;
            }
            
            // Step 2: 读取和验证entry
            JournalEntry entry;
            if (!ReadAndValidateEntry(configName, idx, journalReader, entry)) {
                // 如果entry为空，则跳过
                if (entry.fields.empty() && !entry.cursor.empty()) {
                    isFirstEntry = false;
                    entryCount++;
                    continue;
                }
                // 连接错误或读取失败
                break;
            }
            
            // Step 3: 处理enrty (transform, create event, push)
            if (!CreateAndPushEventGroup(configName, idx, config, entry, queueKey)) {
                LOG_ERROR(sLogger, ("failed to process journal entry", "continue")("config", configName)("idx", idx));
            }
            
            entryCount++;
            isFirstEntry = false;
            
            JournalCheckpointManager::GetInstance().SaveCheckpoint(configName, idx, entry.cursor);
        }
        
        // 只在没有处理任何条目且可能存在问题时记录警告
        if (entryCount == 0 && config.seekPosition != "tail") {
            LOG_WARNING(sLogger, ("no journal entries processed", "may indicate configuration issue")("config", configName)("idx", idx)("seek_position", config.seekPosition));
        }
        
        // journal处理完成，连接保持打开状态以供后续使用
        
    } catch (const std::exception& e) {
        LOG_ERROR(sLogger, ("exception during journal entries processing", e.what())("config", configName)("idx", idx)("entries_processed", entryCount));
    } catch (...) {
        LOG_ERROR(sLogger, ("unknown exception during journal entries processing", "")("config", configName)("idx", idx)("entries_processed", entryCount));
    }
}

bool MoveToNextJournalEntry(const string& configName, size_t idx, const JournalConfig& config,
                           const std::shared_ptr<SystemdJournalReader>& journalReader, 
                           bool isFirstEntry, int entryCount) {
    try {
        // 对于head模式或者非首次读取，需要调用Next()移动到下一条
        // 对于tail模式，首次读取时已经在SeekTail+Previous后定位到最后一个条目，不需要Next()
        if (config.seekPosition == "head" || !isFirstEntry) {
            bool nextSuccess = journalReader->Next();
            if (!nextSuccess) {
                // 检查连接是否仍然有效
                if (!journalReader->IsOpen()) {
                    LOG_WARNING(sLogger, ("journal connection closed during processing", "aborting batch")("config", configName)("idx", idx)("entries_processed", entryCount));
                    return false;
                }
                
                // 在事件驱动模式下，不需要等待，直接返回false让上层处理
                return false;
            }
            // 已移动到下一个条目，继续处理
        }
        // 对于tail模式的首次读取，直接处理当前位置的条目（已在SeekTail+Previous后定位）
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR(sLogger, ("exception during journal entry navigation", e.what())("config", configName)("idx", idx)("entries_processed", entryCount));
        return false;
    } catch (...) {
        LOG_ERROR(sLogger, ("unknown exception during journal entry navigation", "")("config", configName)("idx", idx)("entries_processed", entryCount));
        return false;
    }
}

bool ReadAndValidateEntry(const string& configName, size_t idx,
                         const std::shared_ptr<SystemdJournalReader>& journalReader, JournalEntry& entry) {
    try {
        // 在读取entry之前验证连接状态
        if (!journalReader->IsOpen()) {
            LOG_WARNING(sLogger, ("journal connection closed before GetEntry", "aborting batch")("config", configName)("idx", idx));
            return false;
        }
        
        // 读取当前entry
        bool getEntrySuccess = journalReader->GetEntry(entry);
        
        if (!getEntrySuccess) {
            LOG_WARNING(sLogger, ("failed to get journal entry", "skipping")("config", configName)("idx", idx));
            
            // 检查是否是连接问题导致的失败
            if (!journalReader->IsOpen()) {
                LOG_WARNING(sLogger, ("GetEntry failed due to closed connection", "aborting batch")("config", configName)("idx", idx));
                return false;
            }
            return false;  // Read failed but connection ok, caller should continue
        }
        
        // 检查entry是否为空
        if (entry.fields.empty()) {
            LOG_WARNING(sLogger, ("journal entry is empty", "no fields found")("config", configName)("idx", idx)("cursor", entry.cursor));
            return false;  // Empty entry, special handling needed by caller
        }
        

        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR(sLogger, ("exception during journal entry reading", e.what())("config", configName)("idx", idx));
        // 清空entry以确保不会使用部分读取的数据
        entry = JournalEntry();
        return false;
    } catch (...) {
        LOG_ERROR(sLogger, ("unknown exception during journal entry reading", "")("config", configName)("idx", idx));
        // 清空entry以确保不会使用部分读取的数据
        entry = JournalEntry();
        return false;
    }
}

LogEvent* CreateLogEventFromJournal(const JournalEntry& entry, const JournalConfig& config, 
                                   PipelineEventGroup& eventGroup) {
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
            adjustedNanoSeconds += GetTimeDelta()*1000000;
            // 时间已调整，应用了时间偏移量
        }
        logEvent->SetTimestamp(adjustedSeconds, adjustedNanoSeconds);
    }
    
    return logEvent;
}

bool CreateAndPushEventGroup(const string& configName, size_t idx, const JournalConfig& config,
                            const JournalEntry& entry, QueueKey queueKey) {
    // 应用字段转换
    JournalEntry mutableEntry = entry;  // Make a mutable copy
    ApplyJournalFieldTransforms(mutableEntry, config);
    
    // 创建PipelineEventGroup并添加LogEvent
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    
    CreateLogEventFromJournal(mutableEntry, config, eventGroup);
    
    // 已创建LogEvent对象，包含所有字段和时间戳信息
    
    // 推送到处理队列
    if (!ProcessorRunner::GetInstance()->PushQueue(queueKey, idx, std::move(eventGroup))) {
        LOG_ERROR(sLogger, ("failed to push journal data to process queue", "discard data")
                  ("config", configName)("input idx", idx)("queue", queueKey));
        return false;
    }          
    // 成功推送到处理队列
    return true;
}

void ApplyJournalFieldTransforms(JournalEntry& entry, const JournalConfig& config) {
    if (config.parsePriority) {
        // 查找PRIORITY字段
        auto it = entry.fields.find("PRIORITY");
        if (it != entry.fields.end()) {
            // 查找PRIORITY字段对应的值
            auto priorityIt = JournalUtils::kPriorityConversionMap.find(it->second);
            if (priorityIt != JournalUtils::kPriorityConversionMap.end()) {
                // 如果找到对应的值，则更新PRIORITY字段
                it->second = priorityIt->second;
            }
        }
    }
    
    if (config.parseSyslogFacility) {
        // 查找SYSLOG_FACILITY字段
        auto it = entry.fields.find("SYSLOG_FACILITY");
        if (it != entry.fields.end()) {
            // 查找SYSLOG_FACILITY字段对应的值
            auto facilityIt = JournalUtils::kSyslogFacilityString.find(it->second);
            if (facilityIt != JournalUtils::kSyslogFacilityString.end()) {
                // 如果找到对应的值，则更新SYSLOG_FACILITY字段
                it->second = facilityIt->second;
            }
        }
    }
}

} // namespace logtail::impl 
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

#include "app_config/AppConfig.h"
#include "common/JournalConfig.h"
#include "common/JournalUtils.h" // IWYU pragma: keep
#include "common/TimeUtil.h"
#include "common/memory/SourceBuffer.h"
#include "logger/Logger.h"
#include "models/PipelineEventGroup.h"
#include "reader/JournalReader.h"
#include "runner/ProcessorRunner.h"

using namespace std;

namespace logtail {

namespace {

/**
 * @brief 尝试从journal错误中恢复（如日志轮转导致的cursor失效）
 * @param journalReader journal reader指针
 * @param configName 配置名称
 * @param idx 配置索引
 * @param errorContext 错误上下文描述
 * @return true 如果恢复成功，false 如果恢复失败
 */
bool RecoverFromJournalError(const std::shared_ptr<SystemdJournalReader>& journalReader,
                             const string& configName,
                             size_t idx,
                             const string& errorContext) {
    LOG_WARNING(sLogger,
               ("journal error detected, attempting recovery",
                errorContext)
               ("config", configName)("idx", idx));
    
    // 尝试恢复策略1：重新seek到head（当前可读的最早日志）
    if (journalReader->SeekHead()) {
        LOG_INFO(sLogger,
                ("recovered from journal error by seeking to head",
                 "continuing from earliest available entry")
                ("config", configName)("idx", idx)("context", errorContext));
        return true;
    }
    
    // 如果SeekHead失败，可以在这里添加其他恢复策略
    // 例如：重新打开连接、尝试SeekTail等
    
    LOG_ERROR(sLogger,
             ("failed to recover from journal error",
              "all recovery attempts failed")
             ("config", configName)("idx", idx)("context", errorContext));
    return false;
}

bool MoveToNextJournalEntry(const string& configName,
                            size_t idx,
                            const JournalConfig& config,
                            const std::shared_ptr<SystemdJournalReader>& journalReader,
                            bool isFirstEntry,
                            int entryCount) {
    try {
        if (config.seekPosition == "head" || !isFirstEntry) {
            JournalReadStatus status = journalReader->NextWithStatus();
            
            if (status == JournalReadStatus::kOk) {
                // 成功移动到下一条
                return true;
            }
            if (status == JournalReadStatus::kEndOfJournal) {
                // 到达末尾，正常结束
                return false;
            }
            // 错误情况：可能是日志轮转导致cursor失效
            // 尝试错误恢复
            return RecoverFromJournalError(
                journalReader, 
                configName, 
                idx, 
                "navigation error during Next(), cursor may be invalidated by log rotation");
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(sLogger,
                  ("exception during journal entry navigation",
                   e.what())("config", configName)("idx", idx)("entries_processed", entryCount));
        
        // 尝试错误恢复
        string errorMsg = string("exception during navigation: ") + e.what();
        if (RecoverFromJournalError(journalReader, configName, idx, errorMsg)) {
            // 恢复成功，可以继续处理
            return true;
        }
        return false;
    } catch (...) {
        LOG_ERROR(sLogger,
                  ("unknown exception during journal entry navigation",
                   "")("config", configName)("idx", idx)("entries_processed", entryCount));
        
        // 尝试错误恢复
        if (RecoverFromJournalError(journalReader, configName, idx, 
                                   "unknown exception during navigation")) {
            // 恢复成功，可以继续处理
            return true;
        }
        return false;
    }
}

bool ReadAndValidateEntry(const string& configName,
                          size_t idx,
                          const std::shared_ptr<SystemdJournalReader>& journalReader,
                          JournalEntry& entry) {
    try {
        // 读取当前entry
        bool getEntrySuccess = journalReader->GetEntry(entry);

        if (!getEntrySuccess) {
            // GetEntry 失败可能的原因：
            // 1. 连接已关闭
            // 2. journal 文件被轮转删除（sd_journal_get_realtime_usec 返回错误）
            // 3. cursor 失效
            // 
            // 无论哪种情况，都应该尝试错误恢复
            string errorContext = !journalReader->IsOpen() 
                ? "GetEntry failed, connection closed"
                : "GetEntry failed (possibly due to journal rotation or timestamp read error)";
            
            LOG_WARNING(sLogger,
                       ("GetEntry failed, attempting recovery", errorContext)
                       ("config", configName)("idx", idx));
            
            // 尝试错误恢复
            if (RecoverFromJournalError(journalReader, configName, idx, errorContext)) {
                // 恢复成功，重试读取
                if (journalReader->GetEntry(entry)) {
                    return true;
                }
                // 恢复后重试仍然失败，跳过这条
                LOG_WARNING(sLogger, 
                           ("GetEntry still failed after recovery", "skipping entry")
                           ("config", configName)("idx", idx));
                return false;
            }
            // 恢复失败，中止批次
            return false;
        }
        // 检查entry是否为空
        if (entry.fields.empty()) {
            LOG_WARNING(sLogger,
                        ("journal entry is empty", "no fields found")("config", configName)("idx", idx)("cursor",
                                                                                                        entry.cursor));
            return false; // Empty entry, special handling needed by caller
        }
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(sLogger, ("exception during journal entry reading", e.what())("config", configName)("idx", idx));
        // 清空entry以确保不会使用部分读取的数据
        entry = JournalEntry();
        
        // 尝试错误恢复
        string errorMsg = string("exception during GetEntry: ") + e.what();
        if (RecoverFromJournalError(journalReader, configName, idx, errorMsg)) {
            // 恢复成功，重试读取
            return journalReader->GetEntry(entry);
        }
        return false;
    } catch (...) {
        LOG_ERROR(sLogger, ("unknown exception during journal entry reading", "")("config", configName)("idx", idx));
        entry = JournalEntry();
        
        // 尝试错误恢复
        if (RecoverFromJournalError(journalReader, configName, idx, 
                                   "unknown exception during GetEntry")) {
            // 恢复成功，重试读取
            return journalReader->GetEntry(entry);
        }
        return false;
    }
}

LogEvent*
CreateLogEventFromJournal(const JournalEntry& entry, const JournalConfig& config, PipelineEventGroup& eventGroup) {
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
            adjustedNanoSeconds += GetTimeDelta() * 1000000;
            // 时间已调整，应用了时间偏移量
        }
        logEvent->SetTimestamp(adjustedSeconds, adjustedNanoSeconds);
    }

    return logEvent;
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

bool CreateAndPushEventGroup(
    const string& configName, size_t idx, const JournalConfig& config, const JournalEntry& entry, QueueKey queueKey) {
    // 应用字段转换
    JournalEntry mutableEntry = entry; // Make a mutable copy
    ApplyJournalFieldTransforms(mutableEntry, config);

    // 创建PipelineEventGroup并添加LogEvent
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);

    // 已创建LogEvent对象，包含所有字段和时间戳信息
    CreateLogEventFromJournal(mutableEntry, config, eventGroup);

    // 推送到处理队列
    if (!ProcessorRunner::GetInstance()->PushQueue(queueKey, idx, std::move(eventGroup))) {
        LOG_ERROR(sLogger,
                  ("failed to push journal data to process queue",
                   "discard data")("config", configName)("input idx", idx)("queue", queueKey));
        return false;
    }
    return true;
}

} // anonymous namespace

void ReadJournalEntries(const string& configName,
                        size_t idx,
                        const JournalConfig& config,
                        const std::shared_ptr<SystemdJournalReader>& journalReader,
                        QueueKey queueKey) {
    int entryCount = 0;
    // 防御性边界检查：确保maxEntriesPerBatch在合理范围内
    const int maxEntriesPerBatch = std::max(1, std::min(config.maxEntriesPerBatch, 10000));//公平参数

    // 如果配置值被修正，记录警告
    if (config.maxEntriesPerBatch != maxEntriesPerBatch) {
        LOG_WARNING(sLogger,
                    ("maxEntriesPerBatch clamped to safe range", "")("config", configName)("idx", idx)(
                        "original", config.maxEntriesPerBatch)("clamped", maxEntriesPerBatch));
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
        }

        // 只在没有处理任何条目且可能存在问题时记录警告
        if (entryCount == 0 && config.seekPosition != "tail") {
            LOG_WARNING(sLogger,
                        ("no journal entries processed", "may indicate configuration issue")("config", configName)(
                            "idx", idx)("seek_position", config.seekPosition));
        }
    } catch (const std::exception& e) {
        LOG_ERROR(sLogger,
                  ("exception during journal entries processing",
                   e.what())("config", configName)("idx", idx)("entries_processed", entryCount));
    } catch (...) {
        LOG_ERROR(sLogger,
                  ("unknown exception during journal entries processing",
                   "")("config", configName)("idx", idx)("entries_processed", entryCount));
    }
}

} // namespace logtail

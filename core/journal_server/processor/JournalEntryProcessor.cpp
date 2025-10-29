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
 * @param cursorSeekFallback cursor回退位置（"head" 或 "tail"）
 * @param errorContext 错误上下文描述
 * @return true 如果恢复成功，false 如果恢复失败
 */
bool RecoverFromJournalError(const std::shared_ptr<JournalReader>& journalReader,
                             const string& configName,
                             const string& cursorSeekFallback,
                             const string& errorContext) {
    LOG_WARNING(sLogger,
                ("journal processor journal error detected, attempting recovery",
                 errorContext)("config", configName)("fallback", cursorSeekFallback));

    // 根据配置的cursorSeekFallback决定恢复策略
    bool seekSuccess = false;
    if (cursorSeekFallback == "head") {
        // 恢复策略1：重新seek到head（当前可读的最早日志）
        seekSuccess = journalReader->SeekHead();
        if (seekSuccess) {
            LOG_INFO(sLogger,
                     ("journal processor recovered from journal error by seeking to head",
                      "continuing from earliest available entry")("config", configName)("context", errorContext));
        }
    } else if (cursorSeekFallback == "tail") {
        // 恢复策略2：重新seek到tail（最新日志）
        seekSuccess = journalReader->SeekTail();
        if (seekSuccess) {
            LOG_INFO(sLogger,
                     ("journal processor recovered from journal error by seeking to tail",
                      "continuing from latest entry")("config", configName)("context", errorContext));
        }
    } else {
        LOG_WARNING(sLogger,
                    ("journal processor invalid cursorSeekFallback value, using head as default",
                     cursorSeekFallback)("config", configName));
        seekSuccess = journalReader->SeekHead();
    }

    if (seekSuccess) {
        return true;
    }

    LOG_ERROR(sLogger,
              ("journal processor failed to recover from journal error", "all recovery attempts failed")(
                  "config", configName)("context", errorContext)("fallback", cursorSeekFallback));
    return false;
}

bool MoveToNextJournalEntry(const string& configName,
                            const std::shared_ptr<JournalReader>& journalReader,
                            const string& cursorSeekFallback,
                            int& entryCount) {
    try {
        JournalReadStatus status = journalReader->NextWithStatus();

        if (status == JournalReadStatus::kOk) {
            return true;
        }
        if (status == JournalReadStatus::kEndOfJournal) {
            // 到达末尾，正常结束
            return false;
        }
        // 错误情况：例如可能是日志轮转导致cursor失效
        // 尝试错误恢复
        return RecoverFromJournalError(journalReader,
                                       configName,
                                       cursorSeekFallback,
                                       "navigation error during Next(), cursor may be invalidated by log rotation");
    } catch (const std::exception& e) {
        LOG_ERROR(sLogger,
                  ("journal processor exception during journal entry navigation",
                   e.what())("config", configName)("entries_processed", entryCount));

        string errorMsg = string("exception during navigation: ") + e.what();
        if (RecoverFromJournalError(journalReader, configName, cursorSeekFallback, errorMsg)) {
            return true;
        }
        return false;
    } catch (...) {
        LOG_ERROR(sLogger,
                  ("journal processor unknown exception during journal entry navigation",
                   "")("config", configName)("entries_processed", entryCount));

        if (RecoverFromJournalError(
                journalReader, configName, cursorSeekFallback, "unknown exception during navigation")) {
            return true;
        }
        return false;
    }
}

bool ReadAndValidateEntry(const string& configName,
                          const std::shared_ptr<JournalReader>& journalReader,
                          const string& cursorSeekFallback,
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

            LOG_WARNING(
                sLogger,
                ("journal processor get entry failed, attempting recovery", errorContext)("config", configName));

            // 尝试错误恢复
            if (RecoverFromJournalError(journalReader, configName, cursorSeekFallback, errorContext)) {
                // 恢复成功，重试读取
                if (journalReader->GetEntry(entry)) {
                    return true;
                }
                // 恢复后重试仍然失败，跳过这条
                LOG_WARNING(sLogger,
                            ("journal processor get entry still failed after recovery", "skipping entry")("config",
                                                                                                          configName));
                return false;
            }
            // 恢复失败，中止批次
            return false;
        }
        // 检查entry是否为空
        if (entry.fields.empty()) {
            LOG_WARNING(sLogger,
                        ("journal processor journal entry is empty",
                         "no fields found")("config", configName)("cursor", entry.cursor));
            return false;
        }
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(sLogger,
                  ("journal processor exception during journal entry reading", e.what())("config", configName));
        // 清空entry以确保不会使用部分读取的数据
        entry = JournalEntry();

        // 尝试错误恢复
        string errorMsg = string("exception during GetEntry: ") + e.what();
        if (RecoverFromJournalError(journalReader, configName, cursorSeekFallback, errorMsg)) {
            // 恢复成功，重试读取
            return journalReader->GetEntry(entry);
        }
        return false;
    } catch (...) {
        LOG_ERROR(sLogger,
                  ("journal processor unknown exception during journal entry reading", "")("config", configName));
        entry = JournalEntry();

        // 尝试错误恢复
        if (RecoverFromJournalError(
                journalReader, configName, cursorSeekFallback, "unknown exception during GetEntry")) {
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
    if (config.mUseJournalEventTime && entry.realtimeTimestamp > 0) {
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
    if (config.mParsePriority) {
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

    if (config.mParseSyslogFacility) {
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

bool CreateAndPushEventGroup(const string& configName,
                             const JournalConfig& config,
                             const JournalEntry& entry,
                             QueueKey queueKey) {
    // 应用字段转换
    JournalEntry mutableEntry = entry; // Make a mutable copy
    ApplyJournalFieldTransforms(mutableEntry, config);

    // 创建PipelineEventGroup并添加LogEvent
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);

    // 已创建LogEvent对象，包含所有字段和时间戳信息
    CreateLogEventFromJournal(mutableEntry, config, eventGroup);

    // 推送到处理队列
    if (!ProcessorRunner::GetInstance()->PushQueue(queueKey, 0, std::move(eventGroup))) {
        LOG_ERROR(sLogger,
                  ("journal processor failed to push journal data to process queue",
                   "discard data")("config", configName)("input idx", 0)("queue", queueKey));
        return false;
    }
    return true;
}

} // anonymous namespace

void ReadJournalEntries(const string& configName,
                        const JournalConfig& config,
                        const std::shared_ptr<JournalReader>& journalReader,
                        QueueKey queueKey,
                        bool* hasPendingDataOut) {
    int entryCount = 0;
    // 防御性边界检查：确保maxEntriesPerBatch在合理范围内
    const int maxEntriesPerBatch = std::max(1, std::min(config.mMaxEntriesPerBatch, 10000)); // 公平参数

    // 如果配置值被修正，记录警告
    if (config.mMaxEntriesPerBatch != maxEntriesPerBatch) {
        LOG_WARNING(sLogger,
                    ("journal processor maxEntriesPerBatch clamped to safe range",
                     "")("config", configName)("original", config.mMaxEntriesPerBatch)("clamped", maxEntriesPerBatch));
    }

    try {
        while (entryCount < maxEntriesPerBatch) {
            if (!MoveToNextJournalEntry(configName, journalReader, config.mCursorSeekFallback, entryCount)) {
                break;
            }

            JournalEntry entry;
            if (!ReadAndValidateEntry(configName, journalReader, config.mCursorSeekFallback, entry)) {
                // 如果entry为空，则跳过
                if (entry.fields.empty() && !entry.cursor.empty()) {
                    entryCount++;
                    continue;
                }
                // 连接错误或读取失败
                break;
            }

            if (!CreateAndPushEventGroup(configName, config, entry, queueKey)) {
                LOG_ERROR(sLogger,
                          ("journal processor failed to process journal entry", "continue")("config", configName));
            }

            entryCount++;
        }

        // 判断是否还有待处理数据：
        // 如果因为批处理限制而退出（entryCount == maxEntriesPerBatch），说明还有数据
        // 如果正常退出（没有更多数据），说明已读完
        if (hasPendingDataOut != nullptr) {
            *hasPendingDataOut = (entryCount == maxEntriesPerBatch);
        }

        // 只在没有处理任何条目且可能存在问题时记录警告
        if (entryCount == 0 && config.mSeekPosition != "tail") {
            LOG_DEBUG(sLogger,
                      ("journal processor no journal entries processed", "may indicate configuration issue")(
                          "config", configName)("seek_position", config.mSeekPosition));
        }
    } catch (const std::exception& e) {
        LOG_ERROR(sLogger,
                  ("journal processor exception during journal entries processing",
                   e.what())("config", configName)("entries_processed", entryCount));
        if (hasPendingDataOut != nullptr) {
            *hasPendingDataOut = false; // 异常时保守处理
        }
    } catch (...) {
        LOG_ERROR(sLogger,
                  ("journal processor unknown exception during journal entries processing",
                   "")("config", configName)("entries_processed", entryCount));
        if (hasPendingDataOut != nullptr) {
            *hasPendingDataOut = false; // 异常时保守处理
        }
    }
}

} // namespace logtail

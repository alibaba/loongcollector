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
#include <chrono>
#include <memory>

#include "app_config/AppConfig.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
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

bool RecoverFromJournalError(const std::shared_ptr<JournalReader>& journalReader,
                             const string& configName,
                             const string& cursorSeekFallback,
                             const string& errorContext) {
    LOG_WARNING(sLogger,
                ("journal processor journal error detected, attempting recovery",
                 errorContext)("config", configName)("fallback", cursorSeekFallback));

    bool seekSuccess = false;
    if (cursorSeekFallback == "head") {
        // Recovery strategy 1: seek to head (earliest available log)
        seekSuccess = journalReader->SeekHead();
        if (seekSuccess) {
            LOG_INFO(sLogger,
                     ("journal processor recovered from journal error by seeking to head",
                      "continuing from earliest available entry")("config", configName)("context", errorContext));
        }
    } else if (cursorSeekFallback == "tail") {
        // Recovery strategy 2: seek to tail (latest log)
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
    JournalReadStatus status = journalReader->NextWithStatus();

    if (status == JournalReadStatus::kOk) {
        return true;
    }
    if (status == JournalReadStatus::kEndOfJournal) {
        // Reached end, normal termination
        return false;
    }
    // Error case: possibly cursor invalidation due to log rotation/deleted
    // Attempt error recovery
    return RecoverFromJournalError(journalReader,
                                   configName,
                                   cursorSeekFallback,
                                   "navigation error during Next(), cursor may be invalidated by log rotation");
}

bool ReadAndValidateEntry(const string& configName,
                          const std::shared_ptr<JournalReader>& journalReader,
                          const string& cursorSeekFallback,
                          JournalEntry& entry) {
    try {
        // Read current entry
        bool getEntrySuccess = journalReader->GetEntry(entry);

        if (!getEntrySuccess) {
            // Possible reasons for GetEntry failure:
            // 1. Connection closed
            // 2. Journal file rotated/deleted (sd_journal_get_realtime_usec returns error)
            // 3. Cursor invalidated
            //
            // In all cases, attempt error recovery
            string errorContext = !journalReader->IsOpen()
                ? "GetEntry failed, connection closed"
                : "GetEntry failed (possibly due to journal rotation or timestamp read error)";

            LOG_WARNING(
                sLogger,
                ("journal processor get entry failed, attempting recovery", errorContext)("config", configName));

            // Attempt error recovery
            if (RecoverFromJournalError(journalReader, configName, cursorSeekFallback, errorContext)) {
                if (journalReader->GetEntry(entry)) {
                    return true;
                }
                LOG_WARNING(sLogger,
                            ("journal processor get entry still failed after recovery", "skipping entry")("config",
                                                                                                          configName));
                return false;
            }
            // Recovery failed, abort batch
            return false;
        }
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
        // Clear entry to ensure partial data is not used
        entry = JournalEntry();

        // Attempt error recovery
        string errorMsg = string("exception during GetEntry: ") + e.what();
        if (RecoverFromJournalError(journalReader, configName, cursorSeekFallback, errorMsg)) {
            return journalReader->GetEntry(entry);
        }
        return false;
    } catch (...) {
        LOG_ERROR(sLogger,
                  ("journal processor unknown exception during journal entry reading", "")("config", configName));
        entry = JournalEntry();

        if (RecoverFromJournalError(
                journalReader, configName, cursorSeekFallback, "unknown exception during GetEntry")) {
            return journalReader->GetEntry(entry);
        }
        return false;
    }
}

LogEvent*
CreateLogEventFromJournal(const JournalEntry& entry, const JournalConfig& config, PipelineEventGroup& eventGroup) {
    LogEvent* logEvent = eventGroup.AddLogEvent();

    for (const auto& field : entry.fields) {
        std::string fieldValue = field.second;

        // Apply field transformations if configured
        if (field.first == "PRIORITY" && config.mParsePriority) {
            const auto& conversionMap = JournalUtils::kPriorityConversionMap;
            auto it = conversionMap.find(field.second);
            if (it != conversionMap.end()) {
                fieldValue = it->second;
            }
        } else if (field.first == "SYSLOG_FACILITY" && config.mParseSyslogFacility) {
            const auto& conversionMap = JournalUtils::kSyslogFacilityString;
            auto it = conversionMap.find(field.second);
            if (it != conversionMap.end()) {
                fieldValue = it->second;
            }
        }

        logEvent->SetContent(field.first, fieldValue);
    }

    // Add timestamp fields (always exposed)
    logEvent->SetContent("_realtime_timestamp_", std::to_string(entry.realtimeTimestamp));
    logEvent->SetContent("_monotonic_timestamp_", std::to_string(entry.monotonicTimestamp));

    // Set timestamp
    if (config.mUseJournalEventTime && entry.realtimeTimestamp > 0) {
        // Journal's realtimeTimestamp is in microseconds, convert to seconds and nanoseconds
        uint64_t seconds = entry.realtimeTimestamp / 1000000;
        uint64_t nanoseconds = (entry.realtimeTimestamp % 1000000) * 1000;
        logEvent->SetTimestamp(seconds, nanoseconds);
    } else {
        // Use current time (keep nanosecond precision, apply second-level time auto-adjust)
        auto currentTime = GetCurrentLogtailTime();
        time_t adjustedSeconds = currentTime.tv_sec;
        time_t adjustedNanoSeconds = currentTime.tv_nsec;

        if (AppConfig::GetInstance()->EnableLogTimeAutoAdjust()) {
            adjustedSeconds += GetTimeDelta();
            adjustedNanoSeconds += GetTimeDelta() * 1000000;
            // Time adjusted, time offset applied
        }
        logEvent->SetTimestamp(adjustedSeconds, adjustedNanoSeconds);
    }

    return logEvent;
}

void ClearAccumulatedData(std::shared_ptr<PipelineEventGroup>* accumulatedEventGroup,
                          int* accumulatedEntryCount,
                          std::string* accumulatedFirstCursor) {
    if (accumulatedEventGroup != nullptr) {
        *accumulatedEventGroup = nullptr;
    }
    if (accumulatedEntryCount != nullptr) {
        *accumulatedEntryCount = 0;
    }
    if (accumulatedFirstCursor != nullptr) {
        accumulatedFirstCursor->clear();
    }
}

bool PushEventGroupToQueue(QueueKey queueKey,
                           PipelineEventGroup* eventGroup,
                           int totalEntryCount,
                           const std::string& firstEntryCursor,
                           const std::shared_ptr<JournalReader>& journalReader,
                           const std::string& configName,
                           std::shared_ptr<PipelineEventGroup>* accumulatedEventGroup,
                           int* accumulatedEntryCount,
                           std::string* accumulatedFirstCursor,
                           bool& pushFailed,
                           bool& eventGroupSent) {
    constexpr uint32_t kMaxPushRetries = 100;
    PipelineEventGroup eventGroupToPush = eventGroup->Copy();
    size_t eventCount = eventGroupToPush.GetEvents().size();

    if (!ProcessorRunner::GetInstance()->PushQueue(queueKey, 0, std::move(eventGroupToPush), kMaxPushRetries)) {
        LOG_ERROR(sLogger,
                  ("journal processor failed to push journal entry batch to queue",
                   "queue may be full, will retry on next cycle")("config", configName)("entry_count", totalEntryCount)(
                      "event_count", eventCount));
        if (!firstEntryCursor.empty()) {
            journalReader->SeekCursor(firstEntryCursor);
            journalReader->Previous();
        }
        pushFailed = true;
        return false;
    }
    eventGroupSent = true;
    ClearAccumulatedData(accumulatedEventGroup, accumulatedEntryCount, accumulatedFirstCursor);
    return true;
}

} // anonymous namespace

bool HandleJournalEntries(const string& configName,
                          const JournalConfig& config,
                          const std::shared_ptr<JournalReader>& journalReader,
                          QueueKey queueKey,
                          bool* hasPendingDataOut,
                          std::shared_ptr<PipelineEventGroup>* accumulatedEventGroup,
                          int* accumulatedEntryCount,
                          std::string* accumulatedFirstCursor,
                          bool timeoutTrigger,
                          std::chrono::steady_clock::time_point* lastBatchTimeOut) {
    if (!journalReader || !journalReader->IsOpen()) {
        LOG_WARNING(sLogger,
                    ("journal processor reader is invalid or closed, skipping processing", "")("config", configName));
        if (hasPendingDataOut != nullptr) {
            *hasPendingDataOut = false;
        }
        return false;
    }

    const int maxEntriesPerBatch = config.mMaxEntriesPerBatch;
    int newEntryCount = 0;
    bool pushFailed = false;
    bool eventGroupSent = false;

    try {
        PipelineEventGroup* eventGroup = nullptr;
        std::string firstEntryCursor;
        int totalEntryCount = 0;

        if (accumulatedEventGroup == nullptr) {
            LOG_ERROR(sLogger,
                      ("journal processor accumulatedEventGroup is required",
                       "cannot process without accumulation support")("config", configName));
            if (hasPendingDataOut != nullptr) {
                *hasPendingDataOut = false;
            }
            return false;
        }

        int originalAccumulatedCount = (accumulatedEntryCount != nullptr) ? *accumulatedEntryCount : 0;

        if (*accumulatedEventGroup != nullptr) {
            eventGroup = accumulatedEventGroup->get();
            totalEntryCount = originalAccumulatedCount;
            if (accumulatedFirstCursor != nullptr && !accumulatedFirstCursor->empty()) {
                firstEntryCursor = *accumulatedFirstCursor;
            }
        } else {
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            *accumulatedEventGroup = std::make_shared<PipelineEventGroup>(sourceBuffer);
            eventGroup = accumulatedEventGroup->get();
            totalEntryCount = 0;
        }

        while (totalEntryCount + newEntryCount < maxEntriesPerBatch) {
            if (!MoveToNextJournalEntry(configName, journalReader, config.mCursorSeekFallback, newEntryCount)) {
                break;
            }

            JournalEntry entry;
            if (!ReadAndValidateEntry(configName, journalReader, config.mCursorSeekFallback, entry)) {
                if (entry.fields.empty() && !entry.cursor.empty()) {
                    newEntryCount++;
                    continue;
                }
                break;
            }

            if (totalEntryCount == 0 && newEntryCount == 0 && !entry.cursor.empty()) {
                firstEntryCursor = entry.cursor;
                if (accumulatedFirstCursor != nullptr) {
                    *accumulatedFirstCursor = firstEntryCursor;
                }
            }

            CreateLogEventFromJournal(entry, config, *eventGroup);
            newEntryCount++;
        }

        totalEntryCount += newEntryCount;
        bool noNewData = newEntryCount == 0 && totalEntryCount > 0;
        bool reachedMaxBatch = totalEntryCount >= maxEntriesPerBatch;

        if (totalEntryCount == 0) {
            if (hasPendingDataOut != nullptr) {
                *hasPendingDataOut = false;
            }
            return false;
        }

        if ((timeoutTrigger && totalEntryCount > 0) || reachedMaxBatch) {
            // (1) 必须推送（超时触发或达到最大批处理数量）
            PushEventGroupToQueue(queueKey,
                                  eventGroup,
                                  totalEntryCount,
                                  firstEntryCursor,
                                  journalReader,
                                  configName,
                                  accumulatedEventGroup,
                                  accumulatedEntryCount,
                                  accumulatedFirstCursor,
                                  pushFailed,
                                  eventGroupSent);
        } else if (noNewData) {
            // (2) 应该发送，检查队列状态（没有新数据但有累积数据）
            if (ProcessQueueManager::GetInstance()->IsValidToPush(queueKey)) {
                PushEventGroupToQueue(queueKey,
                                      eventGroup,
                                      totalEntryCount,
                                      firstEntryCursor,
                                      journalReader,
                                      configName,
                                      accumulatedEventGroup,
                                      accumulatedEntryCount,
                                      accumulatedFirstCursor,
                                      pushFailed,
                                      eventGroupSent);
            } else {
                // 队列状态不合适，累积到下一轮
                if (accumulatedEntryCount != nullptr) {
                    *accumulatedEntryCount = totalEntryCount;
                }
                if (hasPendingDataOut != nullptr) {
                    *hasPendingDataOut = true;
                }
                return false;
            }
        } else {
            // (3) 不推送，继续累积
            if (accumulatedEntryCount != nullptr) {
                *accumulatedEntryCount = totalEntryCount;
            }
            LOG_DEBUG(sLogger,
                      ("journal processor accumulating event group", "")("config", configName)("entry_count",
                                                                                               totalEntryCount));
        }

        if (hasPendingDataOut != nullptr) {
            // hasPendingDataOut: 是否有待处理数据
            // - pushFailed: 推送失败，数据仍然存在（累积缓冲区中）
            // - totalEntryCount > 0 && !eventGroupSent: 有数据但没有发送（继续累积）
            // - reachedMaxBatch: 达到最大批处理数量，while循环退出，但journal中可能还有更多entries需要处理
            *hasPendingDataOut = pushFailed || (totalEntryCount > 0 && !eventGroupSent) || reachedMaxBatch;
        }

        if (lastBatchTimeOut != nullptr) {
            auto now = std::chrono::steady_clock::now();
            bool shouldInitializeTime = accumulatedEntryCount != nullptr && *accumulatedEntryCount > 0
                && lastBatchTimeOut->time_since_epoch().count() == 0;
            if (eventGroupSent || shouldInitializeTime) {
                *lastBatchTimeOut = now;
            }
        }

    } catch (const std::exception& e) {
        LOG_ERROR(sLogger,
                  ("journal processor exception during journal entries processing",
                   e.what())("config", configName)("new_entries_processed", newEntryCount));
        if (hasPendingDataOut != nullptr) {
            *hasPendingDataOut = false; // Conservative handling on exception
        }
        ClearAccumulatedData(accumulatedEventGroup, accumulatedEntryCount, accumulatedFirstCursor);
        return false;
    } catch (...) {
        LOG_ERROR(sLogger,
                  ("journal processor unknown exception during journal entries processing",
                   "")("config", configName)("new_entries_processed", newEntryCount));
        if (hasPendingDataOut != nullptr) {
            *hasPendingDataOut = false; // Conservative handling on exception
        }
        ClearAccumulatedData(accumulatedEventGroup, accumulatedEntryCount, accumulatedFirstCursor);
        return false;
    }

    return eventGroupSent;
}

} // namespace logtail

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
 * @brief Attempt to recover from journal errors (e.g., cursor invalidation due to log rotation)
 * @param journalReader journal reader pointer
 * @param configName config name
 * @param cursorSeekFallback cursor fallback position ("head" or "tail")
 * @param errorContext error context description
 * @return true if recovery succeeded, false if recovery failed
 */
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

} // anonymous namespace

void HandleJournalEntries(const string& configName,
                          const JournalConfig& config,
                          const std::shared_ptr<JournalReader>& journalReader,
                          QueueKey queueKey,
                          bool* hasPendingDataOut) {
    if (!journalReader || !journalReader->IsOpen()) {
        LOG_WARNING(sLogger,
                    ("journal processor reader is invalid or closed, skipping processing",
                     "")("config", configName));
        if (hasPendingDataOut != nullptr) {
            *hasPendingDataOut = false;
        }
        return;
    }
    
    int entryCount = 0;
    // Defensive bounds check: ensure maxEntriesPerBatch is in reasonable range
    const int maxEntriesPerBatch = std::max(1, std::min(config.mMaxEntriesPerBatch, 10000)); // Fair parameter

    // Log warning if config value was clamped
    if (config.mMaxEntriesPerBatch != maxEntriesPerBatch) {
        LOG_WARNING(sLogger,
                    ("journal processor maxEntriesPerBatch clamped to safe range",
                     "")("config", configName)("original", config.mMaxEntriesPerBatch)("clamped", maxEntriesPerBatch));
    }

    try {
        // Create a single event group for batch processing
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);

        // Store first entry cursor for error recovery
        std::string firstEntryCursor;
        bool pushFailed = false;

        while (entryCount < maxEntriesPerBatch) {
            if (!MoveToNextJournalEntry(configName, journalReader, config.mCursorSeekFallback, entryCount)) {
                break;
            }

            JournalEntry entry;
            if (!ReadAndValidateEntry(configName, journalReader, config.mCursorSeekFallback, entry)) {
                // Skip if entry is empty
                if (entry.fields.empty() && !entry.cursor.empty()) {
                    entryCount++;
                    continue;
                }
                // Connection error or read failure
                break;
            }

            // Store first entry cursor for potential rollback
            if (entryCount == 0 && !entry.cursor.empty()) {
                firstEntryCursor = entry.cursor;
            }

            // Add log event to the batch event group
            CreateLogEventFromJournal(entry, config, eventGroup);
            entryCount++;
        }

        // Push the batch event group if it contains any events
        if (entryCount > 0) {
            // Use ProcessorRunner's built-in retry mechanism
            constexpr uint32_t kMaxPushRetries = 100;

            if (!ProcessorRunner::GetInstance()->PushQueue(queueKey, 0, std::move(eventGroup), kMaxPushRetries)) {
                LOG_ERROR(
                    sLogger,
                    ("journal processor failed to push journal entry batch to queue",
                     "queue may be full, will retry on next cycle")("config", configName)("entry_count", entryCount));
                if (!firstEntryCursor.empty()) { // seek to first entry and later retry
                    journalReader->SeekCursor(firstEntryCursor);
                    journalReader->Previous();
                }
                pushFailed = true;
            }
        }

        // Determine if there is pending data:
        // If exited due to batch limit (entryCount == maxEntriesPerBatch), there is pending data
        // If exited due to push failure (pushFailed == true), there is pending data to retry
        // If exited normally (no pending data), reading is complete
        if (hasPendingDataOut != nullptr) {
            *hasPendingDataOut = pushFailed || (entryCount == maxEntriesPerBatch);
        }

        // Only log warning when no entries processed and there might be a problem
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
            *hasPendingDataOut = false; // Conservative handling on exception
        }
    } catch (...) {
        LOG_ERROR(sLogger,
                  ("journal processor unknown exception during journal entries processing",
                   "")("config", configName)("entries_processed", entryCount));
        if (hasPendingDataOut != nullptr) {
            *hasPendingDataOut = false; // Conservative handling on exception
        }
    }
}

} // namespace logtail

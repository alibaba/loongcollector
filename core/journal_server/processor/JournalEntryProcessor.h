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

#include <chrono>
#include <memory>
#include <string>

#include "common/JournalConfig.h"
#include "reader/JournalReader.h"

namespace logtail {

struct JournalConfig;
class JournalReader;
using QueueKey = int64_t;

/**
 * @brief Read and process journal entries
 *
 * @param configName Configuration name
 * @param config Journal configuration
 * @param journalReader Journal reader pointer
 * @param queueKey Queue key for pushing events
 * @param timeoutTrigger Whether timeout triggers forced sending
 * @param accumulatedEventGroup Input/output parameter for accumulated eventGroup (may be nullptr)
 * @param accumulatedEntryCount Input/output parameter for accumulated entry count (may be nullptr)
 * @param hasPendingDataOut Output parameter indicating if there is pending data (may be nullptr)
 * @param lastBatchTimeOut Input/output parameter for last batch processing time (may be nullptr)
 * @return Whether eventGroup was sent (true=sent, false=accumulated to buffer)
 */
bool HandleJournalEntries(const std::string& configName,
                          const JournalConfig& config,
                          const std::shared_ptr<JournalReader>& journalReader,
                          QueueKey queueKey,
                          bool timeoutTrigger,
                          std::shared_ptr<PipelineEventGroup>* accumulatedEventGroup,
                          int* accumulatedEntryCount,
                          bool* hasPendingDataOut,
                          std::chrono::steady_clock::time_point* lastBatchTimeOut);

} // namespace logtail

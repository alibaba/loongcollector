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

namespace logtail {

// Forward declarations
class JournalReader;
class PipelineEventGroup;

/**
 * @brief Monitored reader structure
 *
 * Tracks the state of a journal reader being monitored by JournalServer:
 * - Reader reference and configuration name
 * - Pending data state and accumulated batch processing data
 * - Batch processing timing and entry count
 */
struct MonitoredReader {
    std::shared_ptr<JournalReader> reader;
    std::string configName;
    bool hasPendingData{true};
    std::shared_ptr<PipelineEventGroup> accumulatedEventGroup{nullptr};
    std::chrono::steady_clock::time_point lastBatchTime{};
    int accumulatedEntryCount{0};
    std::string accumulatedFirstCursor;
};

} // namespace logtail


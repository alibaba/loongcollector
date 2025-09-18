/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <string>
#include <vector>
#include "collection_pipeline/queue/QueueKey.h"

namespace logtail {

// Forward declaration
class CollectionPipelineContext;

/**
 * @brief Configuration for journal input plugin
 */
struct JournalConfig {
    // Journal reading configuration
    std::string seekPosition;
    int cursorFlushPeriodMs = 5000;
    std::string cursorSeekFallback;

    // Filter configuration
    std::vector<std::string> units;           // Systemd unit filter
    std::vector<std::string> identifiers;    // Syslog identifier filter  
    std::vector<std::string> matchPatterns;  // Custom match patterns
    bool kernel = true;                             // Enable kernel log filter

    // Performance configuration
    int resetIntervalSecond = 3600;          // Connection reset interval
    int maxEntriesPerBatch = 1000;           // Max entries per batch
    int waitTimeoutMs = 1000;                // Wait timeout in milliseconds

    // Field processing configuration
    bool parsePriority = false;              // Parse priority field
    bool parseSyslogFacility = false;       // Parse syslog facility field
    bool useJournalEventTime = true;        // Use journal event time instead of system time

    // Custom journal path (for file-based journals)
    std::vector<std::string> journalPaths;

    // Context reference
    const CollectionPipelineContext* ctx = nullptr;
    
    // Runtime state (set during validation)
    mutable QueueKey queueKey = -1;  // Cached queue key after validation (-1 = not validated)
    mutable std::string lastSeekCheckpoint;  // Last checkpoint we successfully seeked to
    mutable bool needsSeek = true;  // Whether we need to perform seek on next read
    
    JournalConfig() = default;
};

} // namespace logtail 
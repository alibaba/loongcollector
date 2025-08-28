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

namespace logtail {

// Forward declaration
class CollectionPipelineContext;

/**
 * @brief Configuration for journal input plugin
 */
struct JournalConfig {
    // Journal reading configuration
    std::string seekPosition;
    int cursorFlushPeriodMs;
    std::string cursorSeekFallback;

    // Filter configuration
    std::vector<std::string> units;           // Systemd unit filter
    std::vector<std::string> identifiers;    // Syslog identifier filter  
    std::vector<std::string> matchPatterns;  // Custom match patterns
    bool kernel;                             // Enable kernel log filter

    // Performance configuration
    int resetIntervalSecond;                 // Connection reset interval
    int maxEntriesPerBatch;                  // Max entries per batch
    int waitTimeoutMs;                       // Wait timeout in milliseconds

    // Field processing configuration
    bool parsePriority;                      // Parse priority field
    bool parseSyslogFacility;               // Parse syslog facility field
    bool useJournalEventTime;               // Use journal event time instead of system time

    // Custom journal path (for file-based journals)
    std::vector<std::string> journalPaths;

    // Context reference
    const CollectionPipelineContext* ctx;
    
    JournalConfig() : cursorFlushPeriodMs(5000), kernel(false), resetIntervalSecond(3600),
                     maxEntriesPerBatch(1000), waitTimeoutMs(1000),
                     parsePriority(false), parseSyslogFacility(false), useJournalEventTime(true), ctx(nullptr) {
        // Note: kernel默认为false，这样默认采集所有日志而不仅仅是内核日志
        // 用户可以根据需要手动启用kernel过滤器
        // maxEntriesPerBatch默认1000，waitTimeoutMs默认1000ms
    }
};

} // namespace logtail 
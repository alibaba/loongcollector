/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
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
#include <map>
#include <string>
#include <cstdint>

namespace logtail {

/**
 * @brief Represents a single journal entry with all its fields and metadata
 */
class JournalEntry {
public:
    JournalEntry() = default;
    ~JournalEntry() = default;

    // Fields map (key-value pairs from journal)
    std::map<std::string, std::string> fields;
    
    // Journal metadata
    std::string cursor;
    uint64_t realtimeTimestamp;
    uint64_t monotonicTimestamp;
    
    // Helper methods
    bool HasField(const std::string& key) const;
    std::string GetField(const std::string& key, const std::string& defaultValue = "") const;
    void SetField(const std::string& key, const std::string& value);
    
    // Timestamp conversion
    std::chrono::system_clock::time_point GetRealtimeTimestamp() const;
    std::chrono::steady_clock::time_point GetMonotonicTimestamp() const;
};

} // namespace logtail 
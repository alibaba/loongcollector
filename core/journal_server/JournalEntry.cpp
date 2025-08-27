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

#include "journal_server/JournalEntry.h"

#include <chrono>

namespace logtail {

bool JournalEntry::HasField(const std::string& key) const {
    return fields.find(key) != fields.end();
}

std::string JournalEntry::GetField(const std::string& key, const std::string& defaultValue) const {
    auto it = fields.find(key);
    if (it != fields.end()) {
        return it->second;
    }
    return defaultValue;
}

void JournalEntry::SetField(const std::string& key, const std::string& value) {
    fields[key] = value;
}

std::chrono::system_clock::time_point JournalEntry::GetRealtimeTimestamp() const {
    // Convert microseconds to system_clock time_point
    auto duration = std::chrono::microseconds(realtimeTimestamp);
    return std::chrono::system_clock::time_point(duration);
}

std::chrono::steady_clock::time_point JournalEntry::GetMonotonicTimestamp() const {
    // Convert microseconds to steady_clock time_point
    auto duration = std::chrono::microseconds(monotonicTimestamp);
    return std::chrono::steady_clock::time_point(duration);
}

} // namespace logtail 
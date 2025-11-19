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

#include <cstdint>

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace logtail {

struct JournalEntry {
    JournalEntry() = default;
    std::map<std::string, std::string> fields;
    std::string cursor;
    uint64_t realtimeTimestamp = 0;
    uint64_t monotonicTimestamp = 0;
};

enum class JournalReadStatus {
    kOk = 1, // Successfully moved to next entry with data
    kEndOfJournal = 0, // No more entries (reached end)
    kError = -1 // Error occurred (e.g., cursor invalidated by log rotation)
};
enum class JournalStatusType {
    kNop = 0, // No change (SD_JOURNAL_NOP)
    kAppend = 1, // New entries added (SD_JOURNAL_APPEND)
    kInvalidate = 2, // Log rotated or invalidated (SD_JOURNAL_INVALIDATE)
    kError = -1 // Error occurred
};
class JournalReader {
public:
    JournalReader();
    ~JournalReader();
    JournalReader(const JournalReader&) = delete;
    JournalReader& operator=(const JournalReader&) = delete;
    JournalReader(JournalReader&&) = delete;
    JournalReader& operator=(JournalReader&&) = delete;

    bool Open();
    void Close();
    bool IsOpen() const;

    bool SeekHead();
    bool SeekTail();
    bool SeekCursor(const std::string& cursor);
    bool Next();
    bool Previous();
    JournalReadStatus NextWithStatus();

    bool GetEntry(JournalEntry& entry);
    std::string GetCursor();

    bool AddMatch(const std::string& field, const std::string& value);
    bool AddDisjunction();

    std::vector<std::string> GetUniqueValues(const std::string& field);

    bool SetJournalPaths(const std::vector<std::string>& paths);

    bool AddToEpoll(int epollFD);
    int AddToEpollAndGetFD(int epollFD);
    void RemoveFromEpoll(int epollFD);
    JournalStatusType CheckJournalStatus();

    int GetJournalFD() const;

private:
    class Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace logtail

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

/**
 * @brief Represents a single journal entry with all its fields and metadata
 */
struct JournalEntry {
    JournalEntry() = default;

    // Fields map (key-value pairs from journal)
    std::map<std::string, std::string> fields;

    // Journal metadata
    std::string cursor;
    uint64_t realtimeTimestamp = 0;
    uint64_t monotonicTimestamp = 0;
};

/**
 * @brief Journal navigation result status
 */
enum class JournalReadStatus {
    kOk = 1, // Successfully moved to next entry with data
    kEndOfJournal = 0, // No more entries (reached end)
    kError = -1 // Error occurred (e.g., cursor invalidated by log rotation)
};

/**
 * @brief Journal status types returned by CheckJournalStatus()
 */
enum class JournalStatusType {
    kNop = 0, // No change (SD_JOURNAL_NOP)
    kAppend = 1, // New entries added (SD_JOURNAL_APPEND)
    kInvalidate = 2, // Log rotated or invalidated (SD_JOURNAL_INVALIDATE)
    kError = -1 // Error occurred
};

/**
 * @brief Journal reader implementation using sd-journal
 */
class JournalReader {
public:
    JournalReader();
    ~JournalReader();

    // Delete copy and move operations
    JournalReader(const JournalReader&) = delete;
    JournalReader& operator=(const JournalReader&) = delete;
    JournalReader(JournalReader&&) = delete;
    JournalReader& operator=(JournalReader&&) = delete;

    // Core journal operations
    bool Open();
    void Close();
    bool IsOpen() const;

    // Seeking operations
    bool SeekHead();
    bool SeekTail();
    bool SeekCursor(const std::string& cursor);
    bool Next();
    bool Previous();

    /**
     * @brief Move to next entry with detailed status
     * @return JournalReadStatus indicating success, end of journal, or error
     */
    JournalReadStatus NextWithStatus();

    // Reading operations
    bool GetEntry(JournalEntry& entry);
    std::string GetCursor();

    // Filtering operations
    bool AddMatch(const std::string& field, const std::string& value);
    bool AddDisjunction();

    // Get unique values for a field (for glob pattern matching)
    std::vector<std::string> GetUniqueValues(const std::string& field);

    // Journal paths
    bool SetJournalPaths(const std::vector<std::string>& paths);

    // 事件监听相关方法（仅在 Linux 平台可用）
    bool AddToEpoll(int epollFD);
    
    /**
     * @brief 将journal添加到epoll监控并返回文件描述符（原子操作）
     * @param epollFD epoll文件描述符
     * @return 成功返回journal FD（>=0），失败返回-1
     */
    int AddToEpollAndGetFD(int epollFD);
    
    void RemoveFromEpoll(int epollFD);

    /**
     * @brief 检查 journal 状态变化
     * @return JournalStatusType 指示状态类型
     */
    JournalStatusType CheckJournalStatus();

    int GetJournalFD() const;

private:
    class Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace logtail

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
 * @brief Abstract interface for journal reading operations
 * This allows for different implementations (systemd-journal, mock for testing, etc.)
 */
/**
 * @brief Journal navigation result status
 */
enum class JournalReadStatus {
    kOk = 1, // Successfully moved to next entry with data
    kEndOfJournal = 0, // No more entries (reached end)
    kError = -1 // Error occurred (e.g., cursor invalidated by log rotation)
};

/**
 * @brief Journal event types returned by ProcessJournalEvent()
 */
enum class JournalEventType {
    kNop = 0, // No change (SD_JOURNAL_NOP)
    kAppend = 1, // New entries added (SD_JOURNAL_APPEND)
    kInvalidate = 2, // Log rotated or invalidated (SD_JOURNAL_INVALIDATE)
    kError = -1 // Error occurred
};

class JournalReader {
public:
    // Add default constructor
    JournalReader() = default;
    virtual ~JournalReader() = default;

    // Default copy and move operations for interface
    JournalReader(const JournalReader&) = default;
    JournalReader& operator=(const JournalReader&) = default;
    JournalReader(JournalReader&&) = default;
    JournalReader& operator=(JournalReader&&) = default;

    // Core journal operations
    virtual bool Open() = 0;
    virtual void Close() = 0;
    virtual bool IsOpen() const = 0;

    // Seeking operations
    virtual bool SeekHead() = 0;
    virtual bool SeekTail() = 0;
    virtual bool SeekCursor(const std::string& cursor) = 0;
    virtual bool Next() = 0;
    virtual bool Previous() = 0;

    /**
     * @brief Move to next entry with detailed status
     * @return JournalReadStatus indicating success, end of journal, or error
     */
    virtual JournalReadStatus NextWithStatus() = 0;

    // Reading operations
    virtual bool GetEntry(JournalEntry& entry) = 0;
    virtual std::string GetCursor() = 0;

    // Filtering operations
    virtual bool AddMatch(const std::string& field, const std::string& value) = 0;
    virtual bool AddDisjunction() = 0;

    // Get unique values for a field (for glob pattern matching)
    virtual std::vector<std::string> GetUniqueValues(const std::string& field) = 0;

    // Journal paths
    virtual bool SetJournalPaths(const std::vector<std::string>& paths) = 0;
};

/**
 * @brief Systemd journal implementation using sd-journal
 */
class SystemdJournalReader : public JournalReader {
public:
    SystemdJournalReader();
    ~SystemdJournalReader() override;

    // Delete copy and move operations
    SystemdJournalReader(const SystemdJournalReader&) = delete;
    SystemdJournalReader& operator=(const SystemdJournalReader&) = delete;
    SystemdJournalReader(SystemdJournalReader&&) = delete;
    SystemdJournalReader& operator=(SystemdJournalReader&&) = delete;

    // JournalReader interface implementation
    bool Open() override;
    void Close() override;
    bool IsOpen() const override;

    bool SeekHead() override;
    bool SeekTail() override;
    bool SeekCursor(const std::string& cursor) override;
    bool Next() override;
    bool Previous() override;
    JournalReadStatus NextWithStatus() override;

    bool GetEntry(JournalEntry& entry) override;
    std::string GetCursor() override;

    bool AddMatch(const std::string& field, const std::string& value) override;
    bool AddDisjunction() override;

    std::vector<std::string> GetUniqueValues(const std::string& field) override;

    bool SetJournalPaths(const std::vector<std::string>& paths) override;

    // 事件监听相关方法（仅在 Linux 平台可用）
    bool AddToEpoll(int epollFD);
    void RemoveFromEpoll(int epollFD);

    /**
     * @brief 处理 journal 事件
     * @return JournalEventType 指示事件类型
     */
    JournalEventType ProcessJournalEvent();
    int GetJournalFD() const;

private:
    class Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace logtail

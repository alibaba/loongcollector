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
#include <vector>

#include "JournalEntry.h"

namespace logtail {

/**
 * @brief Abstract interface for journal reading operations
 * This allows for different implementations (systemd-journal, mock for testing, etc.)
 */
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
    
    // Reading operations
    virtual bool GetEntry(JournalEntry& entry) = 0;
    virtual std::string GetCursor() = 0;
    
    // Filtering operations
    virtual bool AddMatch(const std::string& field, const std::string& value) = 0;
    virtual bool AddDisjunction() = 0;
    
    // Get unique values for a field (for glob pattern matching)
    virtual std::vector<std::string> GetUniqueValues(const std::string& field) = 0;
    
    // Event waiting
    virtual int Wait(std::chrono::milliseconds timeout) = 0;
    
    // Configuration
    virtual bool SetDataThreshold(size_t threshold) = 0;
    virtual bool SetTimeout(std::chrono::milliseconds timeout) = 0;
    
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
    
    bool GetEntry(JournalEntry& entry) override;
    std::string GetCursor() override;
    
    bool AddMatch(const std::string& field, const std::string& value) override;
    bool AddDisjunction() override;
    
    std::vector<std::string> GetUniqueValues(const std::string& field) override;
    
    int Wait(std::chrono::milliseconds timeout) override;
    
    bool SetDataThreshold(size_t threshold) override;
    bool SetTimeout(std::chrono::milliseconds timeout) override;
    
    bool SetJournalPaths(const std::vector<std::string>& paths) override;

private:
    class Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace logtail 
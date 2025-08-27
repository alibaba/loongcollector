/*
 * Copyright 2024 iLogtail Authors
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

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "collection_pipeline/plugin/interface/Input.h"
#include "json/json.h"

namespace logtail {

// Forward declarations
class JournalEntry;

class InputJournal : public Input {
public:
    static const std::string sName;

    InputJournal();
    ~InputJournal();

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override;
    bool Start() override;
    bool Stop(bool isPipelineRemoving) override;
    bool SupportAck() const override { return true; }

private:
    /**
     * @brief Abstract interface for journal reading operations
     * This allows for different implementations (systemd-journal, mock for testing, etc.)
     */
    class JournalReader {
    public:
        virtual ~JournalReader() = default;

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
        
        // Event waiting
        virtual int Wait(std::chrono::milliseconds timeout) = 0;
        
        // Configuration
        virtual bool SetDataThreshold(size_t threshold) = 0;
        virtual bool SetTimeout(std::chrono::milliseconds timeout) = 0;
    };

    /**
     * @brief Systemd journal implementation using sd-journal
     */
    class SystemdJournalReader : public JournalReader {
    public:
        SystemdJournalReader();
        ~SystemdJournalReader() override;

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
        
        int Wait(std::chrono::milliseconds timeout) override;
        
        bool SetDataThreshold(size_t threshold) override;
        bool SetTimeout(std::chrono::milliseconds timeout) override;

    private:
        class Impl;
        std::unique_ptr<Impl> mImpl;
    };

    // Configuration options
    std::string mSeekPosition;
    int mCursorFlushPeriodMs;
    std::string mCursorSeekFallback;
    std::vector<std::string> mUnits;
    bool mKernel;
    std::vector<std::string> mIdentifiers;
    std::vector<std::string> mJournalPaths;
    std::vector<std::string> mMatchPatterns;
    bool mParseSyslogFacility;
    bool mParsePriority;
    bool mUseJournalEventTime;
    int mResetIntervalSecond;

    // Runtime state
    std::unique_ptr<JournalReader> mJournalReader;
    
    // Threading
    std::atomic<bool> mShutdown;
    // 不再需要线程管理，JournalServer 会处理所有数据

    // Constants
    static constexpr int DEFAULT_RESET_INTERVAL = 3600; // 1 hour
    static constexpr int DEFAULT_CURSOR_FLUSH_PERIOD_MS = 5000; // 5 seconds

    // 不再需要这些辅助方法，JournalServer 会处理所有 journal 操作
    
    // Seek position constants
    static const std::string SEEK_POSITION_CURSOR;
    static const std::string SEEK_POSITION_HEAD;
    static const std::string SEEK_POSITION_TAIL;
    static const std::string SEEK_POSITION_DEFAULT;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class InputJournalUnittest;
#endif
};

} // namespace logtail 
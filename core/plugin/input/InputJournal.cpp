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

#include "InputJournal.h"
#include "JournalEntry.h"

#include <chrono>
#include <thread>
#include <map>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <filesystem>

// Systemd journal headers
#ifdef __linux__
#include <systemd/sd-journal.h>
#endif

#include "common/ParamExtractor.h"
#include "common/FileSystemUtil.h"
#include "journal_server/JournalServer.h"
#include "logger/Logger.h"
#include "app_config/AppConfig.h"

namespace logtail {

const std::string InputJournal::sName = "input_journal";

// Static constants
const std::string InputJournal::SEEK_POSITION_CURSOR = "cursor";
const std::string InputJournal::SEEK_POSITION_HEAD = "head";
const std::string InputJournal::SEEK_POSITION_TAIL = "tail";
const std::string InputJournal::SEEK_POSITION_DEFAULT = "none";

InputJournal::InputJournal()
    : mSeekPosition(SEEK_POSITION_TAIL)
    , mCursorFlushPeriodMs(DEFAULT_CURSOR_FLUSH_PERIOD_MS)
    , mCursorSeekFallback(SEEK_POSITION_TAIL)
    , mKernel(true)
    , mParseSyslogFacility(false)
    , mParsePriority(false)
    , mUseJournalEventTime(false)
    , mResetIntervalSecond(DEFAULT_RESET_INTERVAL)
    , mShutdown(false) {
}

InputJournal::~InputJournal() {
    Stop(true);
}

bool InputJournal::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    (void)optionalGoPipeline; // Suppress unused parameter warning
    std::string errorMsg;
    
    // Parse configuration
    if (!GetOptionalStringParam(config, "SeekPosition", mSeekPosition, errorMsg)) {
        mSeekPosition = SEEK_POSITION_TAIL;
    }
    
    if (!GetOptionalIntParam(config, "CursorFlushPeriodMs", mCursorFlushPeriodMs, errorMsg)) {
        mCursorFlushPeriodMs = DEFAULT_CURSOR_FLUSH_PERIOD_MS;
    }
    
    if (!GetOptionalStringParam(config, "CursorSeekFallback", mCursorSeekFallback, errorMsg)) {
        mCursorSeekFallback = SEEK_POSITION_TAIL;
    }
    
    if (!GetOptionalBoolParam(config, "Kernel", mKernel, errorMsg)) {
        mKernel = true;
    }
    
    if (!GetOptionalBoolParam(config, "ParseSyslogFacility", mParseSyslogFacility, errorMsg)) {
        mParseSyslogFacility = false;
    }
    
    if (!GetOptionalBoolParam(config, "ParsePriority", mParsePriority, errorMsg)) {
        mParsePriority = false;
    }
    
    if (!GetOptionalBoolParam(config, "UseJournalEventTime", mUseJournalEventTime, errorMsg)) {
        mUseJournalEventTime = false;
    }
    
    if (!GetOptionalIntParam(config, "ResetIntervalSecond", mResetIntervalSecond, errorMsg)) {
        mResetIntervalSecond = DEFAULT_RESET_INTERVAL;
    }
    
    // Parse arrays
    if (config.isMember("Units") && config["Units"].isArray()) {
        for (const auto& unit : config["Units"]) {
            if (unit.isString()) {
                mUnits.push_back(unit.asString());
            }
        }
    }
    
    if (config.isMember("Identifiers") && config["Identifiers"].isArray()) {
        for (const auto& identifier : config["Identifiers"]) {
            if (identifier.isString()) {
                mIdentifiers.push_back(identifier.asString());
            }
        }
    }
    
    if (config.isMember("JournalPaths") && config["JournalPaths"].isArray()) {
        for (const auto& path : config["JournalPaths"]) {
            if (path.isString()) {
                mJournalPaths.push_back(path.asString());
            }
        }
    }
    
    if (config.isMember("MatchPatterns") && config["MatchPatterns"].isArray()) {
        for (const auto& pattern : config["MatchPatterns"]) {
            if (pattern.isString()) {
                mMatchPatterns.push_back(pattern.asString());
            }
        }
    }
    
    return true;
}

bool InputJournal::Start() {
    if (mShutdown) {
        return false;
    }
    
    // Create journal configuration for JournalServer
    JournalConfig config;
    config.seekPosition = mSeekPosition;
    config.cursorFlushPeriodMs = mCursorFlushPeriodMs;
    config.cursorSeekFallback = mCursorSeekFallback;
    config.units = mUnits;
    config.kernel = mKernel;
    config.identifiers = mIdentifiers;
    config.journalPaths = mJournalPaths;
    config.resetIntervalSecond = mResetIntervalSecond;
    config.ctx = mContext;
    
    // Register with JournalServer
    JournalServer::GetInstance()->AddJournalInput(
        mContext->GetConfigName(), 
        mIndex, 
        config);
    
    // JournalServer 会处理所有数据，不再需要自己的主线程
    return true;
}

bool InputJournal::Stop(bool isPipelineRemoving) {
    (void)isPipelineRemoving; // Suppress unused parameter warning
    if (mShutdown) {
        return true;
    }
    
    // Unregister from JournalServer
    JournalServer::GetInstance()->RemoveJournalInput(mContext->GetConfigName(), mIndex);
    
    mShutdown = true;
    
    // 不再需要等待线程结束，JournalServer 会处理清理工作
    if (mJournalReader) {
        mJournalReader->Close();
    }
    
    return true;
}

// SystemdJournalReader::Impl implementation
class InputJournal::SystemdJournalReader::Impl {
public:
    Impl() : mIsOpen(false), mJournal(nullptr), mDataThreshold(64 * 1024), mTimeout(1000) {}
    
    ~Impl() {
        Close();
    }
    
    bool Open() {
#ifdef __linux__
        if (mIsOpen) {
            return true; // Already open
        }
        
        // Open the journal
        int ret = sd_journal_open(&mJournal, SD_JOURNAL_LOCAL_ONLY);
        if (ret < 0) {
            LOG_ERROR(sLogger, ("failed to open journal", std::string(strerror(-ret))));
            return false;
        }
        
        // Set data threshold
        ret = sd_journal_set_data_threshold(mJournal, mDataThreshold);
        if (ret < 0) {
            // Continue anyway, this is not critical
        }
        
        mIsOpen = true;
        return true;
#else
        // On non-Linux systems, simulate success
        mIsOpen = true;
        return true;
#endif
    }
    
    void Close() {
#ifdef __linux__
        if (mJournal) {
            sd_journal_close(mJournal);
            mJournal = nullptr;
        }
#endif
        mIsOpen = false;
    }
    
    bool IsOpen() const {
        return mIsOpen && mJournal != nullptr;
    }
    
    bool SeekHead() {
#ifdef __linux__
        if (!IsOpen()) return false;
        
        int ret = sd_journal_seek_head(mJournal);
        if (ret < 0) {
            LOG_ERROR(sLogger, ("failed to seek to head", strerror(-ret)));
            return false;
        }
        return true;
#else
        return mIsOpen;
#endif
    }
    
    bool SeekTail() {
#ifdef __linux__
        if (!IsOpen()) return false;
        
        int ret = sd_journal_seek_tail(mJournal);
        if (ret < 0) {
            LOG_ERROR(sLogger, ("failed to seek to tail", strerror(-ret)));
            return false;
        }
        return true;
#else
        return mIsOpen;
#endif
    }
    
    bool SeekCursor(const std::string& cursor) {
#ifdef __linux__
        if (!IsOpen()) return false;
        
        int ret = sd_journal_seek_cursor(mJournal, cursor.c_str());
        if (ret < 0) {
            LOG_ERROR(sLogger, ("failed to seek to cursor", cursor)("error", strerror(-ret)));
            return false;
        }
        return true;
#else
        return mIsOpen;
#endif
    }
    
    bool Next() {
#ifdef __linux__
        if (!IsOpen()) return false;
        
        int ret = sd_journal_next(mJournal);
        if (ret < 0) {
            LOG_ERROR(sLogger, ("failed to move to next entry", strerror(-ret)));
            return false;
        }
        return ret > 0; // ret > 0 means we have a next entry
#else
        return mIsOpen;
#endif
    }
    
    bool Previous() {
#ifdef __linux__
        if (!IsOpen()) return false;
        
        int ret = sd_journal_previous(mJournal);
        if (ret < 0) {
            LOG_ERROR(sLogger, ("failed to move to previous entry", strerror(-ret)));
            return false;
        }
        return ret > 0; // ret > 0 means we have a previous entry
#else
        return mIsOpen;
#endif
    }
    
    bool GetEntry(JournalEntry& entry) {
#ifdef __linux__
        if (!IsOpen()) return false;
        
        // Clear previous entry data
        entry.fields.clear();
        
        // Get cursor
        char* cursor;
        int ret = sd_journal_get_cursor(mJournal, &cursor);
        if (ret < 0) {
            LOG_ERROR(sLogger, ("failed to get cursor", strerror(-ret)));
            return false;
        }
        entry.cursor = cursor;
        
        // Get realtime timestamp
        uint64_t realtime;
        ret = sd_journal_get_realtime_usec(mJournal, &realtime);
        if (ret < 0) {
            realtime = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }
        entry.realtimeTimestamp = realtime;
        
        // Get monotonic timestamp
        uint64_t monotonic;
        ret = sd_journal_get_monotonic_usec(mJournal, &monotonic, nullptr);
        if (ret < 0) {
            monotonic = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }
        entry.monotonicTimestamp = monotonic;
        
        // Iterate through all fields
        const void* data;
        size_t length;
        const char* field;
        int r = sd_journal_enumerate_data(mJournal, &data, &length);
        while (r > 0) {
            field = static_cast<const char*>(data);
            std::string fieldStr(field);
            std::string valueStr(static_cast<const char*>(data) + strlen(field) + 1, length - strlen(field) - 1);
            entry.fields[fieldStr] = valueStr;
            r = sd_journal_enumerate_data(mJournal, &data, &length);
        }
        
        return true;
#else
        // On non-Linux systems, simulate entry
        if (!mIsOpen) return false;
        
        entry.cursor = "simulated_cursor";
        entry.realtimeTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        entry.monotonicTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        
        entry.fields.clear();
        entry.fields["MESSAGE"] = "Simulated journal entry";
        entry.fields["_SYSTEMD_UNIT"] = "simulated.service";
        entry.fields["PRIORITY"] = "6";
        entry.fields["SYSLOG_FACILITY"] = "3";
        
        return true;
#endif
    }
    
    std::string GetCursor() {
#ifdef __linux__
        if (!IsOpen()) return "";
        
        char* cursor;
        int ret = sd_journal_get_cursor(mJournal, &cursor);
        if (ret < 0) {
            LOG_ERROR(sLogger, ("failed to get cursor", strerror(-ret)));
            return "";
        }
        return cursor;
#else
        return mIsOpen ? "simulated_cursor" : "";
#endif
    }
    
    bool AddMatch(const std::string& field, const std::string& value) {
#ifdef __linux__
        if (!IsOpen()) return false;
        
        // 组合字段和值为 "FIELD=value" 格式
        std::string matchString = field + "=" + value;
        int ret = sd_journal_add_match(mJournal, matchString.c_str(), matchString.length());
        if (ret < 0) {
            LOG_ERROR(sLogger, ("failed to add match", field)("value", value)("error", strerror(-ret)));
            return false;
        }
        return true;
#else
        return mIsOpen;
#endif
    }
    
    bool AddDisjunction() {
#ifdef __linux__
        if (!IsOpen()) return false;
        
        int ret = sd_journal_add_disjunction(mJournal);
        if (ret < 0) {
            LOG_ERROR(sLogger, ("failed to add disjunction", strerror(-ret)));
            return false;
        }
        return true;
#else
        return mIsOpen;
#endif
    }
    
    int Wait(std::chrono::milliseconds timeout) {
#ifdef __linux__
        if (!IsOpen()) return -1;
        
        uint64_t timeout_us = std::chrono::duration_cast<std::chrono::microseconds>(timeout).count();
        int ret = sd_journal_wait(mJournal, timeout_us);
        if (ret < 0) {
            LOG_ERROR(sLogger, ("failed to wait for journal events", strerror(-ret)));
            return -1;
        }
        return ret; // SD_JOURNAL_NOP, SD_JOURNAL_APPEND, SD_JOURNAL_INVALIDATE
#else
        // On non-Linux systems, simulate waiting
        if (!mIsOpen) return -1;
        std::this_thread::sleep_for(timeout);
        return 1; // SD_JOURNAL_APPEND
#endif
    }
    
    bool SetDataThreshold(size_t threshold) {
        mDataThreshold = threshold;
#ifdef __linux__
        if (IsOpen()) {
            int ret = sd_journal_set_data_threshold(mJournal, mDataThreshold);
            if (ret < 0) {
                return false;
            }
        }
#endif
        return true;
    }
    
    bool SetTimeout(std::chrono::milliseconds timeout) {
        mTimeout = std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();
        return true;
    }
    
private:
    bool mIsOpen;
#ifdef __linux__
    sd_journal* mJournal;
#endif
    size_t mDataThreshold;
    int mTimeout;
};

// JournalReader implementation
InputJournal::SystemdJournalReader::SystemdJournalReader() : mImpl(std::make_unique<Impl>()) {}

InputJournal::SystemdJournalReader::~SystemdJournalReader() = default;

bool InputJournal::SystemdJournalReader::Open() { return mImpl->Open(); }
void InputJournal::SystemdJournalReader::Close() { mImpl->Close(); }
bool InputJournal::SystemdJournalReader::IsOpen() const { return mImpl->IsOpen(); }
bool InputJournal::SystemdJournalReader::SeekHead() { return mImpl->SeekHead(); }
bool InputJournal::SystemdJournalReader::SeekTail() { return mImpl->SeekTail(); }
bool InputJournal::SystemdJournalReader::SeekCursor(const std::string& cursor) { return mImpl->SeekCursor(cursor); }
bool InputJournal::SystemdJournalReader::Next() { return mImpl->Next(); }
bool InputJournal::SystemdJournalReader::Previous() { return mImpl->Previous(); }
bool InputJournal::SystemdJournalReader::GetEntry(JournalEntry& entry) { return mImpl->GetEntry(entry); }
std::string InputJournal::SystemdJournalReader::GetCursor() { return mImpl->GetCursor(); }
bool InputJournal::SystemdJournalReader::AddMatch(const std::string& field, const std::string& value) { return mImpl->AddMatch(field, value); }
bool InputJournal::SystemdJournalReader::AddDisjunction() { return mImpl->AddDisjunction(); }
int InputJournal::SystemdJournalReader::Wait(std::chrono::milliseconds timeout) { return mImpl->Wait(timeout); }
bool InputJournal::SystemdJournalReader::SetDataThreshold(size_t threshold) { return mImpl->SetDataThreshold(threshold); }
bool InputJournal::SystemdJournalReader::SetTimeout(std::chrono::milliseconds timeout) { return mImpl->SetTimeout(timeout); }

} // namespace logtail 
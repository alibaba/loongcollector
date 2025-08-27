/*
 * Copyright 2025 iLogtail Authors
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

#include "journal_server/JournalReader.h"

#include <chrono>
#include <thread>
#include <map>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <filesystem>

#include "logger/Logger.h"

// Systemd journal headers
#include <systemd/sd-journal.h>

namespace logtail {

// SystemdJournalReader::Impl implementation
class SystemdJournalReader::Impl {
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
            return false;
        }
        return true;
#else
        return mIsOpen;
#endif
    }
    
    bool Next() {
#ifdef __linux__
        if (!IsOpen()) {
            return false;
        }
        
        int ret = sd_journal_next(mJournal);
        if (ret < 0) {
            return false;
        }
        
        if (ret > 0) {
        } else {
        }
        
        return ret > 0; // ret > 0 means we have a next entry
#else
        // On non-Linux systems, simulate success
        return mIsOpen;
#endif
    }
    
    bool Previous() {
#ifdef __linux__
        if (!IsOpen()) return false;
        
        int ret = sd_journal_previous(mJournal);
        if (ret < 0) {
            return false;
        }
        return ret > 0; // ret > 0 means we have a previous entry
#else
        return mIsOpen;
#endif
    }
    
    bool GetEntry(JournalEntry& entry) {
#ifdef __linux__
        if (!IsOpen()) {
            return false;
        }
        
        // Clear previous entry data
        entry.fields.clear();
        
        // Get cursor
        char* cursor;
        int ret = sd_journal_get_cursor(mJournal, &cursor);
        if (ret < 0) {
            return false;
        }
        entry.cursor = cursor;
        
        // Get realtime timestamp
        uint64_t realtime;
        ret = sd_journal_get_realtime_usec(mJournal, &realtime);
        if (ret < 0) {
            // using current time
        }
        entry.realtimeTimestamp = realtime;
        
        // Get monotonic timestamp
        uint64_t monotonic;
        ret = sd_journal_get_monotonic_usec(mJournal, &monotonic, nullptr);
        if (ret < 0) {
            // using current time
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
        
        if (r < 0) {
            // error enumerating journal data
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
    sd_journal* mJournal;  // Always define, but only use when __linux__ is defined
    size_t mDataThreshold;
    int mTimeout;
};

// JournalReader implementation
SystemdJournalReader::SystemdJournalReader() : mImpl(std::make_unique<Impl>()) {}

SystemdJournalReader::~SystemdJournalReader() = default;

bool SystemdJournalReader::Open() { return mImpl->Open(); }
void SystemdJournalReader::Close() { mImpl->Close(); }
bool SystemdJournalReader::IsOpen() const { return mImpl->IsOpen(); }
bool SystemdJournalReader::SeekHead() { return mImpl->SeekHead(); }
bool SystemdJournalReader::SeekTail() { return mImpl->SeekTail(); }
bool SystemdJournalReader::SeekCursor(const std::string& cursor) { return mImpl->SeekCursor(cursor); }
bool SystemdJournalReader::Next() { return mImpl->Next(); }
bool SystemdJournalReader::Previous() { return mImpl->Previous(); }
bool SystemdJournalReader::GetEntry(JournalEntry& entry) { return mImpl->GetEntry(entry); }
std::string SystemdJournalReader::GetCursor() { return mImpl->GetCursor(); }
bool SystemdJournalReader::AddMatch(const std::string& field, const std::string& value) { return mImpl->AddMatch(field, value); }
bool SystemdJournalReader::AddDisjunction() { return mImpl->AddDisjunction(); }
int SystemdJournalReader::Wait(std::chrono::milliseconds timeout) { return mImpl->Wait(timeout); }
bool SystemdJournalReader::SetDataThreshold(size_t threshold) { return mImpl->SetDataThreshold(threshold); }
bool SystemdJournalReader::SetTimeout(std::chrono::milliseconds timeout) { return mImpl->SetTimeout(timeout); }

} // namespace logtail 
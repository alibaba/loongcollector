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
#include "JournalReader.h"

#include <chrono>
#include <thread>
#include <map>

#include "common/ParamExtractor.h"
#include "logger/Logger.h"

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
    , mShutdown(false)
    , mLastSaveCheckpointTime(std::chrono::steady_clock::now()) {
}

InputJournal::~InputJournal() {
    Stop(true);
}

bool InputJournal::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
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
    
    mMainThread = std::thread(&InputJournal::MainLoop, this);
    
    return true;
}

bool InputJournal::Stop(bool isPipelineRemoving) {
    if (mShutdown) {
        return true;
    }
    
    mShutdown = true;
    mCondition.notify_all();
    
    if (mMainThread.joinable()) {
        mMainThread.join();
    }
    
    if (mJournalReader) {
        mJournalReader->Close();
    }
    
    return true;
}

bool InputJournal::LoadCheckpoint() {
    // TODO: Implement checkpoint loading
    return false;
}

bool InputJournal::SaveCheckpoint(bool force) {
    // TODO: Implement checkpoint saving
    return true;
}

void InputJournal::MainLoop() {
    while (!mShutdown) {
        if (!InitJournal()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }
        
        // Main processing loop
        auto startTime = std::chrono::steady_clock::now();
        while (!mShutdown) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            
            // Check if we need to restart journal
            if (elapsed >= mResetIntervalSecond) {
                break;
            }
            
            // Process journal entries
            if (!ProcessJournalEntries()) {
                break;
            }
            
            // Small delay to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Cleanup for restart
        if (mJournalReader) {
            mJournalReader->Close();
        }
    }
}

bool InputJournal::InitJournal() {
    mJournalReader = std::make_unique<SystemdJournalReader>();
    
    if (!mJournalReader->Open()) {
        return false;
    }
    
    // Add filters
    if (!AddUnits()) {
        return false;
    }
    
    if (!AddKernel()) {
        return false;
    }
    
    if (!AddSyslogIdentifiers()) {
        return false;
    }
    
    if (!AddMatchPatterns()) {
        return false;
    }
    
    // Position journal
    if (mSeekPosition == SEEK_POSITION_HEAD) {
        mJournalReader->SeekHead();
    } else {
        mJournalReader->SeekTail();
        mJournalReader->Previous(); // Move back one entry
    }
    
    return true;
}

bool InputJournal::AddUnits() {
    for (const auto& unit : mUnits) {
        if (!mJournalReader->AddMatch("_SYSTEMD_UNIT", unit)) {
            return false;
        }
        if (!mJournalReader->AddDisjunction()) {
            return false;
        }
    }
    return true;
}

bool InputJournal::AddKernel() {
    if (mKernel && !mUnits.empty()) {
        if (!mJournalReader->AddMatch("_TRANSPORT", "kernel")) {
            return false;
        }
        if (!mJournalReader->AddDisjunction()) {
            return false;
        }
    }
    return true;
}

bool InputJournal::AddSyslogIdentifiers() {
    for (const auto& identifier : mIdentifiers) {
        if (!mJournalReader->AddMatch("SYSLOG_IDENTIFIER", identifier)) {
            return false;
        }
        if (!mJournalReader->AddDisjunction()) {
            return false;
        }
    }
    return true;
}

bool InputJournal::AddMatchPatterns() {
    for (const auto& pattern : mMatchPatterns) {
        if (!mJournalReader->AddMatch(pattern, "")) {
            return false;
        }
        if (!mJournalReader->AddDisjunction()) {
            return false;
        }
    }
    return true;
}

bool InputJournal::ProcessJournalEntries() {
    while (!mShutdown) {
        if (!mJournalReader->Next()) {
            // No more entries, wait for new ones
            int event = mJournalReader->Wait(std::chrono::milliseconds(300));
            if (event == 1) { // SD_JOURNAL_APPEND
                continue;
            } else if (event == -1) { // Error or timeout
                break;
            }
            continue;
        }
        
        JournalEntry entry;
        if (!mJournalReader->GetEntry(entry)) {
            continue;
        }
        
        ProcessJournalEntry(entry);
    }
    
    return true;
}

void InputJournal::ProcessJournalEntry(const JournalEntry& entry) {
    // Parse priority if enabled
    if (mParsePriority && entry.HasField("PRIORITY")) {
        std::string priority = entry.GetField("PRIORITY");
        // Convert numeric priority to text (simplified)
        static const std::map<std::string, std::string> priorityMap = {
            {"0", "emergency"}, {"1", "alert"}, {"2", "critical"},
            {"3", "error"}, {"4", "warning"}, {"5", "notice"},
            {"6", "informational"}, {"7", "debug"}
        };
        
        auto it = priorityMap.find(priority);
        if (it != priorityMap.end()) {
            // Note: In a real implementation, you would modify the entry
            // For now, we just process it silently
        }
    }
    
    // Parse syslog facility if enabled
    if (mParseSyslogFacility && entry.HasField("SYSLOG_FACILITY")) {
        std::string facility = entry.GetField("SYSLOG_FACILITY");
        // Convert numeric facility to text (simplified)
        static const std::map<std::string, std::string> facilityMap = {
            {"0", "kernel"}, {"1", "user"}, {"2", "mail"}, {"3", "daemon"},
            {"4", "auth"}, {"5", "syslog"}, {"6", "lpr"}, {"7", "news"}
        };
        
        auto it = facilityMap.find(facility);
        if (it != facilityMap.end()) {
            // Note: In a real implementation, you would modify the entry
            // For now, we just process it silently
        }
    }
    
    // Determine event time
    std::chrono::system_clock::time_point eventTime;
    if (mUseJournalEventTime) {
        eventTime = entry.GetRealtimeTimestamp();
    } else {
        eventTime = std::chrono::system_clock::now();
    }
    
    // TODO: Convert entry to log format and send to pipeline
    // This would typically involve creating a LogEvent and sending it through the pipeline
}

} // namespace logtail 
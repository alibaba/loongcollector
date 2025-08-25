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

#include "common/ParamExtractor.h"
#include "common/FileSystemUtil.h"
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
    if (!HasContext()) {
        LOG_WARNING(sLogger, ("no context available for checkpoint", "skip loading"));
        return false;
    }
    
    // Get checkpoint file path from AppConfig
    std::string checkpointDir = GetAgentDataDir();
    if (checkpointDir.empty()) {
        LOG_INFO(sLogger, ("checkpoint directory not configured", "skip loading"));
        return false;
    }
    
    // Create checkpoint file path: {checkpointDir}/input_journal_{configName}.json
    std::string configName = GetContext().GetConfigName();
    std::string checkpointFile = checkpointDir + "/input_journal_" + configName + ".json";
    
    // Check if checkpoint file exists
    std::error_code ec;
    std::filesystem::file_status s = std::filesystem::status(checkpointFile, ec);
    if (ec || !std::filesystem::exists(s)) {
        LOG_INFO(sLogger, ("checkpoint file not found", checkpointFile));
        return false;
    }
    
    if (!std::filesystem::is_regular_file(s)) {
        LOG_WARNING(sLogger, ("checkpoint file is not a regular file", "skip")("filepath", checkpointFile));
        return false;
    }
    
    // Read checkpoint file content
    std::string content;
    if (!ReadFile(checkpointFile, content)) {
        LOG_WARNING(sLogger, ("failed to read checkpoint file", "skip")("filepath", checkpointFile));
        return false;
    }
    
    if (content.empty()) {
        LOG_WARNING(sLogger, ("empty checkpoint file", "skip")("filepath", checkpointFile));
        return false;
    }
    
    // Parse JSON content
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(content, root)) {
        LOG_WARNING(sLogger, ("failed to parse checkpoint file", "skip")("filepath", checkpointFile));
        return false;
    }
    
    if (!root.isObject()) {
        LOG_WARNING(sLogger, ("checkpoint file is not a JSON object", "skip")("filepath", checkpointFile));
        return false;
    }
    
    // Extract cursor from checkpoint
    if (root.isMember("cursor") && root["cursor"].isString()) {
        mLastCheckpointCursor = root["cursor"].asString();
        LOG_INFO(sLogger, ("loaded checkpoint cursor", mLastCheckpointCursor)("filepath", checkpointFile));
        return true;
    } else {
        LOG_WARNING(sLogger, ("checkpoint file missing cursor field", "skip")("filepath", checkpointFile));
        return false;
    }
}

bool InputJournal::SaveCheckpoint(bool force) {
    if (!HasContext()) {
        LOG_WARNING(sLogger, ("no context available for checkpoint", "skip saving"));
        return false;
    }
    
    // Check if we need to save (either forced or time-based)
    if (!force) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - mLastSaveCheckpointTime).count();
        
        if (elapsed < mCursorFlushPeriodMs) {
            return true; // Not time to save yet
        }
    }
    
    // Get current cursor from journal reader
    if (!mJournalReader || !mJournalReader->IsOpen()) {
        LOG_DEBUG(sLogger, ("journal reader not available", "skip saving checkpoint"));
        return true;
    }
    
    std::string currentCursor = mJournalReader->GetCursor();
    if (currentCursor.empty()) {
        LOG_DEBUG(sLogger, ("current cursor is empty", "skip saving checkpoint"));
        return true;
    }
    
    // Get checkpoint file path from AppConfig
    std::string checkpointDir = GetAgentDataDir();
    if (checkpointDir.empty()) {
        LOG_WARNING(sLogger, ("checkpoint directory not configured", "skip saving"));
        return false;
    }
    
    // Create checkpoint file path: {checkpointDir}/input_journal_{configName}.json
    std::string configName = GetContext().GetConfigName();
    std::string checkpointFile = checkpointDir + "/input_journal_" + configName + ".json";
    
    // Create checkpoint directory if it doesn't exist
    std::error_code ec;
    if (!std::filesystem::exists(checkpointDir, ec)) {
        if (!std::filesystem::create_directories(checkpointDir, ec)) {
            LOG_ERROR(sLogger, ("failed to create checkpoint directory", checkpointDir)("error", ec.message()));
            return false;
        }
    }
    
    // Prepare checkpoint data
    Json::Value root;
    root["cursor"] = currentCursor;
    root["timestamp"] = static_cast<Json::UInt64>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    root["config_name"] = configName;
    root["plugin_type"] = "input_journal";
    
    // Convert to JSON string
    std::string content = root.toStyledString();
    
    // Write checkpoint file
    std::string errMsg;
    if (!UpdateFileContent(std::filesystem::path(checkpointFile), content, errMsg)) {
        LOG_ERROR(sLogger, ("failed to save checkpoint file", checkpointFile)("error", errMsg));
        return false;
    }
    
    // Update internal state
    mLastCheckpointCursor = currentCursor;
    mLastSaveCheckpointTime = std::chrono::steady_clock::now();
    
    LOG_DEBUG(sLogger, ("saved checkpoint", currentCursor)("filepath", checkpointFile));
    return true;
}

void InputJournal::MainLoop() {
    // Load checkpoint at startup
    LoadCheckpoint();
    
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
            
            // Save checkpoint periodically
            SaveCheckpoint(false);
            
            // Small delay to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Save checkpoint before cleanup
        SaveCheckpoint(true);
        
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
    
    // Position journal based on checkpoint or configuration
    if (!mLastCheckpointCursor.empty()) {
        // Try to seek to checkpoint position
        if (mJournalReader->SeekCursor(mLastCheckpointCursor)) {
            LOG_INFO(sLogger, ("positioned journal to checkpoint cursor", mLastCheckpointCursor));
            // Move to next entry to avoid re-reading the last processed entry
            mJournalReader->Next();
        } else {
            LOG_WARNING(sLogger, ("failed to seek to checkpoint cursor", mLastCheckpointCursor)("falling back to configured position"));
            // Fall back to configured position
            PositionJournalByConfig();
        }
    } else {
        // No checkpoint available, use configured position
        PositionJournalByConfig();
    }
    
    return true;
}

void InputJournal::PositionJournalByConfig() {
    if (mSeekPosition == SEEK_POSITION_HEAD) {
        mJournalReader->SeekHead();
        LOG_INFO(sLogger, ("positioned journal to head"));
    } else if (mSeekPosition == SEEK_POSITION_TAIL) {
        mJournalReader->SeekTail();
        mJournalReader->Previous(); // Move back one entry
        LOG_INFO(sLogger, ("positioned journal to tail"));
    } else if (mSeekPosition == SEEK_POSITION_CURSOR && !mCursorSeekFallback.empty()) {
        // Try fallback position
        if (mCursorSeekFallback == SEEK_POSITION_HEAD) {
            mJournalReader->SeekHead();
            LOG_INFO(sLogger, ("positioned journal to head (fallback)"));
        } else {
            mJournalReader->SeekTail();
            mJournalReader->Previous();
            LOG_INFO(sLogger, ("positioned journal to tail (fallback)"));
        }
    } else {
        // Default to tail
        mJournalReader->SeekTail();
        mJournalReader->Previous();
        LOG_INFO(sLogger, ("positioned journal to tail (default)"));
    }
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

// SystemdJournalReader::Impl implementation
class InputJournal::SystemdJournalReader::Impl {
public:
    Impl() : mIsOpen(false), mCurrentPosition(0) {}
    
    ~Impl() = default;
    
    bool Open() {
        mIsOpen = true;
        return true;
    }
    
    void Close() {
        mIsOpen = false;
    }
    
    bool IsOpen() const {
        return mIsOpen;
    }
    
    bool SeekHead() {
        if (!IsOpen()) return false;
        mCurrentPosition = 0;
        return true;
    }
    
    bool SeekTail() {
        if (!IsOpen()) return false;
        mCurrentPosition = 1000; // Simulate some entries
        return true;
    }
    
    bool SeekCursor(const std::string& cursor) {
        if (!IsOpen()) return false;
        try {
            mCurrentPosition = std::stoul(cursor);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    bool Next() {
        if (!IsOpen()) return false;
        mCurrentPosition++;
        return mCurrentPosition < 1000; // Simulate end of journal
    }
    
    bool Previous() {
        if (!IsOpen()) return false;
        if (mCurrentPosition > 0) {
            mCurrentPosition--;
            return true;
        }
        return false;
    }
    
    bool GetEntry(JournalEntry& entry) {
        if (!IsOpen()) return false;
        
        // Simulate journal entry
        entry.cursor = std::to_string(mCurrentPosition);
        entry.realtimeTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        entry.monotonicTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        
        // Add some sample fields
        entry.fields.clear();
        entry.fields["MESSAGE"] = "Sample journal entry " + std::to_string(mCurrentPosition);
        entry.fields["_SYSTEMD_UNIT"] = "sample.service";
        entry.fields["PRIORITY"] = "6";
        entry.fields["SYSLOG_FACILITY"] = "3";
        
        return true;
    }
    
    std::string GetCursor() {
        if (!IsOpen()) return "";
        return std::to_string(mCurrentPosition);
    }
    
    bool AddMatch(const std::string& field, const std::string& value) {
        if (!IsOpen()) return false;
        // Store match for filtering (simplified)
        return true;
    }
    
    bool AddDisjunction() {
        if (!IsOpen()) return false;
        return true;
    }
    
    int Wait(std::chrono::milliseconds timeout) {
        if (!IsOpen()) return -1;
        
        // Simulate waiting for new entries
        std::this_thread::sleep_for(timeout);
        return 1; // SD_JOURNAL_APPEND
    }
    
    bool SetDataThreshold(size_t threshold) {
        if (!IsOpen()) return false;
        return true;
    }
    
    bool SetTimeout(std::chrono::milliseconds timeout) {
        if (!IsOpen()) return false;
        return true;
    }
    
private:
    bool mIsOpen;
    size_t mCurrentPosition;
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
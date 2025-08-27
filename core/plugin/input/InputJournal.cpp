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

#include <chrono>
#include <string>
#include <vector>

#include "common/ParamExtractor.h"
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
        LOG_WARNING(sLogger, ("InputJournal already shutdown", "cannot start")("config", mContext->GetConfigName())("idx", mIndex));
        return false;
    }
    
    LOG_INFO(sLogger, ("starting InputJournal", "")("config", mContext->GetConfigName())("idx", mIndex));
    
    // Initialize JournalServer if not already initialized
    JournalServer::GetInstance()->Init();
    
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
    
    LOG_DEBUG(sLogger, ("journal config created", "")("config", mContext->GetConfigName())("idx", mIndex));
    LOG_DEBUG(sLogger, ("journal config details", "")("seek_position", mSeekPosition)("units_count", mUnits.size())("identifiers_count", mIdentifiers.size())("journal_paths_count", mJournalPaths.size()));
    
    // Register with JournalServer
    JournalServer::GetInstance()->AddJournalInput(
        mContext->GetConfigName(), 
        mIndex, 
        config);
    
    LOG_INFO(sLogger, ("InputJournal registered with JournalServer", "")("config", mContext->GetConfigName())("idx", mIndex));
    
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
    // 不再需要 JournalReader，JournalServer 会处理所有数据
    
    return true;
}

} // namespace logtail 
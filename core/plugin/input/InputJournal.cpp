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

#include <string>
#include <vector>

#include "common/ParamExtractor.h"
#include "journal_server/JournalServer.h"
#include "logger/Logger.h"

namespace logtail {

const std::string InputJournal::sName = "input_journal";

// Static constants
const std::string InputJournal::kSeekPositionCursor = "cursor";
const std::string InputJournal::kSeekPositionHead = "head";
const std::string InputJournal::kSeekPositionTail = "tail";
const std::string InputJournal::kSeekPositionDefault = "none";

InputJournal::InputJournal()
    : mSeekPosition(kSeekPositionTail)
    , mCursorFlushPeriodMs(kDefaultCursorFlushPeriodMs)
    , mCursorSeekFallback(kSeekPositionTail)
    , mKernel(true)
    , mParseSyslogFacility(false)
    , mParsePriority(false)
    , mUseJournalEventTime(false)
    , mResetIntervalSecond(kDefaultResetInterval)
    , mShutdown(false) {
}

InputJournal::~InputJournal() {
    Stop(true);
}

bool InputJournal::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    (void)optionalGoPipeline; // Suppress unused parameter warning
    
    parseBasicParams(config);
    parseArrayParams(config);
    
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
    
    // 已创建journal配置对象，包含所有配置参数
    // 配置详情：seek位置、units数量、identifiers数量、journal路径数量等
    
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

void InputJournal::parseBasicParams(const Json::Value& config) {
    std::string errorMsg;
    
    if (!GetOptionalStringParam(config, "SeekPosition", mSeekPosition, errorMsg)) {
        mSeekPosition = kSeekPositionTail;
    }
    
    if (!GetOptionalIntParam(config, "CursorFlushPeriodMs", mCursorFlushPeriodMs, errorMsg)) {
        mCursorFlushPeriodMs = kDefaultCursorFlushPeriodMs;
    }
    
    if (!GetOptionalStringParam(config, "CursorSeekFallback", mCursorSeekFallback, errorMsg)) {
        mCursorSeekFallback = kSeekPositionTail;
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
        mResetIntervalSecond = kDefaultResetInterval;
    }
}

void InputJournal::parseArrayParams(const Json::Value& config) {
    parseStringArray(config, "Units", mUnits);
    parseStringArray(config, "Identifiers", mIdentifiers);
    parseStringArray(config, "JournalPaths", mJournalPaths);
    parseStringArray(config, "MatchPatterns", mMatchPatterns);
}

void InputJournal::parseStringArray(const Json::Value& config, const std::string& key, std::vector<std::string>& target) {
    if (config.isMember(key) && config[key].isArray()) {
        for (const auto& item : config[key]) {
            if (item.isString()) {
                target.push_back(item.asString());
            }
        }
    }
}

} // namespace logtail 
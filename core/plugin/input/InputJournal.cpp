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
    
    // 初始化JournalServer
    JournalServer::GetInstance()->Init();
    
    // 创建journal配置对象
    JournalConfig config;
    config.seekPosition = mSeekPosition; // 设置seek位置
    config.cursorFlushPeriodMs = mCursorFlushPeriodMs; // 设置cursor刷新周期
    config.cursorSeekFallback = mCursorSeekFallback; // 设置cursor回退位置
    config.units = mUnits; // 设置units
    config.kernel = mKernel; // 设置kernel
    config.identifiers = mIdentifiers; // 设置identifiers
    config.journalPaths = mJournalPaths; // 设置journal路径
    // 注意：已移除resetIntervalSecond配置，连接永远不重建
    config.ctx = mContext; // 设置context
    
    // 验证和修正配置值
    int fixedCount = config.ValidateAndFixConfig();
    if (fixedCount > 0) {
        LOG_WARNING(sLogger, ("journal config fixed", "some values were corrected")("config", mContext->GetConfigName())("idx", mIndex)("fixed_count", fixedCount));
    }
    
    // 最终验证配置有效性
    if (!config.IsValid()) {
        LOG_ERROR(sLogger, ("invalid journal config", "cannot start")("config", mContext->GetConfigName())("idx", mIndex));
        return false;
    }
    
    // 注册到JournalServer
    JournalServer::GetInstance()->AddJournalInput(
        mContext->GetConfigName(), 
        mIndex, 
        config);
    
    LOG_INFO(sLogger, ("InputJournal registered with JournalServer", "")("config", mContext->GetConfigName())("idx", mIndex));
    
    return true;
}

bool InputJournal::Stop(bool isPipelineRemoving) {
    if (mShutdown) {
        return true;
    }
    
    if (isPipelineRemoving) {
        // 配置被删除：完全清理，包括检查点
        JournalServer::GetInstance()->RemoveJournalInput(mContext->GetConfigName(), mIndex);
        LOG_INFO(sLogger, ("InputJournal removed with checkpoint cleanup", "")("config", mContext->GetConfigName())("idx", mIndex));
    } else {
        // 配置更新：只移除注册，保留检查点
        JournalServer::GetInstance()->RemoveConfigOnly(mContext->GetConfigName(), mIndex);
        LOG_INFO(sLogger, ("InputJournal stopped for config update, checkpoint preserved", "")("config", mContext->GetConfigName())("idx", mIndex));
    }
    
    mShutdown = true;
    
    return true;
}

void InputJournal::parseBasicParams(const Json::Value& config) {
    std::string errorMsg;
    // 获取seek位置
    if (!GetOptionalStringParam(config, "SeekPosition", mSeekPosition, errorMsg)) {
        mSeekPosition = kSeekPositionTail;
    }
    // 验证seek位置的有效性
    if (mSeekPosition != "head" && mSeekPosition != "tail" && 
        mSeekPosition != "cursor" && mSeekPosition != "none") {
        LOG_WARNING(sLogger, ("invalid SeekPosition value, using default", mSeekPosition)("default", kSeekPositionTail));
        mSeekPosition = kSeekPositionTail;
    }
    
    // 获取cursor刷新周期
    if (!GetOptionalIntParam(config, "CursorFlushPeriodMs", mCursorFlushPeriodMs, errorMsg)) {
        mCursorFlushPeriodMs = kDefaultCursorFlushPeriodMs;
    }
    // 验证cursor刷新周期范围 (1秒到5分钟)
    if (mCursorFlushPeriodMs <= 0) {
        LOG_WARNING(sLogger, ("invalid CursorFlushPeriodMs, using default", mCursorFlushPeriodMs)("default", kDefaultCursorFlushPeriodMs));
        mCursorFlushPeriodMs = kDefaultCursorFlushPeriodMs;
    } else if (mCursorFlushPeriodMs > 300000) {
        LOG_WARNING(sLogger, ("CursorFlushPeriodMs too large, capping", mCursorFlushPeriodMs)("max", 300000));
        mCursorFlushPeriodMs = 300000;
    }
    
    // 获取cursor回退位置
    if (!GetOptionalStringParam(config, "CursorSeekFallback", mCursorSeekFallback, errorMsg)) {
        mCursorSeekFallback = kSeekPositionTail;
    }
    // 验证cursor回退位置的有效性
    if (mCursorSeekFallback != "head" && mCursorSeekFallback != "tail") {
        LOG_WARNING(sLogger, ("invalid CursorSeekFallback value, using default", mCursorSeekFallback)("default", kSeekPositionTail));
        mCursorSeekFallback = kSeekPositionTail;
    }
    
    // 获取kernel
    if (!GetOptionalBoolParam(config, "Kernel", mKernel, errorMsg)) {
        mKernel = true;
    }
    
    // 获取parse syslog facility
    if (!GetOptionalBoolParam(config, "ParseSyslogFacility", mParseSyslogFacility, errorMsg)) {
        mParseSyslogFacility = false;
    }
    
    // 获取parse priority
    if (!GetOptionalBoolParam(config, "ParsePriority", mParsePriority, errorMsg)) {
        mParsePriority = false;
    }
    
    // 获取use journal event time
    if (!GetOptionalBoolParam(config, "UseJournalEventTime", mUseJournalEventTime, errorMsg)) {
        mUseJournalEventTime = false;
    }
    
    // 获取max entries per batch (如果有这个配置)
    int maxEntriesPerBatch = 1000;  // 默认值
    if (GetOptionalIntParam(config, "MaxEntriesPerBatch", maxEntriesPerBatch, errorMsg)) {
        if (maxEntriesPerBatch <= 0) {
            LOG_WARNING(sLogger, ("invalid MaxEntriesPerBatch, using default", maxEntriesPerBatch)("default", 1000));
            maxEntriesPerBatch = 1000;
        } else if (maxEntriesPerBatch > 10000) {
            LOG_WARNING(sLogger, ("MaxEntriesPerBatch too large, capping", maxEntriesPerBatch)("max", 10000));
            maxEntriesPerBatch = 10000;
        }
        // 这里可以将值存储到成员变量中，如果需要的话
    }
    
    // 获取wait timeout (如果有这个配置)
    int waitTimeoutMs = 1000;  // 默认值
    if (GetOptionalIntParam(config, "WaitTimeoutMs", waitTimeoutMs, errorMsg)) {
        if (waitTimeoutMs < 0) {
            LOG_WARNING(sLogger, ("invalid WaitTimeoutMs, using default", waitTimeoutMs)("default", 1000));
            waitTimeoutMs = 1000;
        } else if (waitTimeoutMs > 60000) {
            LOG_WARNING(sLogger, ("WaitTimeoutMs too large, capping", waitTimeoutMs)("max", 60000));
            waitTimeoutMs = 60000;  // 最大1分钟
        }
        // 这里可以将值存储到成员变量中，如果需要的话
    }
}

void InputJournal::parseArrayParams(const Json::Value& config) {
    // 获取units    
    parseStringArray(config, "Units", mUnits);
    // 获取identifiers
    parseStringArray(config, "Identifiers", mIdentifiers);
    // 获取journal路径
    parseStringArray(config, "JournalPaths", mJournalPaths);
    // 获取match patterns
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
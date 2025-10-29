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

#include "collection_pipeline/CollectionPipeline.h"
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
    : mSeekPosition(kSeekPositionTail),
      mCursorSeekFallback(kSeekPositionHead) // 即如果游标无效，回退到head
      ,
      mKernel(true),
      mParseSyslogFacility(false),
      mParsePriority(false),
      mUseJournalEventTime(false),
      mShutdown(false) {
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
        LOG_WARNING(sLogger,
                    ("InputJournal already shutdown",
                     "cannot start")("config", mContext ? mContext->GetConfigName() : "unknown")("idx", mIndex));
        return false;
    }

    if (!mContext) {
        LOG_ERROR(sLogger, ("InputJournal Start called without context", "cannot start"));
        return false;
    }

    // 同一Config配置中只能有一个 InputJournal 输入
    if (mContext->HasValidPipeline()) {
        const auto& inputs = mContext->GetPipeline().GetInputs();
        int inputJournalCount = 0;
        for (const auto& input : inputs) {
            if (input && input->GetPlugin() && input->GetPlugin()->Name() == sName) {
                inputJournalCount++;
            }
        }
        if (inputJournalCount > 1) {
            LOG_ERROR(sLogger,
                      ("InputJournal: multiple input_journal instances found in the same config",
                       "only one input_journal is allowed per config")("config", mContext->GetConfigName())(
                          "count", inputJournalCount)("idx", mIndex));
            return false;
        }
    }

    LOG_INFO(sLogger, ("starting InputJournal", "")("config", mContext->GetConfigName())("idx", mIndex));

    // 初始化JournalServer
    JournalServer::GetInstance()->Init();

    JournalConfig config;
    config.mSeekPosition = mSeekPosition;
    config.mCursorSeekFallback = mCursorSeekFallback;
    config.mUnits = mUnits;
    config.mKernel = mKernel;
    config.mIdentifiers = mIdentifiers;
    config.mJournalPaths = mJournalPaths;
    config.mCtx = mContext;

    // 验证和修正配置值
    int fixedCount = config.ValidateAndFixConfig();
    if (fixedCount > 0) {
        LOG_WARNING(sLogger,
                    ("journal config fixed", "some values were corrected")("config", mContext->GetConfigName())(
                        "idx", mIndex)("fixed_count", fixedCount));
    }

    if (!config.IsValid()) {
        LOG_ERROR(sLogger,
                  ("invalid journal config", "cannot start")("config", mContext->GetConfigName())("idx", mIndex));
        return false;
    }

    // 注册到JournalServer
    JournalServer::GetInstance()->AddJournalInput(mContext->GetConfigName(), config);

    LOG_INFO(sLogger,
             ("InputJournal registered with JournalServer", "")("config", mContext->GetConfigName())("idx", mIndex));

    return true;
}

bool InputJournal::Stop(bool isPipelineRemoving) {
    // 使用 CAS 确保幂等性：只有第一个线程能执行清理逻辑
    bool expected = false;
    if (!mShutdown.compare_exchange_strong(expected, true)) {
        // 已经有其他线程在执行或已完成清理，直接返回
        return true;
    }

    if (!mContext) {
        LOG_WARNING(sLogger, ("InputJournal Stop called without context", "skipping cleanup"));
        return true;
    }

    if (isPipelineRemoving) {
        // 配置被删除：完全清理
        JournalServer::GetInstance()->RemoveJournalInput(mContext->GetConfigName());
        LOG_INFO(sLogger, ("InputJournal removed with checkpoint cleanup", "")("config", mContext->GetConfigName()));

        if (!JournalServer::GetInstance()->HasRegisteredPlugins()) {
            JournalServer::GetInstance()->Stop();
        }
    } else {
        // 配置更新：只移除注册
        JournalServer::GetInstance()->RemoveConfigOnly(mContext->GetConfigName());
        LOG_INFO(
            sLogger,
            ("InputJournal stopped for config update, checkpoint preserved", "")("config", mContext->GetConfigName()));
    }

    return true;
}

void InputJournal::parseBasicParams(const Json::Value& config) {
    std::string errorMsg;

    if (!GetOptionalStringParam(config, "SeekPosition", mSeekPosition, errorMsg)) {
        mSeekPosition = kSeekPositionTail;
    }
    // 验证seek位置的有效性
    if (mSeekPosition != "head" && mSeekPosition != "tail" && mSeekPosition != "cursor" && mSeekPosition != "none") {
        LOG_WARNING(sLogger,
                    ("invalid SeekPosition value, using default", mSeekPosition)("default", kSeekPositionTail));
        mSeekPosition = kSeekPositionTail;
    }

    // 获取cursor回退位置（即默认head， 与go实现不同go实现默认tail）
    if (!GetOptionalStringParam(config, "CursorSeekFallback", mCursorSeekFallback, errorMsg)) {
        mCursorSeekFallback = kSeekPositionHead;
    }
    if (mCursorSeekFallback != "head" && mCursorSeekFallback != "tail") {
        LOG_WARNING(
            sLogger,
            ("invalid CursorSeekFallback value, using default", mCursorSeekFallback)("default", kSeekPositionHead));
        mCursorSeekFallback = kSeekPositionHead;
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

    // 获取max entries per batch (如果有这个配置)
    int maxEntriesPerBatch = 1000;
    if (GetOptionalIntParam(config, "MaxEntriesPerBatch", maxEntriesPerBatch, errorMsg)) {
        if (maxEntriesPerBatch <= 0) {
            LOG_WARNING(sLogger, ("invalid MaxEntriesPerBatch, using default", maxEntriesPerBatch)("default", 1000));
            maxEntriesPerBatch = 1000;
        } else if (maxEntriesPerBatch > 10000) {
            LOG_WARNING(sLogger, ("MaxEntriesPerBatch too large, capping", maxEntriesPerBatch)("max", 10000));
            maxEntriesPerBatch = 10000;
        }
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

void InputJournal::parseStringArray(const Json::Value& config,
                                    const std::string& key,
                                    std::vector<std::string>& target) {
    if (config.isMember(key) && config[key].isArray()) {
        for (const auto& item : config[key]) {
            if (item.isString()) {
                target.push_back(item.asString());
            }
        }
    }
}

} // namespace logtail

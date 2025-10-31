/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "json/json.h"

#include "collection_pipeline/queue/QueueKey.h"
#include "common/ParamExtractor.h"

namespace logtail {

class CollectionPipelineContext;

/**
 * @brief journal输入插件的配置
 */
struct JournalConfig {
    std::string mSeekPosition;
    std::string mCursorSeekFallback;

    std::vector<std::string> mUnits;
    std::vector<std::string> mIdentifiers;
    std::vector<std::string> mMatchPatterns;
    bool mKernel = true;

    // 注意：与Go插件不同已移除resetIntervalSecond配置，连接永远不重建
    int mMaxEntriesPerBatch = 1000;

    bool mParsePriority = false;
    bool mParseSyslogFacility = false;
    bool mUseJournalEventTime = true;

    std::vector<std::string> mJournalPaths;

    const CollectionPipelineContext* mCtx = nullptr;

    mutable QueueKey mQueueKey = -1; // 验证后缓存的队列键值（-1 = 未验证）

    JournalConfig() = default;


    static JournalConfig ParseFromJson(const Json::Value& config, const CollectionPipelineContext* ctx = nullptr) {
        JournalConfig journalConfig;
        journalConfig.mCtx = ctx;

        std::string errorMsg;

        if (!GetOptionalStringParam(config, "SeekPosition", journalConfig.mSeekPosition, errorMsg)) {
            journalConfig.mSeekPosition = "tail";
        }

        // （默认head，与go实现不同）
        if (!GetOptionalStringParam(config, "CursorSeekFallback", journalConfig.mCursorSeekFallback, errorMsg)) {
            journalConfig.mCursorSeekFallback = "head";
        }

        if (!GetOptionalBoolParam(config, "Kernel", journalConfig.mKernel, errorMsg)) {
            journalConfig.mKernel = true;
        }
        if (!GetOptionalBoolParam(config, "ParseSyslogFacility", journalConfig.mParseSyslogFacility, errorMsg)) {
            journalConfig.mParseSyslogFacility = false;
        }
        if (!GetOptionalBoolParam(config, "ParsePriority", journalConfig.mParsePriority, errorMsg)) {
            journalConfig.mParsePriority = false;
        }
        if (!GetOptionalBoolParam(config, "UseJournalEventTime", journalConfig.mUseJournalEventTime, errorMsg)) {
            journalConfig.mUseJournalEventTime = false;
        }

        if (!GetOptionalIntParam(config, "MaxEntriesPerBatch", journalConfig.mMaxEntriesPerBatch, errorMsg)) {
            journalConfig.mMaxEntriesPerBatch = 1000;
        }

        static auto parseStringArray = [](const Json::Value& jsonConfig, const std::string& key) {
            std::vector<std::string> result;
            if (jsonConfig.isMember(key) && jsonConfig[key].isArray()) {
                for (const auto& item : jsonConfig[key]) {
                    if (item.isString()) {
                        result.push_back(item.asString());
                    }
                }
            }
            return result;
        };

        journalConfig.mUnits = parseStringArray(config, "Units");
        journalConfig.mIdentifiers = parseStringArray(config, "Identifiers");
        journalConfig.mJournalPaths = parseStringArray(config, "JournalPaths");
        journalConfig.mMatchPatterns = parseStringArray(config, "MatchPatterns");

        return journalConfig;
    }


    int ValidateAndFixConfig() {
        int fixedCount = 0;


        if (mMaxEntriesPerBatch <= 0) {
            mMaxEntriesPerBatch = 1000;
            fixedCount++;
        } else if (mMaxEntriesPerBatch > 10000) {
            mMaxEntriesPerBatch = 10000;
            fixedCount++;
        }

        if (mSeekPosition != "head" && mSeekPosition != "tail" && mSeekPosition != "cursor"
            && mSeekPosition != "none") {
            mSeekPosition = "tail";
            fixedCount++;
        }

        if (mCursorSeekFallback != "head" && mCursorSeekFallback != "tail") {
            mCursorSeekFallback = "head";
            fixedCount++;
        }

        auto removeEmpty = [&fixedCount](std::vector<std::string>& vec) {
            auto originalSize = vec.size();
            vec.erase(std::remove_if(vec.begin(), vec.end(), [](const std::string& s) { return s.empty(); }),
                      vec.end());
            if (vec.size() != originalSize) {
                fixedCount++;
            }
        };

        removeEmpty(mUnits);
        removeEmpty(mIdentifiers);
        removeEmpty(mMatchPatterns);
        removeEmpty(mJournalPaths);

        if (!mJournalPaths.empty()) {
            std::vector<std::string> validPaths;
            for (const auto& path : mJournalPaths) {
                if (path.length() > 0 && path.length() < 4096) {
                    validPaths.push_back(path);
                } else {
                    fixedCount++;
                }
            }
            if (validPaths.size() != mJournalPaths.size()) {
                mJournalPaths = std::move(validPaths);
            }
        }

        return fixedCount;
    }

    bool IsValid() const { return mMaxEntriesPerBatch > 0 && !mSeekPosition.empty() && !mCursorSeekFallback.empty(); }
};

} // namespace logtail

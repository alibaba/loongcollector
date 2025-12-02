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
 * @brief Configuration for journal input plugin
 */
struct JournalConfig {
    std::string mSeekPosition;
    std::string mCursorSeekFallback;

    std::vector<std::string> mUnits;
    std::vector<std::string> mIdentifiers;
    std::vector<std::string> mMatchPatterns;
    bool mKernel = true;

    int mResetIntervalSecond = 3600;
    size_t mMaxBytesPerBatch = 512 * 1024; // 512KB, default like file reader BUFFER_SIZE
    int mBatchTimeoutMs = 1000;

    bool mParsePriority = false;
    bool mParseSyslogFacility = false;
    bool mUseJournalEventTime = true;

    std::vector<std::string> mJournalPaths;

    const CollectionPipelineContext* mCtx = nullptr;

    mutable QueueKey mQueueKey = -1; // Cached queue key after validation (-1 = not validated)

    JournalConfig() = default;


    static JournalConfig ParseFromJson(const Json::Value& config, const CollectionPipelineContext* ctx = nullptr) {
        JournalConfig journalConfig;
        journalConfig.mCtx = ctx;

        std::string errorMsg;

        if (!GetOptionalStringParam(config, "SeekPosition", journalConfig.mSeekPosition, errorMsg)) {
            journalConfig.mSeekPosition = "tail";
        }

        // (default head, different from Go implementation)
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

        if (!GetOptionalIntParam(config, "ResetIntervalSecond", journalConfig.mResetIntervalSecond, errorMsg)) {
            journalConfig.mResetIntervalSecond = 3600;
        }

        int maxBytesPerBatch = 0;
        if (GetOptionalIntParam(config, "MaxBytesPerBatch", maxBytesPerBatch, errorMsg) && maxBytesPerBatch > 0) {
            journalConfig.mMaxBytesPerBatch = static_cast<size_t>(maxBytesPerBatch);
        } else {
            journalConfig.mMaxBytesPerBatch = 512 * 1024; // 512KB default
        }

        if (!GetOptionalIntParam(config, "BatchTimeoutMs", journalConfig.mBatchTimeoutMs, errorMsg)) {
            journalConfig.mBatchTimeoutMs = 1000;
        }

        auto ParseStringArray = [](const Json::Value& jsonConfig, const std::string& key) {
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

        journalConfig.mUnits = ParseStringArray(config, "Units");
        journalConfig.mIdentifiers = ParseStringArray(config, "Identifiers");
        journalConfig.mJournalPaths = ParseStringArray(config, "JournalPaths");
        journalConfig.mMatchPatterns = ParseStringArray(config, "MatchPatterns");

        return journalConfig;
    }


    int ValidateAndFixConfig() {
        int fixedCount = 0;

        if (mResetIntervalSecond <= 0) {
            mResetIntervalSecond = 3600;
            fixedCount++;
        } else if (mResetIntervalSecond > 86400) {
            // Limit maximum to 24 hours
            mResetIntervalSecond = 86400;
            fixedCount++;
        }

        // Validate MaxBytesPerBatch: min 10KB, max 10MB
        if (mMaxBytesPerBatch < 10 * 1024) {
            mMaxBytesPerBatch = 10 * 1024; // 10KB minimum
            fixedCount++;
        } else if (mMaxBytesPerBatch > 10 * 1024 * 1024) {
            mMaxBytesPerBatch = 10 * 1024 * 1024; // 10MB maximum
            fixedCount++;
        }

        if (mBatchTimeoutMs <= 0) {
            mBatchTimeoutMs = 1000;
            fixedCount++;
        } else if (mBatchTimeoutMs > 60000) {
            mBatchTimeoutMs = 60000; // Maximum 60 seconds
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

    bool IsValid() const { return mMaxBytesPerBatch > 0 && !mSeekPosition.empty() && !mCursorSeekFallback.empty(); }
};

} // namespace logtail

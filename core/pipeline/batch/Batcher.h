/*
 * Copyright 2024 iLogtail Authors
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

#include <json/json.h>

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

#include "pipeline/batch/BatchItem.h"
#include "pipeline/batch/BatchStatus.h"
#include "pipeline/batch/FlushStrategy.h"
#include "pipeline/batch/TimeoutFlushManager.h"
#include "common/Flags.h"
#include "common/ParamExtractor.h"
#include "models/PipelineEventGroup.h"
#include "pipeline/PipelineContext.h"

namespace logtail {

template <typename T = EventBatchStatus>
class Batcher {
public:
    bool Init(const Json::Value& config,
              Flusher* flusher,
              const DefaultFlushStrategyOptions& strategy,
              bool enableGroupBatch = false) {
        std::string errorMsg;
        PipelineContext& ctx = flusher->GetContext();

        uint32_t maxSizeBytes = strategy.mMaxSizeBytes;
        if (!GetOptionalUIntParam(config, "MaxSizeBytes", maxSizeBytes, errorMsg)) {
            PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                                  ctx.GetAlarm(),
                                  errorMsg,
                                  maxSizeBytes,
                                  flusher->Name(),
                                  ctx.GetConfigName(),
                                  ctx.GetProjectName(),
                                  ctx.GetLogstoreName(),
                                  ctx.GetRegion());
        }

        uint32_t maxCnt = strategy.mMaxCnt;
        if (!GetOptionalUIntParam(config, "MaxCnt", maxCnt, errorMsg)) {
            PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                                  ctx.GetAlarm(),
                                  errorMsg,
                                  maxCnt,
                                  flusher->Name(),
                                  ctx.GetConfigName(),
                                  ctx.GetProjectName(),
                                  ctx.GetLogstoreName(),
                                  ctx.GetRegion());
        }

        uint32_t timeoutSecs = strategy.mTimeoutSecs;
        if (!GetOptionalUIntParam(config, "TimeoutSecs", timeoutSecs, errorMsg)) {
            PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                                  ctx.GetAlarm(),
                                  errorMsg,
                                  timeoutSecs,
                                  flusher->Name(),
                                  ctx.GetConfigName(),
                                  ctx.GetProjectName(),
                                  ctx.GetLogstoreName(),
                                  ctx.GetRegion());
        }

        if (enableGroupBatch) {
            uint32_t groupTimeout = timeoutSecs / 2;
            mGroupFlushStrategy = GroupFlushStrategy(maxSizeBytes, groupTimeout);
            mGroupQueue = GroupBatchItem();
            mEventFlushStrategy.SetTimeoutSecs(timeoutSecs - groupTimeout);
        } else {
            mEventFlushStrategy.SetTimeoutSecs(timeoutSecs);
        }
        mEventFlushStrategy.SetMaxSizeBytes(maxSizeBytes);
        mEventFlushStrategy.SetMaxCnt(maxCnt);

        mFlusher = flusher;

        return true;
    }

    // when group level batch is disabled, there should be only 1 element in BatchedEventsList
    void Add(PipelineEventGroup&& g, std::vector<BatchedEventsList>& res) {
        std::lock_guard<std::mutex> lock(mMux);
        size_t key = g.GetTagsHash();
        EventBatchItem<T>& item = mEventQueueMap[key];

        size_t eventsSize = g.GetEvents().size();
        for (size_t i = 0; i < eventsSize; ++i) {
            PipelineEventPtr& e = g.MutableEvents()[i];
            if (!item.IsEmpty() && mEventFlushStrategy.NeedFlushByTime(item.GetStatus(), e)) {
                if (!mGroupQueue) {
                    item.Flush(res);
                } else {
                    if (!mGroupQueue->IsEmpty() && mGroupFlushStrategy->NeedFlushByTime(mGroupQueue->GetStatus())) {
                        mGroupQueue->Flush(res);
                    }
                    if (mGroupQueue->IsEmpty()) {
                        TimeoutFlushManager::GetInstance()->UpdateRecord(mFlusher->GetContext().GetConfigName(),
                                                                         0,
                                                                         0,
                                                                         mGroupFlushStrategy->GetTimeoutSecs(),
                                                                         mFlusher);
                    }
                    item.Flush(mGroupQueue.value());
                    if (mGroupFlushStrategy->NeedFlushBySize(mGroupQueue->GetStatus())) {
                        mGroupQueue->Flush(res);
                    }
                }
            }
            if (item.IsEmpty()) {
                item.Reset(g.GetSizedTags(),
                           g.GetSourceBuffer(),
                           g.GetExactlyOnceCheckpoint(),
                           g.GetMetadata(EventGroupMetaKey::SOURCE_ID));
                TimeoutFlushManager::GetInstance()->UpdateRecord(
                    mFlusher->GetContext().GetConfigName(), 0, key, mEventFlushStrategy.GetTimeoutSecs(), mFlusher);
            } else if (i == 0) {
                item.AddSourceBuffer(g.GetSourceBuffer());
            }
            item.Add(std::move(e));
            if (mEventFlushStrategy.NeedFlushBySize(item.GetStatus())
                || mEventFlushStrategy.NeedFlushByCnt(item.GetStatus())) {
                item.Flush(res);
            }
        }
    }

    // key != 0: event level queue
    // key = 0: group level queue
    void FlushQueue(size_t key, BatchedEventsList& res) {
        std::lock_guard<std::mutex> lock(mMux);
        if (key == 0) {
            if (!mGroupQueue) {
                return;
            }
            return mGroupQueue->Flush(res);
        }

        auto iter = mEventQueueMap.find(key);
        if (iter == mEventQueueMap.end()) {
            return;
        }

        if (!mGroupQueue) {
            iter->second.Flush(res);
            mEventQueueMap.erase(iter);
            return;
        }

        if (!mGroupQueue->IsEmpty() && mGroupFlushStrategy->NeedFlushByTime(mGroupQueue->GetStatus())) {
            mGroupQueue->Flush(res);
        }
        if (mGroupQueue->IsEmpty()) {
            TimeoutFlushManager::GetInstance()->UpdateRecord(
                mFlusher->GetContext().GetConfigName(), 0, 0, mGroupFlushStrategy->GetTimeoutSecs(), mFlusher);
        }
        iter->second.Flush(mGroupQueue.value());
        mEventQueueMap.erase(iter);
        if (mGroupFlushStrategy->NeedFlushBySize(mGroupQueue->GetStatus())) {
            mGroupQueue->Flush(res);
        }
    }

    void FlushAll(std::vector<BatchedEventsList>& res) {
        std::lock_guard<std::mutex> lock(mMux);
        for (auto& item : mEventQueueMap) {
            if (!mGroupQueue) {
                item.second.Flush(res);
            } else {
                if (!mGroupQueue->IsEmpty() && mGroupFlushStrategy->NeedFlushByTime(mGroupQueue->GetStatus())) {
                    mGroupQueue->Flush(res);
                }
                item.second.Flush(mGroupQueue.value());
                if (mGroupFlushStrategy->NeedFlushBySize(mGroupQueue->GetStatus())) {
                    mGroupQueue->Flush(res);
                }
            }
        }
        if (mGroupQueue) {
            mGroupQueue->Flush(res);
        }
        mEventQueueMap.clear();
    }

#ifdef APSARA_UNIT_TEST_MAIN
    EventFlushStrategy<T>& GetEventFlushStrategy() { return mEventFlushStrategy; }
    std::optional<GroupFlushStrategy>& GetGroupFlushStrategy() { return mGroupFlushStrategy; }
#endif

private:
    std::mutex mMux;
    std::map<size_t, EventBatchItem<T>> mEventQueueMap;
    EventFlushStrategy<T> mEventFlushStrategy;

    std::optional<GroupBatchItem> mGroupQueue;
    std::optional<GroupFlushStrategy> mGroupFlushStrategy;

    Flusher* mFlusher = nullptr;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class BatcherUnittest;
#endif
};

} // namespace logtail

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

#include <memory>
#include <unordered_set>
#include <vector>

#include "models/PipelineEventGroup.h"
#include "models/StringView.h"
#include "pipeline/batch/BatchStatus.h"
#include "pipeline/batch/BatchedEvents.h"
#include "pipeline/batch/FlushStrategy.h"

namespace logtail {

class GroupBatchItem {
public:
    GroupBatchItem() { mStatus.Reset(); }

    void Add(BatchedEvents&& g, int64_t totalEnqueTimeMs) {
        mEventsCnt += g.mEvents.size();
        mTotalEnqueTimeMs += totalEnqueTimeMs;
        mGroups.emplace_back(std::move(g));
        mStatus.Update(mGroups.back());
    }

    void Flush(BatchedEventsList& res) {
        if (mGroups.empty()) {
            return;
        }
        for (auto& g : mGroups) {
            res.emplace_back(std::move(g));
        }
        Clear();
    }

    void Flush(std::vector<BatchedEventsList>& res) {
        if (mGroups.empty()) {
            return;
        }
        res.emplace_back(std::move(mGroups));
        Clear();
    }

    GroupBatchStatus& GetStatus() { return mStatus; }
    size_t GroupSize() const { return mGroups.size(); }
    size_t EventSize() const { return mEventsCnt; }
    size_t DataSize() const { return mStatus.GetSize(); }
    int64_t TotalEnqueTimeMs() const { return mTotalEnqueTimeMs; }

    bool IsEmpty() { return mGroups.empty(); }

private:
    void Clear() {
        mGroups.clear();
        mStatus.Reset();
        mEventsCnt = 0;
        mTotalEnqueTimeMs = 0;
    }

    std::vector<BatchedEvents> mGroups;

    GroupBatchStatus mStatus;
    size_t mEventsCnt = 0;
    // if more than 10^6 events are contained in the batch, the value may overflow
    // however, this is almost impossible in practice
    int64_t mTotalEnqueTimeMs = 0;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class EventBatchItemUnittest;
    friend class GroupBatchItemUnittest;
    friend class BatcherUnittest;
#endif
};

template <typename T = EventBatchStatus>
class EventBatchItem {
public:
    void Add(PipelineEventPtr&& e) {
        mBatch.mEvents.emplace_back(std::move(e));
        mStatus.Update(mBatch.mEvents.back());
        mTotalEnqueTimeMs += std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now())
                                 .time_since_epoch()
                                 .count();
    }

    void Flush(GroupBatchItem& res) {
        if (mBatch.mEvents.empty()) {
            return;
        }
        if (mBatch.mExactlyOnceCheckpoint) {
            UpdateExactlyOnceLogPosition();
        }
        mBatch.mSizeBytes = DataSize();
        res.Add(std::move(mBatch), mTotalEnqueTimeMs);
        Clear();
    }

    void Flush(BatchedEventsList& res) {
        if (mBatch.mEvents.empty()) {
            return;
        }
        if (mBatch.mExactlyOnceCheckpoint) {
            UpdateExactlyOnceLogPosition();
        }
        mBatch.mSizeBytes = DataSize();
        res.emplace_back(std::move(mBatch));
        Clear();
    }

    void Flush(std::vector<BatchedEventsList>& res) {
        if (mBatch.mEvents.empty()) {
            return;
        }
        res.emplace_back();
        if (mBatch.mExactlyOnceCheckpoint) {
            UpdateExactlyOnceLogPosition();
        }
        mBatch.mSizeBytes = DataSize();
        res.back().emplace_back(std::move(mBatch));
        Clear();
    }

    void Reset(const SizedMap& tags,
               const std::shared_ptr<SourceBuffer>& sourceBuffer,
               const RangeCheckpointPtr& exactlyOnceCheckpoint,
               StringView packIdPrefix) {
        Clear();
        // should be copied instead of moved in case of one original group splitted into two
        mBatch.mTags = tags;
        mBatch.mExactlyOnceCheckpoint = exactlyOnceCheckpoint;
        mBatch.mPackIdPrefix = packIdPrefix;
        AddSourceBuffer(sourceBuffer);
    }

    void AddSourceBuffer(const std::shared_ptr<SourceBuffer>& sourceBuffer) {
        if (mSourceBuffers.find(sourceBuffer.get()) == mSourceBuffers.end()) {
            mSourceBuffers.insert(sourceBuffer.get());
            mBatch.mSourceBuffers.emplace_back(sourceBuffer);
        }
    }

    T& GetStatus() { return mStatus; }

    bool IsEmpty() { return mBatch.mEvents.empty(); }

    size_t DataSize() const { return sizeof(decltype(mBatch.mEvents)) + mStatus.GetSize() + mBatch.mTags.DataSize(); }
    size_t EventSize() const { return mBatch.mEvents.size(); }
    int64_t TotalEnqueTimeMs() const { return mTotalEnqueTimeMs; }

private:
    void Clear() {
        mBatch.Clear();
        mSourceBuffers.clear();
        mStatus.Reset();
        mTotalEnqueTimeMs = 0;
    }

    void UpdateExactlyOnceLogPosition() {
        uint32_t offset = mBatch.mEvents.front().Cast<LogEvent>().GetPosition().first;
        auto lastEventPosition = mBatch.mEvents.back().Cast<LogEvent>().GetPosition();
        mBatch.mExactlyOnceCheckpoint->data.set_read_offset(offset);
        mBatch.mExactlyOnceCheckpoint->data.set_read_length(lastEventPosition.first + lastEventPosition.second
                                                            - offset);
    }

    BatchedEvents mBatch;
    std::unordered_set<SourceBuffer*> mSourceBuffers;
    T mStatus;
    // if more than 10^6 events are contained in the batch, the value may overflow
    // however, this is almost impossible in practice
    int64_t mTotalEnqueTimeMs = 0;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class EventBatchItemUnittest;
    friend class BatcherUnittest;
#endif
};

} // namespace logtail

// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "pipeline/queue/SenderQueue.h"

#include "logger/Logger.h"

using namespace std;

namespace logtail {

SenderQueue::SenderQueue(
    size_t cap, size_t low, size_t high, QueueKey key, const string& flusherId, const PipelineContext& ctx)
    : QueueInterface(key, cap, ctx), BoundedSenderQueueInterface(cap, low, high, key, flusherId, ctx) {
    mQueue.resize(cap);
    mFetchTimesCnt = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_QUEUE_GET_ITEMS_TIMES_TOTAL);
    mFetchItemsCnt = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_QUEUE_GET_ITEMS_TOTAL);    
    WriteMetrics::GetInstance()->CommitMetricsRecordRef(mMetricsRecordRef);    
}

bool SenderQueue::Push(unique_ptr<SenderQueueItem>&& item) {
    item->mEnqueTime = chrono::system_clock::now();
    auto size = item->mData.size();

    mInItemsTotal->Add(1);
    mInItemDataSizeBytes->Add(size);

    if (Full()) {
        mExtraBuffer.push(std::move(item));

        mExtraBufferSize->Set(mExtraBuffer.size());
        mExtraBufferDataSizeBytes->Add(size);
        return true;
    }

    size_t index = mRead;
    for (; index < mWrite; ++index) {
        if (mQueue[index % mCapacity] == nullptr) {
            break;
        }
    }
    mQueue[index % mCapacity] = std::move(item);
    if (index == mWrite) {
        ++mWrite;
    }
    ++mSize;
    ChangeStateIfNeededAfterPush();

    mQueueSizeTotal->Set(Size());
    mQueueDataSizeByte->Add(size);
    mValidToPushFlag->Set(IsValidToPush());
    return true;
}

bool SenderQueue::Remove(SenderQueueItem* item) {
    if (item == nullptr) {
        return false;
    }
    
    size_t size = 0;
    chrono::system_clock::time_point enQueuTime;
    auto index = mRead;
    for (; index < mWrite; ++index) {
        if (mQueue[index % mCapacity].get() == item) {
            size = item->mData.size();
            enQueuTime = item->mEnqueTime;
            mQueue[index % mCapacity].reset();
            break;
        }
    }
    if (index == mWrite) {
        return false;
    }
    while (mRead < mWrite && mQueue[mRead % mCapacity] == nullptr) {
        ++mRead;
    }
    --mSize;

    mOutItemsTotal->Add(1);
    mTotalDelayMs->Add(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - enQueuTime).count());
    mQueueDataSizeByte->Sub(size);

    if (!mExtraBuffer.empty()) {
        auto newSize = mExtraBuffer.front()->mData.size();
        Push(std::move(mExtraBuffer.front()));
        mExtraBuffer.pop();

        mExtraBufferSize->Set(mExtraBuffer.size());
        mExtraBufferDataSizeBytes->Sub(newSize);
        return true;
    }
    if (ChangeStateIfNeededAfterPop()) {
        GiveFeedback();
    }

    mQueueSizeTotal->Set(Size());
    mValidToPushFlag->Set(IsValidToPush());
    return true;
}


void SenderQueue::GetLimitAvailableItems(vector<SenderQueueItem*>& items, int32_t limit) {
    mFetchTimesCnt->Add(1);
    if (Empty()) {
        return;
    }
    for (auto index = mRead; index < mWrite; ++index) {
        SenderQueueItem* item = mQueue[index % mCapacity].get();
        if (item == nullptr) {
            continue;
        }
        mFetchItemsCnt->Add(1);
        if (mRateLimiter && !mRateLimiter->IsValidToPop()) {
            mLimitByRateLimiterCnt->Add(1);
            return;
        }
        for (auto& limiter : mConcurrencyLimiters) {
            if (!limiter.first->IsValidToPop()) {
                limiter.second->Add(1);
                return;
            }
        }
        
        if (item->mStatus.Get() == SendingStatus::IDLE) {
            item->mStatus.Set(SendingStatus::SENDING);
            items.emplace_back(item);
            for (auto& limiter : mConcurrencyLimiters) {
                if (limiter.first != nullptr) {
                    limiter.first->PostPop();
                }
            }
            if (mRateLimiter) {
                mRateLimiter->PostPop(item->mRawSize);
            }
        }
        --limit;
        if (limit <= 0) {
            return;
        }
    }
}


void SenderQueue::GetAllAvailableItems(vector<SenderQueueItem*>& items) {
    if (Empty()) {
        return;
    }
    for (auto index = mRead; index < mWrite; ++index) {
        SenderQueueItem* item = mQueue[index % mCapacity].get();
        if (item == nullptr) {
            continue;
        }
        if (item->mStatus.Get() == SendingStatus::IDLE) {
            item->mStatus.Set(SendingStatus::SENDING);
            items.emplace_back(item);
        }
    }
}


} // namespace logtail

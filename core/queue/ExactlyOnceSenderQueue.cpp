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

#include "queue/ExactlyOnceSenderQueue.h"

#include <iostream>

#include "flusher/sls/FlusherSLS.h"
#include "logger/Logger.h"
#include "queue/SLSSenderQueueItem.h"

using namespace std;

namespace logtail {

bool ExactlyOnceSenderQueue::Push(unique_ptr<SenderQueueItem>&& item) {
    if (item == nullptr) {
        return false;
    }

    if (!mIsInitialised) {
        const auto f = static_cast<const FlusherSLS*>(item->mFlusher);
        if (f->mMaxSendRate > 0) {
            mRateLimiter = RateLimiter(f->mMaxSendRate);
        }
        mConcurrencyLimiters.emplace_back(FlusherSLS::GetRegionConcurrencyLimiter(f->mRegion));
        mConcurrencyLimiters.emplace_back(FlusherSLS::GetProjectConcurrencyLimiter(f->mProject));
        mIsInitialised = true;
    }

    auto ptr = static_cast<SLSSenderQueueItem*>(item.get());
    auto& eo = ptr->mExactlyOnceCheckpoint;
    if (eo->IsComplete()) {
        if (mQueue[eo->index] != nullptr) {
            // should not happen
            return false;
        }
        item->mEnqueTime = time(nullptr);
        mQueue[eo->index] = std::move(item);
    } else {
        for (size_t idx = 0; idx < mCapacity; ++idx, ++mWrite) {
            auto index = mWrite % mCapacity;
            if (mQueue[index] != nullptr) {
                continue;
            }
            item->mEnqueTime = time(nullptr);
            mQueue[index] = std::move(item);
            auto& newCpt = mRangeCheckpoints[index];
            newCpt->data.set_read_offset(eo->data.read_offset());
            newCpt->data.set_read_length(eo->data.read_length());
            eo = newCpt;
            ptr->mShardHashKey = eo->data.hash_key();
            ++mWrite;
            break;
        }
        if (!eo->IsComplete()) {
            item->mEnqueTime = time(nullptr);
            mExtraBuffer.push(std::move(item));
            return true;
        }
    }
    eo->Prepare();
    ++mSize;
    ChangeStateIfNeededAfterPush();
    return true;
}

bool ExactlyOnceSenderQueue::Remove(SenderQueueItem* item) {
    if (item == nullptr) {
        return false;
    }
    auto& eo = static_cast<SLSSenderQueueItem*>(item)->mExactlyOnceCheckpoint;
    if (eo->index >= mCapacity || mQueue[eo->index] == nullptr) {
        // should not happen
        return false;
    }
    mQueue[eo->index].reset();
    --mSize;

    if (!mExtraBuffer.empty()) {
        Push(std::move(mExtraBuffer.front()));
        mExtraBuffer.pop();
        return true;
    }
    if (ChangeStateIfNeededAfterPop()) {
        GiveFeedback();
    }
    return true;
}

void ExactlyOnceSenderQueue::GetAllAvailableItems(vector<SenderQueueItem*>& items, bool withLimits) {
    if (Empty()) {
        return;
    }
    for (size_t index = 0; index < mCapacity; ++index) {
        SenderQueueItem* item = mQueue[index].get();
        if (item == nullptr) {
            continue;
        }
        if (withLimits) {
            if (mRateLimiter && !mRateLimiter->IsValidToPop()) {
                return;
            }
            for (auto& limiter : mConcurrencyLimiters) {
                if (!limiter->IsValidToPop()) {
                    return;
                }
            }
        }
        if (item->mStatus == SendingStatus::IDLE) {
            item->mStatus = SendingStatus::SENDING;
            items.emplace_back(item);
            if (withLimits) {
                for (auto& limiter : mConcurrencyLimiters) {
                    limiter->PostPop();
                }
                if (mRateLimiter) {
                    mRateLimiter->PostPop(item->mRawSize);
                }
            }
        }
    }
}

void ExactlyOnceSenderQueue::Reset(const vector<RangeCheckpointPtr>& checkpoints) {
    BoundedSenderQueueInterface::Reset(checkpoints.size(), checkpoints.size() - 1, checkpoints.size());
    mQueue.resize(checkpoints.size());
    mQueue.clear();
    mWrite = mSize = 0;
    mRangeCheckpoints = checkpoints;
}

} // namespace logtail

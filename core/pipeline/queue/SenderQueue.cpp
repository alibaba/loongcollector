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

bool SenderQueue::Push(unique_ptr<SenderQueueItem>&& item) {
    if (Full()) {
        item->mEnqueTime = time(nullptr);
        mExtraBuffer.push(std::move(item));
        return true;
    }

    size_t index = mRead;
    for (; index < mWrite; ++index) {
        if (mQueue[index % mCapacity] == nullptr) {
            break;
        }
    }
    item->mEnqueTime = time(nullptr);
    mQueue[index % mCapacity] = std::move(item);
    if (index == mWrite) {
        ++mWrite;
    }
    ++mSize;
    ChangeStateIfNeededAfterPush();
    return true;
}

bool SenderQueue::Remove(SenderQueueItem* item) {
    auto index = mRead;
    for (; index < mWrite; ++index) {
        if (mQueue[index % mCapacity].get() == item) {
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

void SenderQueue::GetAllAvailableItems(vector<SenderQueueItem*>& items, bool withLimits) {
    if (Empty()) {
        return;
    }
    for (auto index = mRead; index < mWrite; ++index) {
        SenderQueueItem* item = mQueue[index % mCapacity].get();
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
                    if (limiter != nullptr) {
                        limiter->PostPop();
                    }
                }
                if (mRateLimiter) {
                    mRateLimiter->PostPop(item->mRawSize);
                }
            }
        }
    }
}

} // namespace logtail

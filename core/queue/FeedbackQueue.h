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

#include "queue/FeedbackQueueKey.h"

namespace logtail {

template <typename T>
class FeedbackQueue {
public:
    FeedbackQueue(QueueKey key, size_t cap, size_t low, size_t high)
        : mKey(key), mCapacity(cap), mLowWatermark(low), mHighWatermark(high) {}

    FeedbackQueue(const FeedbackQueue& que) = delete;
    FeedbackQueue& operator=(const FeedbackQueue&) = delete;

    virtual bool Push(T&& item) = 0;
    virtual bool Pop(T& item) = 0;

    bool IsValidToPush() const { return mValidToPush; }

    bool Empty() const { return Size() == 0; }

    QueueKey GetKey() const { return mKey; }

    void InvalidatePop() { mValidToPop = false; }
    void ValidatePop() { mValidToPop = true; }

protected:
    bool IsValidToPop() const { return mValidToPop && !Empty() && IsDownStreamQueuesValidToPush(); }

    bool ChangeStateIfNeededAfterPush() {
        if (Size() == mHighWatermark) {
            mValidToPush = false;
            return true;
        }
        return false;
    }

    bool ChangeStateIfNeededAfterPop() {
        if (!mValidToPush && Size() == mLowWatermark) {
            mValidToPush = true;
            return true;
        }
        return false;
    }

    const QueueKey mKey;

private:
    virtual size_t Size() const = 0;

    bool Full() const { return Size() == mCapacity; }

    virtual bool IsDownStreamQueuesValidToPush() const = 0;

    const size_t mCapacity;

    size_t mLowWatermark;
    size_t mHighWatermark;

    bool mValidToPush = true;
    bool mValidToPop = true;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessQueueManagerUnittest;
    friend class ExactlyOnceQueueManagerUnittest;
#endif
};

} // namespace logtail

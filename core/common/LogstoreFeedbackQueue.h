/*
 * Copyright 2022 iLogtail Authors
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
#include <stdlib.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "Lock.h"
#include "MemoryBarrier.h"
#include "WaitObject.h"

namespace logtail {

enum class QueueType { Normal, ExactlyOnce };

class TriggerEvent {
public:
    TriggerEvent() : mTriggerState(false) {}

    bool Wait(int32_t waitMs) {
        WaitObject::Lock lock(mWaitObj);
        ReadWriteBarrier();
        if (mTriggerState) {
            mTriggerState = false;
            return true;
        }
        mWaitObj.wait(lock, (int64_t)waitMs * (int64_t)1000);
        if (mTriggerState) {
            mTriggerState = false;
            return true;
        }
        return false;
    }

    void Trigger() {
        WaitObject::Lock lock(mWaitObj);
        {
            ReadWriteBarrier();
            mTriggerState = true;
        }
        mWaitObj.signal();
    }

protected:
    WaitObject mWaitObj;
    volatile bool mTriggerState;
};

template <class TT, class PARAM>
class SingleLogstoreFeedbackQueue {
public:
    SingleLogstoreFeedbackQueue() : mType(QueueType::Normal) {
        ResetParam(PARAM::GetInstance()->GetLowSize(),
                   PARAM::GetInstance()->GetHighSize(),
                   PARAM::GetInstance()->GetMaxSize());
    }

    SingleLogstoreFeedbackQueue(const SingleLogstoreFeedbackQueue& que) = delete;
    SingleLogstoreFeedbackQueue& operator=(const SingleLogstoreFeedbackQueue&) = delete;

    void SetType(QueueType type) { mType = type; }

    void ConvertToExactlyOnceQueue(const size_t lowSize, const size_t highSize, const size_t maxSize) {
        SetType(QueueType::ExactlyOnce);
        ResetParam(lowSize, highSize, maxSize);
    }

    void ResetParam(const size_t lowSize, const size_t highSize, const size_t maxSize) {
        LOW_SIZE = lowSize;
        HIGH_SIZE = highSize;
        SIZE = maxSize;

        mArray.resize(SIZE);
        mArray.clear();
        Reset();
    }

    void Reset() {
        mValid = true;
        mWrite = mRead = mSize = 0;
    }

    bool IsValid() const { return mValid; }

    bool IsEmpty() const { return mSize == 0; }

    bool IsFull() const { return mSize == SIZE; }

    bool PushItem(const TT& item) {
        if (IsFull()) {
            return false;
        }

        mArray[mWrite % SIZE] = item;
        ++mWrite;
        ++mSize;
        if (mSize == HIGH_SIZE) {
            mValid = false;
        }
        return true;
    }

    bool PushItem(TT&& item) {
        if (IsFull()) {
            return false;
        }

        mArray[mWrite % SIZE] = std::move(item);
        ++mWrite;
        ++mSize;
        if (mSize == HIGH_SIZE) {
            mValid = false;
        }
        return true;
    }

    // Pop an item from queue.
    //
    // @return:
    // - 0: the queue is empty.
    // - 1: item is poped and the queue's status is unchanged.
    // - 2: item is poped and the queue becomes valid.
    int PopItem(TT& item) {
        if (IsEmpty()) {
            return 0;
        }

        int rst = 1;
        item = std::move(mArray[mRead % SIZE]);
        ++mRead;
        if (--mSize == LOW_SIZE && !mValid) {
            mValid = true;
            rst = 2;
        }
        return rst;
    }

    size_t GetSize() const { return mSize; }

    QueueType GetQueueType() const { return mType; }

protected:
    size_t LOW_SIZE;
    size_t HIGH_SIZE;
    size_t SIZE;

    std::vector<TT> mArray;
    volatile bool mValid;
    volatile uint64_t mWrite;
    volatile uint64_t mRead;
    volatile size_t mSize;
    volatile QueueType mType;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessQueueUnittest;
    friend class ExactlyOnceQueueManagerUnittest;
#endif
};

} // namespace logtail

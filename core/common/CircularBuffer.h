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
#include <atomic>
#include "MemoryBarrier.h"
#include "Semaphore.h"

namespace logtail {

template <class T, size_t N = 1000>
class AbstractCircularBuffer {
public:
    virtual ~AbstractCircularBuffer() {}

    virtual bool TryPushItem(const T&) = 0;
    virtual void PushItem(const T&) = 0;
    virtual bool TryPopItem(T&) = 0;
    virtual void PopItem(T&) = 0;
    virtual size_t GetItemNumber() = 0;
};

template <class T, size_t N = 1000>
class CircularBuffer : public AbstractCircularBuffer<T, N> {
public:
    CircularBuffer() { mWriter = mReader = 0; }

    bool TryPushItem(const T& item) {
        size_t tmpWriter = mWriter;
        size_t tmpReader = mReader;
        size_t nextWriter = (tmpWriter + 1) % SIZE;

        //ensure we check the mReader *before* write the item into mData
        ReadWriteBarrier();

        if (nextWriter == tmpReader) {
            //the buffer is full
            return false;
        }

        //cast off "volatile"
        const_cast<T&>(mData[tmpWriter]) = item;

        //ensure the item writes into mData *before* we update mWriter
        WriteBarrier();

        mWriter = nextWriter;
        mItemNumber++;
        return true;
    }

    void PushItem(const T& item) {
        while (true) {
            if (TryPushItem(item)) {
                return;
            }
            usleep(SLEEP_TIME); //microseconds
        }
    }

    bool TryPopItem(T& item) {
        size_t tmpWriter = mWriter;
        size_t tmpReader = mReader;

        //ensure we check the mWriter *before* read the item from mData
        ReadBarrier();

        if (tmpWriter == tmpReader) {
            //the buffer is empty
            return false;
        }

        //cast off "volatile"
        item = const_cast<T&>(mData[tmpReader]);

        //ensure we read the item *before* update mReader
        ReadWriteBarrier();

        size_t NextReader = (tmpReader + 1) % SIZE;
        mReader = NextReader;
        mItemNumber--;
        return true;
    }
    void PopItem(T& item) {
        while (true) {
            if (TryPopItem(item)) {
                return;
            }
            usleep(SLEEP_TIME); //microseconds
        }
    }

    size_t GetItemNumber() { return mItemNumber; }

private:
    //Because we always "Keep One Item Open", the actually size should be N + 1
    static const size_t SIZE = N + 1;
    static const int SLEEP_TIME = 10; //microsecond
    volatile size_t mWriter;
    volatile size_t mReader;
    volatile T mData[SIZE];
    std::atomic<size_t> mItemNumber;
};

template <class T, size_t N = 1000>
class CircularBufferSem : public CircularBuffer<T, N> {
public:
    CircularBufferSem() : mSemFull(N), mSemEmpty(0) {}

    bool TryPushItem(const T& item) {
        if (CircularBuffer<T, N>::TryPushItem(item)) {
            mSemEmpty.Post();
            return true;
        } else {
            return false;
        }
    }

    void PushItem(const T& item) {
        mSemFull.Wait();
        bool result;
        result = TryPushItem(item);
        assert(result);
        (void)result;
    }

    bool TryPopItem(T& item) {
        if (CircularBuffer<T, N>::TryPopItem(item)) {
            mSemFull.Post();
            return true;
        } else {
            return false;
        }
    }

    void PopItem(T& item) {
        mSemEmpty.Wait();
        bool result;
        result = TryPopItem(item);
        assert(result);
        (void)result;
    }

private:
    Semaphore mSemFull;
    Semaphore mSemEmpty;
};

} // namespace logtail
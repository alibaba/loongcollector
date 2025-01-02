// Copyright 2023 iLogtail Authors
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


#pragma once

#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <chrono>

#include "common/queue/blockingconcurrentqueue.h"
#include "logger/Logger.h"

namespace logtail {
namespace ebpf {

template <typename T>
class Consumer {
public:
    using ProcessFunc = std::function<bool(const std::vector<T>&, size_t)>;
    Consumer(moodycamel::BlockingConcurrentQueue<T>& queue, std::chrono::milliseconds maxWaitTime, const ProcessFunc& func)
        : mQueue(queue), mMaxWaitTime(maxWaitTime), mStopFlag(false), mSuspendFlag(false), mProcessFunc(func) {}

    void start() {
        mWorker = std::thread(&Consumer::run, this);
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mMtx);
            mStopFlag = true;
            mCv.notify_all();
        }
        if (mWorker.joinable()) {
            mWorker.join();
        }
    }

    void suspend() {
        std::unique_lock<std::mutex> lock(mMtx);
        mSuspendFlag = true;
    }

    void resume() {
        {
            std::unique_lock<std::mutex> lock(mMtx);
            mSuspendFlag = false;
        }
        mCv.notify_all();
    }

private:
    void run() {
        std::vector<T> items(1024);
        while (!mStopFlag) {
            {
                std::unique_lock<std::mutex> lock(mMtx);
                mCv.wait(lock, [this] { return !mSuspendFlag || mStopFlag; });
            }

            if (mStopFlag) break;

            LOG_INFO(sLogger, ("begin process", ""));
            size_t numItemsDequeued = mQueue.wait_dequeue_bulk_timed(items.begin(), items.size(), mMaxWaitTime);
            if (numItemsDequeued > 0) {
                mProcessFunc(items, numItemsDequeued);
            }
        }
    }

    moodycamel::BlockingConcurrentQueue<T>& mQueue;
    std::thread mWorker;
    std::chrono::milliseconds mMaxWaitTime;
    std::atomic<bool> mStopFlag;
    std::atomic<bool> mSuspendFlag;
    std::mutex mMtx;
    std::condition_variable mCv;
    ProcessFunc mProcessFunc;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class ConsumerUnittest;
#endif
};

}
}

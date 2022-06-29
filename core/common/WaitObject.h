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
#include <mutex>
#include <condition_variable>
#include <cstdint>
#include <chrono>

namespace logtail {

class WaitObject {
    std::mutex mMutex;
    std::condition_variable mCond;

    WaitObject(const WaitObject&) = delete;
    WaitObject& operator=(const WaitObject&) = delete;

public:
    class Lock {
        friend class WaitObject;
        std::unique_lock<std::mutex> mImpl;

    public:
        Lock(WaitObject& wo) : mImpl(wo.mMutex) {}
    };

    WaitObject() {}

    void signal() { mCond.notify_one(); }

    void broadcast() { mCond.notify_all(); }

    void wait(Lock& lk, int64_t timeout = -1) {
        if (timeout > 0) {
            mCond.wait_for(lk.mImpl, std::chrono::microseconds(timeout));
        } else {
            mCond.wait(lk.mImpl);
        }
    }

    void lock() { mMutex.lock(); }
    void unlock() { mMutex.unlock(); }
};

} // namespace logtail

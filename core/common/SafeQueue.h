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

#include <condition_variable>
#include <mutex>
#include <queue>

namespace logtail {

// lock-based unbounded queue, suitable for common use (except for process queue and sender queue)
template <typename T>
class SafeQueue {
public:
    void Push(T&& data) {
        std::lock_guard<std::mutex> lock(mMux);
        mQueue.push(std::move(data));
        mCond.notify_all();
    }

    bool WaitAndPop(T& value, int64_t ms) {
        std::unique_lock<std::mutex> lock(mMux);
        if (!mCond.wait_for(lock, std::chrono::milliseconds(ms), [this] { return !mQueue.empty(); })) {
            return false;
        }
        value = std::move(mQueue.front());
        mQueue.pop();
        return true;
    }

    bool TryPop(T& value) {
        std::lock_guard<std::mutex> lock(mMux);
        if (mQueue.empty()) {
            return false;
        }
        value = std::move(mQueue.front());
        mQueue.pop();
        return true;
    }

    bool WaitAndPopAll(std::vector<T>& values, int64_t ms) {
        std::unique_lock<std::mutex> lock(mMux);
        if (!mCond.wait_for(lock, std::chrono::milliseconds(ms), [this] { return !mQueue.empty(); })) {
            return false;
        }
        while (!mQueue.empty()) {
            values.push_back(std::move(mQueue.front()));
            mQueue.pop();
        }
        return true;
    }

    bool Empty() const {
        std::lock_guard<std::mutex> lock(mMux);
        return mQueue.empty();
    }

    size_t Size() const {
        std::lock_guard<std::mutex> lock(mMux);
        return mQueue.size();
    }

private:
    std::queue<T> mQueue;
    mutable std::mutex mMux;
    mutable std::condition_variable mCond;
};

} // namespace logtail

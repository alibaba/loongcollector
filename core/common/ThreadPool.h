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
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <vector>
#include <queue>

namespace logtail {

class ThreadPool {
public:
    using Task = std::function<void()>;

    explicit ThreadPool(size_t num) : mIsRunning(false), mThreadNum(num) {}

    ~ThreadPool() {
        if (mIsRunning) {
            Stop();
        }
    }

    void Start() {
        mIsRunning = true;
        for (size_t i = 0; i < mThreadNum; i++) {
            mThreads.emplace_back(std::thread(&ThreadPool::execute, this));
        }
    }

    void Stop() {
        {
            std::unique_lock<std::mutex> lock(mMutex);
            mIsRunning = false;
            mCond.notify_all();
        }

        for (auto& t : mThreads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    void Add(const Task& task) {
        if (mIsRunning) {
            std::unique_lock<std::mutex> lock(mMutex);
            mTasks.push(task);
            mCond.notify_one();
        }
    }

    size_t Size() const {
        if (!mIsRunning) {
            return 0;
        }
        std::unique_lock<std::mutex> lock(mMutex);
        return mTasks.size();
    }

private:
    void execute() {
        while (mIsRunning) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(mMutex);
                if (!mTasks.empty()) {
                    task = mTasks.front();
                    mTasks.pop();
                } else if (mIsRunning && mTasks.empty()) {
                    mCond.wait(lock);
                }
            }

            if (task) {
                task();
            }
        }
    }

public:
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool& other) = delete;

private:
    volatile bool mIsRunning;
    mutable std::mutex mMutex;
    std::condition_variable mCond;
    size_t mThreadNum;
    std::vector<std::thread> mThreads;
    std::queue<Task> mTasks;
};

} // namespace logtail

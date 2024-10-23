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

#include "common/timer/Timer.h"

#include "logger/Logger.h"

using namespace std;

namespace logtail {

void Timer::Init() {
    {
        lock_guard<mutex> lock(mThreadRunningMux);
        mIsThreadRunning = true;
    }
    mThreadRes = async(launch::async, &Timer::Run, this);
}

void Timer::Stop() {
    {
        lock_guard<mutex> lock(mThreadRunningMux);
        mIsThreadRunning = false;
    }
    mCV.notify_one();
    future_status s = mThreadRes.wait_for(chrono::seconds(1));
    if (s == future_status::ready) {
        LOG_INFO(sLogger, ("timer", "stopped successfully"));
    } else {
        LOG_WARNING(sLogger, ("timer", "forced to stopped"));
    }
}

void Timer::PushEvent(unique_ptr<TimerEvent>&& e) {
    lock_guard<mutex> lock(mQueueMux);
    if (mQueue.empty() || e->GetExecTime() < mQueue.top()->GetExecTime()) {
        mQueue.push(std::move(e));
        mCV.notify_one();
    } else {
        mQueue.push(std::move(e));
    }
}

void Timer::Run() {
    LOG_INFO(sLogger, ("timer", "started"));
    unique_lock<mutex> threadLock(mThreadRunningMux);
    while (mIsThreadRunning) {
        unique_lock<mutex> queueLock(mQueueMux);
        if (mQueue.empty()) {
            queueLock.unlock();
            mCV.wait(threadLock, [this]() { return !mIsThreadRunning || !mQueue.empty(); });
        } else {
            auto now = chrono::steady_clock::now();
            while (!mQueue.empty()) {
                auto& e = mQueue.top();
                if (now < e->GetExecTime()) {
                    auto timeout = e->GetExecTime() - now + chrono::milliseconds(1);
                    queueLock.unlock();
                    mCV.wait_for(threadLock, timeout);
                    break;
                } else {
                    auto e = std::move(const_cast<unique_ptr<TimerEvent>&>(mQueue.top()));
                    mQueue.pop();
                    queueLock.unlock();
                    if (!e->IsValid()) {
                        LOG_INFO(sLogger, ("invalid timer event", "task is cancelled"));
                    } else {
                        e->Execute();
                    }
                    queueLock.lock();
                }
            }
        }
    }
}

} // namespace logtail

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
#include <future>
#include <memory>
#include <mutex>
#include <queue>

#include "common/timer/TimerEvent.h"

namespace logtail {

struct TimerEventCompare {
    bool operator()(const std::unique_ptr<TimerEvent>& lhs, const std::unique_ptr<TimerEvent>& rhs) const {
        return lhs->GetExecTime() > rhs->GetExecTime();
    }
};

class Timer {
public:
    Timer(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer& operator=(Timer&&) = delete;
    ~Timer() = default;
    static Timer* GetInstance() {
        static Timer sInstance;
        return &sInstance;
    }
    void Init();
    void Stop();
    void PushEvent(std::unique_ptr<TimerEvent>&& e);

private:
    Timer() = default;
    void Run();

    mutable std::mutex mQueueMux;
    std::priority_queue<std::unique_ptr<TimerEvent>, std::vector<std::unique_ptr<TimerEvent>>, TimerEventCompare>
        mQueue;

    std::future<void> mThreadRes;
    mutable std::mutex mThreadRunningMux;
    bool mIsThreadRunning = true;
    mutable std::condition_variable mCV;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class TimerUnittest;
    friend class ScrapeSchedulerUnittest;
    friend class HostMonitorInputRunnerUnittest;
#endif
};

} // namespace logtail

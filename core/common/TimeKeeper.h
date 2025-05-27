// Copyright 2025 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstdint>

#include <atomic>
#include <chrono>
#include <thread>

namespace logtail {
class TimeKeeper {
private:
    // alignas(8) is unnecessary on most 64-bit OS
    // volatile is enough to ensure visibility in multithread env
    // int64_t itself is atomic on x86-64 and ARMv8+ archs
    volatile int64_t mCurrentTimeSec = 0;
    volatile int64_t mCurrentTimeMs = 0;
    volatile int64_t mCurrentTimeUs = 0;
    volatile int64_t mCurrentTimeNs = 0;
    volatile int64_t mKtimeNs = 0;

    std::thread mUpdateThread;
    std::atomic<bool> mShouldStop{false};

public:
    static TimeKeeper* GetInstance();

    TimeKeeper();
    ~TimeKeeper();
    TimeKeeper(const TimeKeeper&) = delete;
    TimeKeeper& operator=(const TimeKeeper&) = delete;
    TimeKeeper(TimeKeeper&&) = delete;
    TimeKeeper& operator=(TimeKeeper&&) = delete;
    void UpdateTime();
    void Stop();

    [[nodiscard]] int64_t NowSec() const { return mCurrentTimeSec; }
    [[nodiscard]] int64_t NowMs() const { return mCurrentTimeMs; }
    [[nodiscard]] int64_t NowUs() const { return mCurrentTimeUs; }
    [[nodiscard]] int64_t NowNs() const { return mCurrentTimeNs; }
    [[nodiscard]] int64_t KtimeNs() const { return mKtimeNs; }
};
} // namespace logtail

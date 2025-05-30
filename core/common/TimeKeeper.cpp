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

#include "common/TimeKeeper.h"

#include "common/TimeUtil.h"

namespace logtail {

TimeKeeper* TimeKeeper::GetInstance() {
    static TimeKeeper sInstance;
    return &sInstance;
}

TimeKeeper::TimeKeeper() {
    mUpdateThread = std::thread([this]() {
        while (!mShouldStop) {
            UpdateTime();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });
}

TimeKeeper::~TimeKeeper() {
    mShouldStop = true;
    if (mUpdateThread.joinable()) {
        mUpdateThread.join();
    }
}

void TimeKeeper::UpdateTime() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    mCurrentTimeSec = std::chrono::duration_cast<std::chrono::seconds>(now).count();
    mCurrentTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    mCurrentTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    mCurrentTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    static auto sDiff = GetTimeDiffFromMonotonic().count();
    mKtimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count() - sDiff;
}

void TimeKeeper::Stop() {
    mShouldStop = true;
}

} // namespace logtail

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

#include <utility>

#include "common/timer/Timer.h"

namespace logtail {
namespace ebpf {

class AggregateEvent : public PeriodicalTimerEvent {
public:
    AggregateEvent(int interval,
                   std::function<bool(const std::chrono::steady_clock::time_point&)> handler,
                   std::function<bool(int)> stopChecker,
                   int uid = 0)
        : PeriodicalTimerEvent(interval),
          mUid(uid),
          mHandler(std::move(handler)),
          mStopChecker(std::move(stopChecker)) {}

    bool IsValid() const override { return true; }

    bool Execute() override { return mHandler(mExecTime); }

    bool IsStop() override { return mStopChecker(mUid); }

private:
    int mUid;
    std::function<bool(const std::chrono::steady_clock::time_point& execTime)> mHandler;
    std::function<bool(int)> mStopChecker;
};

} // namespace ebpf
} // namespace logtail

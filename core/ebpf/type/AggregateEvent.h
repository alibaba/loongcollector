// Copyright 2025 iLogtail Authors
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

#include <memory>
#include <utility>

#include "common/timer/Timer.h"
#include "ebpf/include/export.h"

namespace logtail {
namespace ebpf {

class ScheduleConfig {
public:
    PluginType mType;
    std::chrono::seconds mInterval;
    ScheduleConfig(PluginType type, const std::chrono::seconds& interval) : mType(type), mInterval(interval) {}
};

class AggregateEventV2 : public TimerEvent {
public:
    AggregateEventV2(const std::chrono::steady_clock::time_point& execTime,
                     const std::shared_ptr<ScheduleConfig> config)
        : TimerEvent(execTime), mScheduleConfig(config) {}

    bool IsValid() const override;
    bool Execute() override;

private:
    std::shared_ptr<ScheduleConfig> mScheduleConfig;
};

// class AggregateEvent : public PeriodicalTimerEvent {
// public:
//     AggregateEvent(int interval,
//                    std::function<bool(const std::chrono::steady_clock::time_point&)> handler,
//                    std::function<bool()> stopChecker)
//         : PeriodicalTimerEvent(interval), mHandler(std::move(handler)), mStopChecker(std::move(stopChecker)) {}

//     bool IsValid() const override { return true; }

//     bool Execute() override { return mHandler(mExecTime); }

//     bool IsStop() override { return mStopChecker(); }

// private:
//     std::function<bool(const std::chrono::steady_clock::time_point& execTime)> mHandler;
//     std::function<bool()> mStopChecker;
// };

} // namespace ebpf
} // namespace logtail

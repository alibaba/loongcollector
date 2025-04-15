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

namespace logtail::ebpf {

class ScheduleConfig {
public:
    PluginType mType;
    std::chrono::seconds mInterval;
    ScheduleConfig(PluginType type, const std::chrono::seconds& interval) : mType(type), mInterval(interval) {}
};

class AggregateEvent : public TimerEvent {
public:
    AggregateEvent(const std::chrono::steady_clock::time_point& execTime, const std::shared_ptr<ScheduleConfig>& config)
        : TimerEvent(execTime), mScheduleConfig(config) {}

    [[nodiscard]] bool IsValid() const override;
    bool Execute() override;

private:
    std::shared_ptr<ScheduleConfig> mScheduleConfig;
};

} // namespace logtail::ebpf

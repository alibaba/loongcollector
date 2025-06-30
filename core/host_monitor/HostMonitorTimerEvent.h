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

#include <chrono>
#include <memory>
#include <string>

#include "host_monitor/collector/BaseCollector.h"
#include "timer/TimerEvent.h"

namespace logtail {

class HostMonitorTimerEvent : public TimerEvent {
public:
    HostMonitorTimerEvent(const std::chrono::steady_clock::time_point& execTime,
                          HostMonitorCollectConfig&& collectConfig,
                          std::unique_ptr<BaseCollector>&& collector)
        : TimerEvent(execTime), mCollectConfig(std::move(collectConfig)), mCollector(std::move(collector)) {}

    bool IsValid() const override;
    bool Execute() override;

    HostMonitorCollectConfig mCollectConfig;
    std::unique_ptr<BaseCollector> mCollector;
};

} // namespace logtail

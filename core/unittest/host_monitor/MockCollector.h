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

#include "host_monitor/collector/BaseCollector.h"

namespace logtail {

class MockCollector : public BaseCollector {
public:
    MockCollector() = default;
    ~MockCollector() = default;

    bool Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) override {
        auto event = group->AddLogEvent();
        time_t logtime = time(nullptr);
        event->SetTimestamp(logtime);
        std::string key = "mock_key";
        std::string value = "mock_value";
        event->SetContent(key, value);
        return true;
    }
    static const std::string sName;
    const std::string& Name() const { return sName; }
};

const std::string MockCollector::sName = "mock";

} // namespace logtail

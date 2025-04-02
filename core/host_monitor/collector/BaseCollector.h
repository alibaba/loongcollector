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

#include <string>

#include "host_monitor/HostMonitorTimerEvent.h"
#include "models/PipelineEventGroup.h"

namespace logtail {

class BaseCollector {
public:
    virtual ~BaseCollector() = default;

    virtual bool Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) = 0;
    virtual const std::string& Name() const = 0;

protected:
    bool mValidState = true;
};

} // namespace logtail

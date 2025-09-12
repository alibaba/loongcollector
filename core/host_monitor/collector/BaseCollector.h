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

#include "host_monitor/HostMonitorTypes.h"
#include "models/PipelineEventGroup.h"

namespace logtail {

// Forward declarations
class HostMonitorContext;

class BaseCollector {
public:
    virtual ~BaseCollector() = default;

    virtual bool Init(HostMonitorContext& collectContext);
    virtual bool Collect(HostMonitorContext& collectContext, PipelineEventGroup* group) = 0;
    [[nodiscard]] virtual const std::string& Name() const = 0;
    [[nodiscard]] virtual const std::chrono::seconds GetCollectInterval() const = 0;

protected:
    bool mValidState = true;
};

class CollectorInstance {
public:
    explicit CollectorInstance(std::unique_ptr<BaseCollector>&& collector) : mCollector(std::move(collector)) {}

    bool Init(HostMonitorContext& collectContext);

    bool Collect(HostMonitorContext& collectContext, PipelineEventGroup* group) {
        return mCollector->Collect(collectContext, group);
    }

    std::chrono::seconds GetCollectInterval() const { return mCollector->GetCollectInterval(); }

private:
    std::chrono::steady_clock::time_point mStartTime;
    std::unique_ptr<BaseCollector> mCollector;
};


} // namespace logtail

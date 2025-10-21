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
#include <string>

#include "host_monitor/collector/BaseCollector.h"

namespace logtail {

class SelfCheckCollector : public BaseCollector {
public:
    SelfCheckCollector() = default;
    ~SelfCheckCollector() override = default;

    bool Collect(HostMonitorContext& collectContext, PipelineEventGroup* group) override;
    [[nodiscard]] const std::chrono::seconds GetCollectInterval() const override;

    static const std::string sName;
    const std::string& Name() const override { return sName; }

private:
    // Check if a collector needs to be restarted based on last run time
    bool ShouldRestartCollector(const std::chrono::steady_clock::time_point& lastRunTime,
                                const std::chrono::seconds& interval) const;

    // Restart entire ilogtail process when collector timeout is detected
    void RestartAgent();
};

} // namespace logtail

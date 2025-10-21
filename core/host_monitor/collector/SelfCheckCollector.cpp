/*
 * Copyright 2025 iLogtail Authors
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

#include "host_monitor/collector/SelfCheckCollector.h"

#include <cstdlib>

#include <chrono>
#include <string>

#include "AlarmManager.h"
#include "application/Application.h"
#include "common/Flags.h"
#include "host_monitor/HostMonitorContext.h"
#include "host_monitor/HostMonitorInputRunner.h"
#include "logger/Logger.h"
#include "models/PipelineEventGroup.h"

DEFINE_FLAG_INT32(self_check_collector_interval, "self check collector interval in seconds", 30);

namespace logtail {

const std::string SelfCheckCollector::sName = "self_check";

bool SelfCheckCollector::Collect(HostMonitorContext&, PipelineEventGroup*) {
    auto* runner = HostMonitorInputRunner::GetInstance();
    auto now = std::chrono::steady_clock::now();

    // Get all registered collectors and check their status
    std::vector<std::pair<std::string, std::string>> collectorsToRestart;

    {
        std::shared_lock<std::shared_mutex> lock(runner->mRegisteredCollectorMutex);
        for (const auto& [key, runInfo] : runner->mRegisteredCollector) {
            // Skip self check collector to avoid self-restart
            if (key.collectorName == sName) {
                continue;
            }

            if (ShouldRestartCollector(runInfo.lastRunTime, runInfo.interval)) {
                collectorsToRestart.emplace_back(key.configName, key.collectorName);

                LOG_WARNING(sLogger,
                            ("SelfCheckCollector", "Collector blocked")("config", key.configName)(
                                "collector", key.collectorName)("interval", runInfo.interval.count())(
                                "seconds since last run",
                                std::chrono::duration_cast<std::chrono::seconds>(now - runInfo.lastRunTime).count()));
            }
        }
    }

    // Restart if any collector is blocked
    if (!collectorsToRestart.empty()) {
        std::string reason = "Host Monitor collector is blocked: ";
        for (const auto& [configName, collectorName] : collectorsToRestart) {
            reason += configName;
            reason += ":";
            reason += collectorName;
            reason += ", ";
        }
        LOG_ERROR(sLogger, ("Host Monitor self check", "Critical error detected, restarting agent"));
        AlarmManager::GetInstance()->SendAlarmCritical(AlarmType::HOST_MONITOR_ALARM, reason);
        RestartAgent();
    }
    return true;
}

const std::chrono::seconds SelfCheckCollector::GetCollectInterval() const {
    return std::chrono::seconds(INT32_FLAG(self_check_collector_interval));
}

bool SelfCheckCollector::ShouldRestartCollector(const std::chrono::steady_clock::time_point& lastRunTime,
                                                const std::chrono::seconds& interval) const {
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastRun = std::chrono::duration_cast<std::chrono::seconds>(now - lastRunTime);

    // Restart if more than 5 intervals have passed since last run
    return timeSinceLastRun > (interval * 5);
}

void SelfCheckCollector::RestartAgent() {
    Application::GetInstance()->SetSigTermSignalFlag(true);
    sleep(60);
    _exit(1);
}

} // namespace logtail

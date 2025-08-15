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

#include "host_monitor/HostMonitorTimerEvent.h"

#include "AlarmManager.h"
#include "host_monitor/HostMonitorInputRunner.h"
#include "logger/Logger.h"

namespace logtail {

bool HostMonitorTimerEvent::IsValid() const {
    bool valid = HostMonitorInputRunner::GetInstance()->IsCollectTaskValid(
        mCollectContext.mStartTime, mCollectContext.mConfigName, mCollectContext.mCollectorName);
    return valid;
}

bool HostMonitorTimerEvent::Execute() {
    HostMonitorInputRunner::GetInstance()->ScheduleOnce(mCollectContext);
    return true;
}

void HostMonitorTimerEvent::CollectContext::Reset() {
    auto systemClockNow = std::chrono::system_clock::now();
    auto steadyClockNow = std::chrono::steady_clock::now();
    auto metricTimeNow = std::chrono::system_clock::to_time_t(systemClockNow);

    mCount = 0;
    CalculateFirstCollectTime(metricTimeNow, steadyClockNow);

    // TODO: add more collect type logic when there is more than one collect type
    mCollectType = HostMonitorCollectType::kMultiValue;
}

void HostMonitorTimerEvent::CollectContext::CalculateFirstCollectTime(
    time_t metricTimeNow, std::chrono::steady_clock::time_point steadyClockNow) {
    // In case of multiple collect, one report metrics
    // 1:25 1:26 1:30 1:35 1:40 1:45
    // If start at 1:26, the next metric time should be 1:30. But this data point is belong to the previous interval
    // (1:20, 1:25, 1:30 assume the interval is 5 minutes)
    // so we need to add the collect interval to the next metric time
    auto firstMetricTime
        = ((metricTimeNow / mReportInterval.count()) + 1) * mReportInterval.count() + mCollectInterval.count();
    auto firstScheduleTime = steadyClockNow + std::chrono::seconds(firstMetricTime - metricTimeNow);

    SetTime(firstScheduleTime, firstMetricTime);
}

bool HostMonitorTimerEvent::CollectContext::CheckClockRolling() {
    auto steadyClockNow = std::chrono::steady_clock::now();
    auto systemClockNow = std::chrono::system_clock::now();
    auto systemTimeT = std::chrono::system_clock::to_time_t(systemClockNow);

    if (std::chrono::duration_cast<std::chrono::seconds>(mCollectTime.mScheduleTime - steadyClockNow).count()
        != systemTimeT - mCollectTime.mMetricTime) {
        LOG_ERROR(sLogger,
                  ("host monitor system clock rolling",
                   "will reset collect scheduling")("config", mConfigName)("collector", mCollectorName));
        AlarmManager::GetInstance()->SendAlarmError(
            HOST_MONITOR_ALARM,
            "host monitor system clock rolling, rolling interval: "
                + std::to_string(
                    std::chrono::duration_cast<std::chrono::seconds>(mCollectTime.mScheduleTime - steadyClockNow)
                        .count())
                + ", collector name: " + mCollectorName,
            "",
            "",
            mConfigName);
        return true;
    }
    return false;
}

} // namespace logtail

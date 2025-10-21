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

#include "host_monitor/HostMonitorContext.h"

#include "SelfCheckCollector.h"
#include "host_monitor/collector/BaseCollector.h"
#include "logger/Logger.h"
#include "monitor/AlarmManager.h"


namespace logtail {

void HostMonitorContext::Reset() {
    auto systemClockNow = std::chrono::system_clock::now();
    auto steadyClockNow = std::chrono::steady_clock::now();
    auto metricTimeNow = std::chrono::system_clock::to_time_t(systemClockNow);

    mCount = 0;
    CalculateFirstCollectTime(metricTimeNow, steadyClockNow);
}

void HostMonitorContext::CalculateFirstCollectTime(time_t metricTimeNow,
                                                   std::chrono::steady_clock::time_point steadyClockNow) {
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

// System clock can be rolling, so we need to check if the system clock is rolling
bool HostMonitorContext::CheckClockRolling() {
    auto steadyClockNow = std::chrono::steady_clock::now();
    auto systemClockNow = std::chrono::system_clock::now();
    auto systemTimeT = std::chrono::system_clock::to_time_t(systemClockNow);

    // if the difference between the schedule time and the steady clock is more than 60 seconds, it means the system
    // clock is rolling
    if (std::abs(std::chrono::duration_cast<std::chrono::seconds>(mCollectTime.mScheduleTime - steadyClockNow).count()
                 - std::abs(mCollectTime.mMetricTime - systemTimeT))
        > 60) {
        LOG_ERROR(
            sLogger,
            ("host monitor system clock rolling", "will reset collect scheduling")("config", mConfigName)(
                "collector", mCollectorName)("original metric time", mCollectTime.mMetricTime)(
                "original schedule time", mCollectTime.mScheduleTime.time_since_epoch().count())(
                "current system time", systemTimeT)("current steady time", steadyClockNow.time_since_epoch().count())(
                "expected diff",
                std::chrono::duration_cast<std::chrono::seconds>(mCollectTime.mScheduleTime - steadyClockNow).count())(
                "actual diff", mCollectTime.mMetricTime - systemTimeT));
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

bool HostMonitorContext::ShouldGenerateMetric() {
    if (mCollectorName == SelfCheckCollector::sName) {
        return false;
    }
    if (mCollectType == HostMonitorCollectType::kMultiValue) {
        ++mCount;
        if (mCount < mCountPerReport) {
            return false;
        }
        mCount = 0;
    }
    return true;
}

} // namespace logtail

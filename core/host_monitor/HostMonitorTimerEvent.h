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

#include "collection_pipeline/queue/QueueKey.h"
#include "timer/TimerEvent.h"

namespace logtail {

enum class HostMonitorCollectType {
    kSingleValue,
    kMultiValue,
    kUnknown,
};

class HostMonitorTimerEvent : public TimerEvent {
public:
    struct CollectTime {
        std::chrono::steady_clock::time_point mScheduleTime;
        time_t mMetricTime;

        std::chrono::steady_clock::time_point GetShiftSteadyTime(time_t shiftMetricTime) const {
            return mScheduleTime + std::chrono::seconds(shiftMetricTime - mMetricTime);
        }
    };

    struct CollectContext {
        std::string mConfigName;
        std::string mCollectorName;
        QueueKey mProcessQueueKey;
        size_t mInputIndex;

        std::chrono::seconds mCollectInterval;
        std::chrono::seconds mReportInterval;
        // basic multi-value metrics
        int mCountPerReport = 0;
        int mCount = 0;

        CollectTime mCollectTime;
        std::chrono::steady_clock::time_point mStartTime;
        HostMonitorCollectType mCollectType;

        CollectContext(const std::string& configName,
                       const std::string& collectorName,
                       QueueKey processQueueKey,
                       size_t inputIndex,
                       const std::chrono::seconds& reportInterval)
            : mConfigName(configName),
              mCollectorName(collectorName),
              mProcessQueueKey(processQueueKey),
              mInputIndex(inputIndex),
              mReportInterval(reportInterval),
              mStartTime(std::chrono::steady_clock::now()) {}

        void SetTime(const std::chrono::steady_clock::time_point& scheduleTime, time_t metricTime) {
            mCollectTime.mScheduleTime = scheduleTime;
            mCollectTime.mMetricTime = metricTime;
        }

        std::chrono::steady_clock::time_point GetScheduleTime() const { return mCollectTime.mScheduleTime; }
        time_t GetMetricTime() const { return mCollectTime.mMetricTime; }

        void Reset();
        void CalculateFirstCollectTime(time_t metricTimeNow, std::chrono::steady_clock::time_point steadyClockNow);
        bool CheckClockRolling();
    };

    HostMonitorTimerEvent(const CollectContext& collectContext)
        : TimerEvent(collectContext.GetScheduleTime()), mCollectContext(collectContext) {}

    bool IsValid() const override;
    bool Execute() override;

private:
    CollectContext mCollectContext;
};

} // namespace logtail

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

class HostMonitorTimerEvent : public TimerEvent {
public:
    struct CollectConfig {
        std::string mCollectorName;
        QueueKey mProcessQueueKey;
        size_t mInputIndex;
        std::chrono::seconds mInterval;

        CollectConfig(const std::string& collectorName,
                      QueueKey processQueueKey,
                      size_t inputIndex,
                      const std::chrono::seconds& interval)
            : mCollectorName(collectorName),
              mProcessQueueKey(processQueueKey),
              mInputIndex(inputIndex),
              mInterval(interval) {}
    };

    HostMonitorTimerEvent(const std::chrono::steady_clock::time_point& execTime, const CollectConfig& collectConfig)
        : TimerEvent(execTime), mCollectConfig(collectConfig) {}

    bool IsValid() const override;
    bool Execute() override;

private:
    CollectConfig mCollectConfig;
};

} // namespace logtail

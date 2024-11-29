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
#include <utility>

#include "QueueKey.h"
#include "timer/TimerEvent.h"

namespace logtail {


class HostMonitorTimerEvent : public TimerEvent {
public:
    struct CollectConfig {
        CollectConfig(std::string configName,
                      std::string collectorName,
                      QueueKey processQueueKey,
                      int inputIndex,
                      std::chrono::seconds interval)
            : mConfigName(configName),
              mCollectorName(collectorName),
              mProcessQueueKey(processQueueKey),
              mInputIndex(inputIndex),
              mInterval(interval) {}

        std::string mConfigName;
        std::string mCollectorName;
        QueueKey mProcessQueueKey;
        size_t mInputIndex;
        std::chrono::seconds mInterval;
    };

    HostMonitorTimerEvent(std::chrono::steady_clock::time_point execTime, std::unique_ptr<CollectConfig> collectConfig)
        : TimerEvent(execTime), mCollectConfig(std::move(collectConfig)) {}

    bool IsValid() const override;
    bool Execute() override;
    void ResetForNextExec() { SetExecTime(GetExecTime() + mCollectConfig->mInterval); }

private:
    std::unique_ptr<CollectConfig> mCollectConfig;
};

} // namespace logtail

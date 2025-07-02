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

#include "models/PipelineEventGroup.h"

namespace logtail {

struct HostMonitorCollectConfig {
    std::string mConfigName;
    std::string mCollectorName;
    QueueKey mProcessQueueKey;
    size_t mInputIndex;
    std::chrono::seconds mFlushInterval;

    HostMonitorCollectConfig(const std::string& configName,
                             const std::string& collectorName,
                             QueueKey processQueueKey,
                             size_t inputIndex,
                             const std::chrono::seconds& flushInterval)
        : mConfigName(configName),
          mCollectorName(collectorName),
          mProcessQueueKey(processQueueKey),
          mInputIndex(inputIndex),
          mFlushInterval(flushInterval) {}
};

class BaseCollector {
public:
    virtual ~BaseCollector() = default;
    void Init(const HostMonitorCollectConfig& collectConfig) {
        mTotalCount = collectConfig.mFlushInterval.count() / GetCollectInterval();
        mCount = 0;
    }
    virtual bool Collect(PipelineEventGroup* group) = 0;
    [[nodiscard]] virtual const std::string& Name() const = 0;
    [[nodiscard]] virtual int GetCollectInterval() const = 0;

protected:
    bool mValidState = true;
    int mTotalCount = 0;
    int mCount = 0;
};

} // namespace logtail

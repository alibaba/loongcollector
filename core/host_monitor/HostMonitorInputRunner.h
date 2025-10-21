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

#include <cstdint>

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "collection_pipeline/queue/QueueKey.h"
#include "common/ThreadPool.h"
#include "host_monitor/HostMonitorContext.h"
#include "host_monitor/collector/BaseCollector.h"
#include "host_monitor/collector/CollectorMetrics.h"
#include "monitor/metric_models/MetricRecord.h"
#include "runner/InputRunner.h"

namespace logtail {

struct CollectorInfo {
    std::string name;
    uint32_t interval;
    HostMonitorCollectType type;
};

struct CollectorRunInfo {
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastRunTime;
    std::chrono::seconds interval;
};

class HostMonitorInputRunner : public InputRunner {
public:
    struct CollectorKey {
        std::string configName;
        std::string collectorName;

        bool operator<(const CollectorKey& other) const {
            if (configName != other.configName) {
                return configName < other.configName;
            }
            return collectorName < other.collectorName;
        }

        bool operator==(const CollectorKey& other) const {
            return configName == other.configName && collectorName == other.collectorName;
        }
    };

    HostMonitorInputRunner(const HostMonitorInputRunner&) = delete;
    HostMonitorInputRunner(HostMonitorInputRunner&&) = delete;
    HostMonitorInputRunner& operator=(const HostMonitorInputRunner&) = delete;
    HostMonitorInputRunner& operator=(HostMonitorInputRunner&&) = delete;
    static HostMonitorInputRunner* GetInstance() {
        static HostMonitorInputRunner sInstance;
        return &sInstance;
    }

    void UpdateCollector(const std::string& configName,
                         const std::vector<CollectorInfo>& newCollectorInfos,
                         QueueKey processQueueKey,
                         size_t inputIndex);
    void RemoveCollector(const std::string& configName);
    void RemoveAllCollector();

    void Init() override;
    void Stop() override;
    bool HasRegisteredPlugins() const override;

    bool IsCollectTaskValid(const std::chrono::steady_clock::time_point& startTime,
                            const std::string& configName,
                            const std::string& collectorName);
    void ScheduleOnce(CollectContextPtr collectContext);
    void InitMetrics();
    void UpdateMetrics(uint64_t outItemsSize, uint64_t dropItems, uint64_t latencyMs);

private:
    HostMonitorInputRunner();
    ~HostMonitorInputRunner() override = default;

    template <typename T>
    void RegisterCollector() {
        mCollectorCreatorMap.emplace(T::sName,
                                     []() -> CollectorInstance { return CollectorInstance(std::make_unique<T>()); });
    }

    void PushQueue(CollectContextPtr context, PipelineEventGroup&& group);
    void PushNextTimerEvent(CollectContextPtr config);
    void AddHostLabels(PipelineEventGroup& group);

    std::atomic_bool mIsStarted = false;
    std::unique_ptr<ThreadPool> mThreadPool;
    std::atomic_uint64_t mRunningPipelineCount = 0;

    mutable std::shared_mutex mRegisteredCollectorMutex;
    std::map<CollectorKey, CollectorRunInfo> mRegisteredCollector;

    std::unordered_map<std::string, std::function<CollectorInstance()>> mCollectorCreatorMap;

    // Metrics
    MetricsRecordRef mMetricsRecordRef;
    CounterPtr mOutItemsTotal;
    CounterPtr mOutItemsSize;
    CounterPtr mDropItemsTotal;
    TimeCounterPtr mLatencyTimeMs;
    IntGaugePtr mLastRunTime;

    friend class SelfCheckCollector;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class HostMonitorInputRunnerUnittest;
#endif
};

} // namespace logtail

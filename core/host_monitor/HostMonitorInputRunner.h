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

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "collection_pipeline/queue/QueueKey.h"
#include "common/ThreadPool.h"
#include "host_monitor/HostMonitorTimerEvent.h"
#include "host_monitor/collector/BaseCollector.h"
#include "runner/InputRunner.h"

namespace logtail {

class CollectorInstance {
public:
    CollectorInstance(std::unique_ptr<BaseCollector>&& collector) : mCollector(std::move(collector)) {}

    bool IsEnabled() const { return mIsEnabled; }
    void Enable() {
        mIsEnabled = true;
        mLastEnableTime = std::chrono::steady_clock::now();
    }
    void Disable() { mIsEnabled = false; }
    bool IsValidEvent(const std::chrono::steady_clock::time_point& execTime) const {
        return mIsEnabled && execTime >= mLastEnableTime;
    }
    bool Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) {
        return mCollector->Collect(collectConfig, group);
    }

private:
    bool mIsEnabled = false;
    std::chrono::steady_clock::time_point mLastEnableTime;
    std::unique_ptr<BaseCollector> mCollector;
};

class HostMonitorInputRunner : public InputRunner {
public:
    HostMonitorInputRunner(const HostMonitorInputRunner&) = delete;
    HostMonitorInputRunner(HostMonitorInputRunner&&) = delete;
    HostMonitorInputRunner& operator=(const HostMonitorInputRunner&) = delete;
    HostMonitorInputRunner& operator=(HostMonitorInputRunner&&) = delete;
    static HostMonitorInputRunner* GetInstance() {
        static HostMonitorInputRunner sInstance;
        return &sInstance;
    }

    // Only support singleton mode
    void UpdateCollector(const std::vector<std::string>& newCollectorNames,
                         const std::vector<uint32_t>& newCollectorIntervals,
                         QueueKey processQueueKey,
                         size_t inputIndex);
    void RemoveCollector();

    void Init() override;
    void Stop() override;
    bool HasRegisteredPlugins() const override;

    bool IsCollectTaskValid(const std::chrono::steady_clock::time_point& execTime, const std::string& collectorName);
    void ScheduleOnce(const std::chrono::steady_clock::time_point& execTime,
                      HostMonitorTimerEvent::CollectConfig& collectConfig);

private:
    HostMonitorInputRunner();
    ~HostMonitorInputRunner() override = default;

    template <typename T>
    void RegisterCollector() {
        auto collector = std::make_unique<T>();
        mRegisteredCollectorMap.emplace(T::sName, CollectorInstance(std::move(collector)));
    }

    void PushNextTimerEvent(const std::chrono::steady_clock::time_point& execTime,
                            const HostMonitorTimerEvent::CollectConfig& config);

    std::atomic_bool mIsStarted = false;
    std::unique_ptr<ThreadPool> mThreadPool;

    mutable std::shared_mutex mRegisteredCollectorMapMutex;
    std::unordered_map<std::string, CollectorInstance> mRegisteredCollectorMap;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class HostMonitorInputRunnerUnittest;
#endif
};

} // namespace logtail

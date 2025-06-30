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

#include <atomic>
#include <chrono>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "collection_pipeline/queue/QueueKey.h"
#include "common/ThreadPool.h"
#include "host_monitor/HostMonitorTimerEvent.h"
#include "host_monitor/collector/BaseCollector.h"
#include "runner/InputRunner.h"

namespace logtail {

template <typename T>
class CollectorFactory {
public:
    static std::unique_ptr<BaseCollector> Create(const HostMonitorCollectConfig& collectConfig) {
        auto collector = std::make_unique<T>();
        collector->Init(collectConfig);
        return collector;
    }
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
    void UpdateCollector(const std::string& configName,
                         const std::vector<std::string>& newCollectorNames,
                         const std::vector<uint32_t>& newCollectorIntervals,
                         QueueKey processQueueKey,
                         size_t inputIndex);
    void RemoveCollector(const std::string& configName);
    void RemoveAllCollector();

    void Init() override;
    void Stop() override;
    bool HasRegisteredPlugins() const override;

    bool IsCollectTaskValid(const std::string& configName, const std::string& collectorName);
    void ScheduleOnce(HostMonitorTimerEvent& event, const std::chrono::steady_clock::time_point& execTime);

private:
    HostMonitorInputRunner();
    ~HostMonitorInputRunner() override = default;

    template <typename T>
    void RegisterCollector() {
        mRegisteredCollectorMap.emplace(T::sName, CollectorFactory<T>::Create);
    }

    void PushNextTimerEvent(const std::chrono::steady_clock::time_point& execTime, HostMonitorTimerEvent& event);
    void AddHostLabels(PipelineEventGroup& group);

    std::atomic_bool mIsStarted = false;
    std::unique_ptr<ThreadPool> mThreadPool;

    std::unordered_map<std::string, std::function<std::unique_ptr<BaseCollector>(const HostMonitorCollectConfig&)>>
        mRegisteredCollectorMap;
    mutable std::shared_mutex mRunningConfigMapMutex;
    std::unordered_map<std::string, std::unordered_set<std::string>> mRunningConfigMap;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class HostMonitorInputRunnerUnittest;
#endif
};

} // namespace logtail

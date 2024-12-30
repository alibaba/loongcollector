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
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common/ThreadPool.h"
#include "host_monitor/HostMonitorTimerEvent.h"
#include "host_monitor/collector/BaseCollector.h"
#include "host_monitor/collector/ProcessEntityCollector.h"
#include "pipeline/queue/QueueKey.h"
#include "runner/InputRunner.h"

namespace logtail {

const int DEFAULT_SCHEDULE_INTERVAL = 10;

class HostMonitorInputRunner : public InputRunner {
public:
    HostMonitorInputRunner(const HostMonitorInputRunner&) = delete;
    HostMonitorInputRunner(HostMonitorInputRunner&&) = delete;
    HostMonitorInputRunner& operator=(const HostMonitorInputRunner&) = delete;
    HostMonitorInputRunner& operator=(HostMonitorInputRunner&&) = delete;
    ~HostMonitorInputRunner() override = default;
    static HostMonitorInputRunner* GetInstance() {
        static HostMonitorInputRunner sInstance;
        return &sInstance;
    }

    // Only support singleton mode
    void UpdateCollector(const std::vector<std::string>& collectorNames, QueueKey processQueueKey, int inputIndex);
    void RemoveCollector();

    void Init() override;
    void Stop() override;
    bool HasRegisteredPlugins() const override;

    bool IsCollectTaskValid(const std::string& configName, const std::string& collectorName);
    void ScheduleOnce(HostMonitorTimerEvent::CollectConfig& collectConfig);

private:
    HostMonitorInputRunner();

    template <typename T>
    void RegisterCollector();
    std::shared_ptr<BaseCollector> GetCollector(const std::string& collectorName);

    std::atomic_bool mIsStarted = false;

    ThreadPool mThreadPool;

    mutable std::shared_mutex mRegisteredCollectorMapMutex;
    std::unordered_map<std::string, bool> mRegisteredCollectorMap;
    std::unordered_map<std::string, std::shared_ptr<BaseCollector>> mCollectorInstanceMap;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class HostMonitorInputRunnerUnittest;
#endif
};

} // namespace logtail

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
#include <vector>

#include "BaseCollector.h"
#include "InputRunner.h"
#include "Lock.h"
#include "QueueKey.h"
#include "ThreadPool.h"
#include "host_monitor/HostMonitorTimerEvent.h"

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

    void UpdateCollector(const std::string& configName,
                         const std::vector<std::string>& collectorNames,
                         QueueKey processQueueKey,
                         int inputIndex);
    void RemoveCollector(const std::string& configName);

    void Init() override;
    void Stop() override;
    bool HasRegisteredPlugins() const override;

    bool IsCollectTaskValid(const std::string& configName, const std::string& collectorName) const;
    void ScheduleOnce(std::unique_ptr<HostMonitorTimerEvent::CollectConfig> collectConfig);

private:
    HostMonitorInputRunner();
    std::unique_ptr<HostMonitorTimerEvent>
    BuildTimerEvent(std::unique_ptr<HostMonitorTimerEvent::CollectConfig> collectConfig);

    template <typename T>
    void RegisterCollector();
    std::shared_ptr<BaseCollector> GetCollector(const std::string& collectorName);

    std::atomic_bool mIsStarted = false;

    ThreadPool mThreadPool;

    mutable std::shared_mutex mCollectorRegisterMapMutex;
    std::unordered_map<std::string, std::vector<std::string>> mCollectorRegisterMap;
    std::unordered_map<std::string, std::shared_ptr<BaseCollector>> mCollectorInstanceMap;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class HostMonitorInputRunnerUnittest;
#endif
};

} // namespace logtail

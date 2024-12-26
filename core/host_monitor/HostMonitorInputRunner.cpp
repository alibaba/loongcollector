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

#include "HostMonitorInputRunner.h"

#include <future>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

#include "HostMonitorTimerEvent.h"
#include "ProcessEntityCollector.h"
#include "common/Lock.h"
#include "common/timer/Timer.h"
#include "host_monitor/collector/ProcessEntityCollector.h"
#include "logger/Logger.h"
#include "runner/ProcessorRunner.h"


namespace logtail {

HostMonitorInputRunner::HostMonitorInputRunner() : mThreadPool(ThreadPool(3)) {
    RegisterCollector<ProcessEntityCollector>();
}

void HostMonitorInputRunner::UpdateCollector(const std::string& configName,
                                             const std::vector<std::string>& newCollectors,
                                             QueueKey processQueueKey,
                                             int inputIndex) {
    std::vector<std::string> oldCollectors;
    {
        std::unique_lock lock(mCollectorRegisterMapMutex);
        auto it = mCollectorRegisterMap.find(configName);
        if (it != mCollectorRegisterMap.end()) {
            oldCollectors = it->second;
        }
        mCollectorRegisterMap[configName] = newCollectors;
    }
    for (const auto& collectorName : newCollectors) {
        LOG_INFO(sLogger, ("add new host monitor collector", configName)("collector", collectorName));
        HostMonitorTimerEvent::CollectConfig collectConfig(
            configName, collectorName, processQueueKey, inputIndex, std::chrono::seconds(DEFAULT_SCHEDULE_INTERVAL));
        // only push event when the collector is new added
        if (std::find(oldCollectors.begin(), oldCollectors.end(), collectorName) == oldCollectors.end()) {
            Timer::GetInstance()->PushEvent(BuildTimerEvent(collectConfig));
        }
    }
}

void HostMonitorInputRunner::RemoveCollector(const std::string& configName) {
    std::unique_lock lock(mCollectorRegisterMapMutex);
    mCollectorRegisterMap.erase(configName);
}

void HostMonitorInputRunner::Init() {
    if (mIsStarted.exchange(true)) {
        return;
    }
    LOG_INFO(sLogger, ("HostMonitorInputRunner", "Start"));
#ifndef APSARA_UNIT_TEST_MAIN
    mThreadPool.Start();
#endif
}

void HostMonitorInputRunner::Stop() {
    if (!mIsStarted.exchange(false)) {
        return;
    }
#ifndef APSARA_UNIT_TEST_MAIN
    std::future<void> result = std::async(std::launch::async, [this]() { mThreadPool.Stop(); });
    if (result.wait_for(std::chrono::seconds(3)) == std::future_status::timeout) {
        LOG_ERROR(sLogger, ("HostMonitorInputRunner stop timeout 3 seconds", "may cause thread leak"));
    }
#endif
    LOG_INFO(sLogger, ("HostMonitorInputRunner", "Stop"));
}

bool HostMonitorInputRunner::HasRegisteredPlugins() const {
    std::shared_lock lock(mCollectorRegisterMapMutex);
    return !mCollectorRegisterMap.empty();
}

bool HostMonitorInputRunner::IsCollectTaskValid(const std::string& configName, const std::string& collectorName) const {
    std::shared_lock lock(mCollectorRegisterMapMutex);
    auto collectors = mCollectorRegisterMap.find(configName);
    if (collectors == mCollectorRegisterMap.end()) {
        return false;
    }
    for (const auto& collectorName : collectors->second) {
        if (collectorName == collectorName) {
            return true;
        }
    }
    return false;
}

void HostMonitorInputRunner::ScheduleOnce(HostMonitorTimerEvent::CollectConfig& config) {
    auto collectFn = [this, config]() mutable {
        PipelineEventGroup group(std::make_shared<SourceBuffer>());
        auto collector = GetCollector(config.mCollectorName);
        if (!collector) {
            collector->Collect(group);
        } else {
            LOG_ERROR(sLogger,
                      ("collector not found, will not collect",
                       "discard data")("config", config.mConfigName)("collector", config.mCollectorName));
            return;
        }

        bool result = ProcessorRunner::GetInstance()->PushQueue(
            config.mProcessQueueKey, config.mInputIndex, std::move(group), 3);
        if (!result) {
            LOG_WARNING(sLogger,
                        ("push queue failed", "discard data")("config", config.mConfigName)("collector",
                                                                                            config.mCollectorName));
        }
        LOG_DEBUG(sLogger,
                  ("schedule host monitor collector again", config.mConfigName)("collector", config.mCollectorName));
        auto event = BuildTimerEvent(config);
        event->ResetForNextExec();
        Timer::GetInstance()->PushEvent(std::move(event));
    };
    mThreadPool.Add(collectFn);
}

std::unique_ptr<HostMonitorTimerEvent>
HostMonitorInputRunner::BuildTimerEvent(HostMonitorTimerEvent::CollectConfig& collectConfig) {
    auto now = std::chrono::steady_clock::now();
    auto event = std::make_unique<HostMonitorTimerEvent>(now, collectConfig);
    return event;
}

std::shared_ptr<BaseCollector> HostMonitorInputRunner::GetCollector(const std::string& collectorName) {
    auto it = mCollectorInstanceMap.find(collectorName);
    if (it == mCollectorInstanceMap.end()) {
        return nullptr;
    }
    return it->second;
}

template <typename T>
void HostMonitorInputRunner::RegisterCollector() {
    auto collector = std::make_shared<T>();
    mCollectorInstanceMap[collector->Name()] = collector;
}

} // namespace logtail

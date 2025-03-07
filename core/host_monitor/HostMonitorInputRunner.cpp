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

#include "host_monitor/HostMonitorInputRunner.h"

#include <cstdint>

#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/Flags.h"
#include "common/timer/Timer.h"
#include "host_monitor/HostMonitorTimerEvent.h"
#include "host_monitor/collector/ProcessEntityCollector.h"
#include "logger/Logger.h"
#include "runner/ProcessorRunner.h"

DEFINE_FLAG_INT32(host_monitor_thread_pool_size, "host monitor thread pool size", 3);

namespace logtail {

HostMonitorInputRunner::HostMonitorInputRunner() {
    RegisterCollector<ProcessEntityCollector>();
    size_t threadPoolSize = 1;
    // threadPoolSize should be greater than 0
    if (INT32_FLAG(host_monitor_thread_pool_size) > 0) {
        threadPoolSize = INT32_FLAG(host_monitor_thread_pool_size);
    }
    // threadPoolSize should be less than or equal to the number of registered collectors
    mThreadPool = std::make_unique<ThreadPool>(std::min(threadPoolSize, mRegisteredCollectorMap.size()));
}

void HostMonitorInputRunner::UpdateCollector(const std::vector<std::string>& newCollectorNames,
                                             const std::vector<uint32_t>& newCollectorIntervals,
                                             QueueKey processQueueKey,
                                             size_t inputIndex) {
    std::unique_lock<std::shared_mutex> lock(mRegisteredCollectorMapMutex);
    for (size_t i = 0; i < newCollectorNames.size(); ++i) {
        const auto& collectorName = newCollectorNames[i];
        auto iter = mRegisteredCollectorMap.find(collectorName);
        if (iter == mRegisteredCollectorMap.end()) {
            LOG_ERROR(sLogger, ("host monitor", "collector not support")("collector", collectorName));
            continue;
        }
        // register new collector
        iter->second.Enable();
        // add timer event
        HostMonitorTimerEvent::CollectConfig collectConfig(
            collectorName, processQueueKey, inputIndex, std::chrono::seconds(newCollectorIntervals[i]));
        auto now = std::chrono::steady_clock::now();
        auto event = std::make_unique<HostMonitorTimerEvent>(now, collectConfig);
        Timer::GetInstance()->PushEvent(std::move(event));
        LOG_INFO(sLogger, ("host monitor", "add new collector")("collector", collectorName));
    }
}

void HostMonitorInputRunner::RemoveCollector() {
    std::unique_lock<std::shared_mutex> lock(mRegisteredCollectorMapMutex);
    for (auto& collector : mRegisteredCollectorMap) {
        collector.second.Disable();
    }
}

void HostMonitorInputRunner::Init() {
    if (mIsStarted.exchange(true)) {
        return;
    }
    LOG_INFO(sLogger, ("HostMonitorInputRunner", "Start"));
#ifndef APSARA_UNIT_TEST_MAIN
    mThreadPool->Start();
    Timer::GetInstance()->Init();
#endif
}

void HostMonitorInputRunner::Stop() {
    if (!mIsStarted.exchange(false)) {
        return;
    }
    RemoveCollector();
#ifndef APSARA_UNIT_TEST_MAIN
    std::future<void> result = std::async(std::launch::async, [this]() { mThreadPool->Stop(); });
    if (result.wait_for(std::chrono::seconds(3)) == std::future_status::timeout) {
        LOG_ERROR(sLogger, ("host monitor runner stop timeout 3 seconds", "forced to stopped, may cause thread leak"));
    } else {
        LOG_INFO(sLogger, ("host monitor runner", "stop successfully"));
    }
#endif
}

bool HostMonitorInputRunner::HasRegisteredPlugins() const {
    std::shared_lock<std::shared_mutex> lock(mRegisteredCollectorMapMutex);
    for (auto& collector : mRegisteredCollectorMap) {
        if (collector.second.IsEnabled()) {
            return true;
        }
    }
    return false;
}

bool HostMonitorInputRunner::IsCollectTaskValid(const std::chrono::steady_clock::time_point& execTime,
                                                const std::string& collectorName) {
    std::shared_lock<std::shared_mutex> lock(mRegisteredCollectorMapMutex);
    auto it = mRegisteredCollectorMap.find(collectorName);
    if (it == mRegisteredCollectorMap.end()) {
        return false;
    }
    return it->second.IsValidEvent(execTime);
}

void HostMonitorInputRunner::ScheduleOnce(const std::chrono::steady_clock::time_point& execTime,
                                          HostMonitorTimerEvent::CollectConfig& config) {
    if (!ProcessQueueManager::GetInstance()->IsValidToPush(config.mProcessQueueKey)) {
        LOG_WARNING(sLogger,
                    ("host monitor push process queue failed", "discard data")("collector", config.mCollectorName));
        PushNextTimerEvent(execTime, config);
        return;
    }

    auto collectFn = [this, config, execTime]() mutable {
        PipelineEventGroup group(std::make_shared<SourceBuffer>());
        std::unique_lock<std::shared_mutex> lock(mRegisteredCollectorMapMutex);
        auto collector = mRegisteredCollectorMap.find(config.mCollectorName);
        if (collector == mRegisteredCollectorMap.end()) {
            LOG_ERROR(
                sLogger,
                ("collector not found, will not collect again", "discard data")("collector", config.mCollectorName));
            return;
        }
        if (!collector->second.IsEnabled()) {
            LOG_DEBUG(sLogger,
                      ("collector not enabled, may be caused by config update", "discard data")("collector",
                                                                                                config.mCollectorName));
            return;
        }
        if (collector->second.Collect(config, &group)) {
            LOG_DEBUG(
                sLogger,
                ("host monitor", "collect data")("collector", config.mCollectorName)("size", group.GetEvents().size()));
            if (group.GetEvents().size() > 0) {
                bool result = ProcessorRunner::GetInstance()->PushQueue(
                    config.mProcessQueueKey, config.mInputIndex, std::move(group));
                if (!result) {
                    LOG_ERROR(
                        sLogger,
                        ("host monitor push process queue failed", "discard data")("collector", config.mCollectorName));
                }
            }
        } else {
            LOG_ERROR(sLogger,
                      ("host monitor collect data failed", "collect error")("collector", config.mCollectorName));
        }
        PushNextTimerEvent(execTime, config);
    };
    mThreadPool->Add(collectFn);
}

void HostMonitorInputRunner::PushNextTimerEvent(const std::chrono::steady_clock::time_point& execTime,
                                                const HostMonitorTimerEvent::CollectConfig& config) {
    std::chrono::steady_clock::time_point nextExecTime = execTime + config.mInterval;
    while (nextExecTime < std::chrono::steady_clock::now()) {
        nextExecTime += config.mInterval;
    }
    auto event = std::make_unique<HostMonitorTimerEvent>(nextExecTime, config);
    Timer::GetInstance()->PushEvent(std::move(event));
}

} // namespace logtail

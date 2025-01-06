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

#include <chrono>

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

#include "HostMonitorTimerEvent.h"
#include "ProcessEntityCollector.h"
#include "common/timer/Timer.h"
#include "host_monitor/collector/ProcessEntityCollector.h"
#include "logger/Logger.h"
#include "runner/ProcessorRunner.h"


namespace logtail {

HostMonitorInputRunner::HostMonitorInputRunner() : mThreadPool(ThreadPool(3)) {
    RegisterCollector<ProcessEntityCollector>();
}

void HostMonitorInputRunner::UpdateCollector(const std::vector<std::string>& newCollectors,
                                             QueueKey processQueueKey,
                                             int inputIndex) {
    std::unique_lock lock(mRegisteredCollectorMapMutex);
    for (auto& collector : newCollectors) {
        auto iter = mRegisteredCollectorMap.find(collector);
        if (iter == mRegisteredCollectorMap.end()) {
            LOG_ERROR(sLogger, ("host monitor", "collector not support")("collector", collector));
            continue;
        }
        // register new collector
        iter->second.Enable();
        // add timer event
        HostMonitorTimerEvent::CollectConfig collectConfig(
            collector, processQueueKey, inputIndex, std::chrono::seconds(DEFAULT_SCHEDULE_INTERVAL));
        auto now = std::chrono::steady_clock::now();
        auto event = std::make_unique<HostMonitorTimerEvent>(now, collectConfig);
        Timer::GetInstance()->PushEvent(std::move(event));
        LOG_INFO(sLogger, ("host monitor", "add new collector")("collector", collector));
    }
}

void HostMonitorInputRunner::RemoveCollector() {
    std::unique_lock lock(mRegisteredCollectorMapMutex);
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
    mThreadPool.Start();
#endif
}

void HostMonitorInputRunner::Stop() {
    if (!mIsStarted.exchange(false)) {
        return;
    }
    RemoveCollector();
#ifndef APSARA_UNIT_TEST_MAIN
    std::future<void> result = std::async(std::launch::async, [this]() { mThreadPool.Stop(); });
    if (result.wait_for(std::chrono::seconds(3)) == std::future_status::timeout) {
        LOG_ERROR(sLogger, ("HostMonitorInputRunner stop timeout 3 seconds", "may cause thread leak"));
    }
#endif
    LOG_INFO(sLogger, ("HostMonitorInputRunner", "Stop"));
}

bool HostMonitorInputRunner::HasRegisteredPlugins() const {
    std::shared_lock lock(mRegisteredCollectorMapMutex);
    for (auto& collector : mRegisteredCollectorMap) {
        if (collector.second.IsEnabled()) {
            return true;
        }
    }
    return false;
}

bool HostMonitorInputRunner::IsCollectTaskValid(std::chrono::steady_clock::time_point execTime,
                                                const std::string& collectorName) {
    std::shared_lock lock(mRegisteredCollectorMapMutex);
    auto it = mRegisteredCollectorMap.find(collectorName);
    if (it == mRegisteredCollectorMap.end()) {
        return false;
    }
    return it->second.IsEnabled() && (it->second.mLastEnableTime <= execTime);
}

void HostMonitorInputRunner::ScheduleOnce(std::chrono::steady_clock::time_point execTime,
                                          HostMonitorTimerEvent::CollectConfig& config) {
    auto collectFn = [this, config, execTime]() mutable {
        PipelineEventGroup group(std::make_shared<SourceBuffer>());
        group.SetMetadata(EventGroupMetaKey::HOST_MONITOR_COLLECT_INTERVAL, std::to_string(config.mInterval.count()));

        std::unique_lock lock(mRegisteredCollectorMapMutex);
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
        collector->second.GetCollector()->Collect(group);
        LOG_DEBUG(
            sLogger,
            ("host monitor", "collect data")("collector", config.mCollectorName)("size", group.GetEvents().size()));
        if (group.GetEvents().size() > 0) {
            LOG_DEBUG(sLogger, ("host monitor", "push process queue")("collector", config.mProcessQueueKey));
            bool result = ProcessorRunner::GetInstance()->PushQueue(
                config.mProcessQueueKey, config.mInputIndex, std::move(group));
            if (!result) {
                // there is no process for host monitor, so should not block in process queue
                LOG_ERROR(
                    sLogger,
                    ("host monitor push process queue failed", "discard data")("collector", config.mCollectorName));
            }
        }

        // add next collect event
        std::chrono::steady_clock::time_point nextExecTime = execTime + config.mInterval;
        while (nextExecTime < std::chrono::steady_clock::now()) {
            nextExecTime += config.mInterval;
        }
        auto event = std::make_unique<HostMonitorTimerEvent>(nextExecTime, config);
        Timer::GetInstance()->PushEvent(std::move(event));
    };
    mThreadPool.Add(collectFn);
}

template <typename T>
void HostMonitorInputRunner::RegisterCollector() {
    auto collector = std::make_unique<T>();
    mRegisteredCollectorMap.emplace(T::sName, CollectorInstance(std::move(collector)));
}

} // namespace logtail

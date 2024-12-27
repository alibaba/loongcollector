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

#include <algorithm>
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
        auto oldCollector = mRegisteredCollectorMap.find(collector);
        if (oldCollector == mRegisteredCollectorMap.end()) {
            if (mCollectorInstanceMap.find(collector) == mCollectorInstanceMap.end()) {
                LOG_ERROR(sLogger, ("host monitor", "collector not found")("collector", collector));
                continue;
            }
            mRegisteredCollectorMap[collector] = true;
            HostMonitorTimerEvent::CollectConfig collectConfig(
                collector, processQueueKey, inputIndex, std::chrono::seconds(DEFAULT_SCHEDULE_INTERVAL));
            auto now = std::chrono::steady_clock::now();
            auto event = std::make_unique<HostMonitorTimerEvent>(now + collectConfig.mInterval, collectConfig);
            Timer::GetInstance()->PushEvent(std::move(event));
            LOG_INFO(sLogger, ("host monitor", "add new collector")("collector", collector));
        } else {
            // config removed and added again, timer event is still in the queue
            if (!oldCollector->second) {
                oldCollector->second = true;
            }
        }
    }
}

void HostMonitorInputRunner::RemoveCollector() {
    std::unique_lock lock(mRegisteredCollectorMapMutex);
    for (auto& collector : mRegisteredCollectorMap) {
        collector.second = false;
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
        if (collector.second) {
            return true;
        }
    }
    return false;
}

bool HostMonitorInputRunner::IsCollectTaskValid(const std::string& configName, const std::string& collectorName) {
    std::unique_lock lock(mRegisteredCollectorMapMutex);
    auto it = mRegisteredCollectorMap.find(collectorName);
    if (it != mRegisteredCollectorMap.end() && it->second) {
        return true;
    }
    return false;
}

void HostMonitorInputRunner::ScheduleOnce(HostMonitorTimerEvent::CollectConfig& config) {
    auto collectFn = [this, config]() mutable {
        PipelineEventGroup group(std::make_shared<SourceBuffer>());
        auto collector = GetCollector(config.mCollectorName);
        if (collector) {
            collector->Collect(group);
        } else {
            LOG_ERROR(sLogger,
                      ("collector not found, will not collect again",
                       "discard data")("config", config.mConfigName)("collector", config.mCollectorName));
            return;
        }

        LOG_DEBUG(sLogger, ("host monitor collect", "collector")(config.mCollectorName, group.GetEvents().size()));
        if (group.GetEvents().size() > 0) {
            bool result = ProcessorRunner::GetInstance()->PushQueue(
                config.mProcessQueueKey, config.mInputIndex, std::move(group));
            if (!result) {
                // there is no process for host monitor, so should not block in process queue
                LOG_ERROR(sLogger,
                          ("host monitor push process queue failed",
                           "discard data")("config", config.mConfigName)("collector", config.mCollectorName));
            }
        }
    };
    mThreadPool.Add(collectFn);
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

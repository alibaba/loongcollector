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
#include <unordered_map>
#include <utility>
#include <vector>

#include "SystemInterface.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/Flags.h"
#include "common/MachineInfoUtil.h"
#include "common/StringView.h"
#include "common/timer/Timer.h"
#include "host_monitor/Constants.h"
#include "host_monitor/HostMonitorTimerEvent.h"
#include "host_monitor/collector/CPUCollector.h"
#include "host_monitor/collector/DiskCollector.h"
#include "host_monitor/collector/MemCollector.h"
#include "host_monitor/collector/NetCollector.h"
#include "host_monitor/collector/ProcessCollector.h"
#include "host_monitor/collector/ProcessEntityCollector.h"
#include "host_monitor/collector/SystemCollector.h"
#include "logger/Logger.h"
#include "models/MetricEvent.h"
#include "models/PipelineEventGroup.h"
#include "monitor/Monitor.h"
#include "runner/ProcessorRunner.h"

#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif

DEFINE_FLAG_INT32(host_monitor_thread_pool_size, "host monitor thread pool size", 3);

namespace logtail {

HostMonitorInputRunner::HostMonitorInputRunner() {
    RegisterCollector<ProcessEntityCollector>();
    RegisterCollector<CPUCollector>();
    RegisterCollector<SystemCollector>();
    RegisterCollector<MemCollector>();
    RegisterCollector<NetCollector>();
    RegisterCollector<ProcessCollector>();
    RegisterCollector<DiskCollector>();

    size_t threadPoolSize = 1;
    // threadPoolSize should be greater than 0
    if (INT32_FLAG(host_monitor_thread_pool_size) > 0) {
        threadPoolSize = INT32_FLAG(host_monitor_thread_pool_size);
    }
    // threadPoolSize should be less than or equal to the number of registered collectors
    mThreadPool = std::make_unique<ThreadPool>(std::min(threadPoolSize, mRegisteredCollectorMap.size()));
}

void HostMonitorInputRunner::UpdateCollector(const std::string& configName,
                                             const std::vector<std::string>& newCollectorNames,
                                             const std::vector<uint32_t>& newCollectorIntervals,
                                             QueueKey processQueueKey,
                                             size_t inputIndex) {
    std::unique_lock<std::shared_mutex> lock(mRunningConfigMapMutex);
    if (mRunningConfigMap.find(configName) != mRunningConfigMap.end()) {
        LOG_ERROR(sLogger, ("host monitor", "config already running")("config", configName));
        return;
    }
    mRunningConfigMap.emplace(configName, std::unordered_set<std::string>());
    for (size_t i = 0; i < newCollectorNames.size(); ++i) {
        const auto& collectorName = newCollectorNames[i];
        auto iter = mRegisteredCollectorMap.find(collectorName);
        if (iter == mRegisteredCollectorMap.end()) {
            LOG_ERROR(sLogger, ("host monitor", "collector not support")("collector", collectorName));
            continue;
        }
        // add timer event
        HostMonitorCollectConfig collectConfig(
            configName, collectorName, processQueueKey, inputIndex, std::chrono::seconds(newCollectorIntervals[i]));
        auto collector = iter->second(collectConfig);
        auto now = std::chrono::steady_clock::now();
        auto event = std::make_unique<HostMonitorTimerEvent>(now, std::move(collectConfig), std::move(collector));
        Timer::GetInstance()->PushEvent(std::move(event));
        mRunningConfigMap[configName].insert(collectorName);
        LOG_INFO(sLogger, ("host monitor", "add new collector")("collector", collectorName));
    }
}

void HostMonitorInputRunner::RemoveCollector(const std::string& configName) {
    std::unique_lock<std::shared_mutex> lock(mRunningConfigMapMutex);
    mRunningConfigMap.erase(configName);
}

void HostMonitorInputRunner::RemoveAllCollector() {
    std::unique_lock<std::shared_mutex> lock(mRunningConfigMapMutex);
    mRunningConfigMap.clear();
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
    RemoveAllCollector();
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
    std::shared_lock<std::shared_mutex> lock(mRunningConfigMapMutex);
    return !mRunningConfigMap.empty();
}

bool HostMonitorInputRunner::IsCollectTaskValid(const std::string& configName, const std::string& collectorName) {
    std::shared_lock<std::shared_mutex> lock(mRunningConfigMapMutex);
    auto it = mRunningConfigMap.find(configName);
    if (it == mRunningConfigMap.end()) {
        return false;
    }
    return it->second.find(collectorName) != it->second.end();
}

void HostMonitorInputRunner::ScheduleOnce(HostMonitorTimerEvent& event,
                                          const std::chrono::steady_clock::time_point& execTime) {
    if (!ProcessQueueManager::GetInstance()->IsValidToPush(event.mCollectConfig.mProcessQueueKey)) {
        LOG_WARNING(sLogger,
                    ("host monitor push process queue failed", "discard data")("collector",
                                                                               event.mCollectConfig.mCollectorName));
        PushNextTimerEvent(execTime, event);
        return;
    }

    auto collectFn = [this, &event, execTime]() mutable {
        PipelineEventGroup group(std::make_shared<SourceBuffer>());
        std::unique_lock<std::shared_mutex> lock(mRunningConfigMapMutex);
        auto it = mRunningConfigMap.find(event.mCollectConfig.mCollectorName);
        if (it == mRunningConfigMap.end()) {
            LOG_ERROR(sLogger,
                      ("collector not found, will not collect again",
                       "discard data")("collector", event.mCollectConfig.mCollectorName));
            return;
        }
        if (event.mCollector->Collect(&group)) {
            LOG_DEBUG(sLogger,
                      ("host monitor", "collect data")("collector", event.mCollectConfig.mCollectorName)(
                          "size", group.GetEvents().size()));
            if (group.GetEvents().size() > 0) {
                AddHostLabels(group);
                bool result = ProcessorRunner::GetInstance()->PushQueue(
                    event.mCollectConfig.mProcessQueueKey, event.mCollectConfig.mInputIndex, std::move(group));
                if (!result) {
                    LOG_ERROR(sLogger,
                              ("host monitor push process queue failed",
                               "discard data")("collector", event.mCollectConfig.mCollectorName));
                }
            }
        } else {
            LOG_ERROR(sLogger,
                      ("host monitor collect data failed", "collect error")("collector",
                                                                            event.mCollectConfig.mCollectorName));
        }
        PushNextTimerEvent(execTime, event);
    };
    mThreadPool->Add(collectFn);
}

void HostMonitorInputRunner::PushNextTimerEvent(const std::chrono::steady_clock::time_point& execTime,
                                                HostMonitorTimerEvent& event) {
    auto& config = event.mCollectConfig;
    std::chrono::steady_clock::time_point nextExecTime = execTime + config.mInterval;
    while (nextExecTime < std::chrono::steady_clock::now()) {
        nextExecTime += config.mInterval;
    }
    auto newEvent
        = std::make_unique<HostMonitorTimerEvent>(nextExecTime, std::move(config), std::move(event.mCollector));
    Timer::GetInstance()->PushEvent(std::move(newEvent));
}

void HostMonitorInputRunner::AddHostLabels(PipelineEventGroup& group) {
#ifdef __ENTERPRISE__
    const auto* entity = InstanceIdentity::Instance()->GetEntity();
    for (auto& e : group.MutableEvents()) {
        if (!e.Is<MetricEvent>()) {
            continue;
        }
        auto& metricEvent = e.Cast<MetricEvent>();
        if (entity != nullptr) {
            metricEvent.SetTagNoCopy(DEFAULT_INSTANCE_ID_LABEL, entity->GetHostID());
            metricEvent.SetTagNoCopy(DEFAULT_USER_ID_LABEL, entity->GetEcsUserID());
        }
    }
#else
    auto hostIP = group.GetSourceBuffer()->CopyString(LoongCollectorMonitor::mIpAddr);
    for (auto& e : group.MutableEvents()) {
        if (!e.Is<MetricEvent>()) {
            continue;
        }
        auto& metricEvent = e.Cast<MetricEvent>();
        metricEvent.SetTagNoCopy(DEFAULT_HOST_IP_LABEL, StringView(hostIP.data, hostIP.size));
    }
#endif
}

} // namespace logtail

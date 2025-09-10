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
#include <future>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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
    RegisterCollector<DiskCollector>();
    RegisterCollector<ProcessCollector>();
    RegisterCollector<NetCollector>();

    size_t threadPoolSize = 1;
    // threadPoolSize should be greater than 0
    if (INT32_FLAG(host_monitor_thread_pool_size) > 0) {
        threadPoolSize = INT32_FLAG(host_monitor_thread_pool_size);
    }
    // threadPoolSize should be less than or equal to the number of registered collectors
    mThreadPool = std::make_unique<ThreadPool>(threadPoolSize);
}

void HostMonitorInputRunner::UpdateCollector(const std::string& configName,
                                             const std::vector<CollectorInfo>& newCollectorInfos,
                                             QueueKey processQueueKey,
                                             size_t inputIndex) {
    for (size_t i = 0; i < newCollectorInfos.size(); ++i) {
        const auto& collectorName = newCollectorInfos[i].name;

        if (mCollectorCreatorMap.find(collectorName) == mCollectorCreatorMap.end()) {
            LOG_ERROR(sLogger,
                      ("host monitor", "collector not supported")("config", configName)("collector", collectorName));
            continue;
        }
        auto collector = mCollectorCreatorMap.at(collectorName)();

        auto collectContext = std::make_shared<HostMonitorContext>(configName,
                                                                   collectorName,
                                                                   processQueueKey,
                                                                   inputIndex,
                                                                   std::chrono::seconds(newCollectorInfos[i].interval),
                                                                   std::move(collector));
        collectContext->mCollectType = newCollectorInfos[i].type;
        if (!collectContext->mCollector.Init(*collectContext)) {
            LOG_ERROR(sLogger, ("host monitor", "init collector failed")("collector", collectorName));
            continue;
        }
        if (collectContext->mCollectInterval.count() == 0 || collectContext->mReportInterval.count() == 0) {
            LOG_ERROR(sLogger,
                      ("host monitor", "collect interval or report interval is 0, will not collect")(
                          "config", configName)("collector", collectorName));
            continue;
        }
        collectContext->Reset();

        { // add collector to registered collector map
            std::unique_lock<std::shared_mutex> lock(mRegisteredStartTimeMutex);
            CollectorKey key{configName, collectorName};
            mRegisteredStartTime[key] = collectContext->mStartTime;
        }

        // add timer event
        auto event = std::make_unique<HostMonitorTimerEvent>(collectContext);
        Timer::GetInstance()->PushEvent(std::move(event));
        LOG_INFO(sLogger, ("host monitor", "add new collector")("collector", collectorName));
    }
}

void HostMonitorInputRunner::RemoveCollector(const std::string& configName) {
    std::unique_lock<std::shared_mutex> lock(mRegisteredStartTimeMutex);
    auto it = mRegisteredStartTime.begin();
    while (it != mRegisteredStartTime.end()) {
        if (it->first.configName == configName) {
            it = mRegisteredStartTime.erase(it);
        } else {
            ++it;
        }
    }
}

void HostMonitorInputRunner::RemoveAllCollector() {
    std::unique_lock<std::shared_mutex> lock(mRegisteredStartTimeMutex);
    mRegisteredStartTime.clear();
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
    std::shared_lock<std::shared_mutex> lock(mRegisteredStartTimeMutex);
    return !mRegisteredStartTime.empty();
}

bool HostMonitorInputRunner::IsCollectTaskValid(const std::chrono::steady_clock::time_point& startTime,
                                                const std::string& configName,
                                                const std::string& collectorName) {
    std::shared_lock<std::shared_mutex> lock(mRegisteredStartTimeMutex);
    CollectorKey key{configName, collectorName};
    auto it = mRegisteredStartTime.find(key);
    if (it == mRegisteredStartTime.end()) {
        return false;
    }
    return it->second == startTime;
}

void HostMonitorInputRunner::ScheduleOnce(CollectContextPtr context) {
    auto collectFn = [this, context]() {
        try {
            bool result = false;
            if (context->ShouldGenerateMetric()) {
                PipelineEventGroup group(std::make_shared<SourceBuffer>());
                result = context->mCollector.Collect(*context, &group);
                if (result) {
                    LOG_DEBUG(sLogger,
                              ("host monitor",
                               "collect data")("collector", context->mCollectorName)("size", group.GetEvents().size()));
                    if (group.GetEvents().size() > 0) {
                        AddHostLabels(group);
                        PushQueue(context, std::move(group));
                    }
                } else {
                    LOG_ERROR(
                        sLogger,
                        ("host monitor collect data failed", "collect error")("collector", context->mCollectorName));
                }
            } else {
                result = context->mCollector.Collect(*context, nullptr);
                if (!result) {
                    LOG_ERROR(
                        sLogger,
                        ("host monitor collect data failed", "collect error")("collector", context->mCollectorName));
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR(sLogger,
                      ("host monitor collect data failed",
                       "collect error")("collector", context->mCollectorName)("error", e.what()));
        }
        {
            std::shared_lock<std::shared_mutex> lock(mRegisteredStartTimeMutex);
            CollectorKey key{context->mConfigName, context->mCollectorName};
            auto it = mRegisteredStartTime.find(key);
            if (it == mRegisteredStartTime.end() || it->second != context->mStartTime) {
                LOG_DEBUG(sLogger,
                          ("old collector is removed, will not collect again",
                           "discard data")("config", context->mConfigName)("collector", context->mCollectorName));
                return;
            }
        }
        PushNextTimerEvent(context);
    };
    mThreadPool->Add(collectFn);
}

void HostMonitorInputRunner::PushQueue(CollectContextPtr context, PipelineEventGroup&& group) {
    std::shared_lock<std::shared_mutex> lock(mRegisteredStartTimeMutex);
    CollectorKey key{context->mConfigName, context->mCollectorName};
    auto it = mRegisteredStartTime.find(key);
    if (it == mRegisteredStartTime.end() || it->second != context->mStartTime) {
        return;
    }
    bool pushResult
        = ProcessorRunner::GetInstance()->PushQueue(context->mProcessQueueKey, context->mInputIndex, std::move(group));
    if (!pushResult) {
        LOG_ERROR(sLogger,
                  ("host monitor push process queue failed", "discard data")("collector", context->mCollectorName));
    }
}

void HostMonitorInputRunner::PushNextTimerEvent(CollectContextPtr context) {
    if (context->CheckClockRolling()) {
        context->Reset();
    } else {
        auto now = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point nextScheduleTime = context->GetScheduleTime() + context->mCollectInterval;
        time_t nextMetricTime = context->GetMetricTime() + context->mCollectInterval.count();
        int64_t skipCount = 0;
        if (now > nextScheduleTime) {
            skipCount = (now - nextScheduleTime) / context->mCollectInterval;
            nextScheduleTime += (skipCount + 1) * context->mCollectInterval;
            nextMetricTime += (skipCount + 1) * context->mCollectInterval.count();
            LOG_WARNING(sLogger,
                        ("host monitor skip collect", "may casue data unaccurate")(
                            "collector", context->mCollectorName)("skip count", skipCount + 1));
            if (context->mCollectType == HostMonitorCollectType::kMultiValue) {
                context->mCount = (context->mCount + skipCount + 1) % context->mCountPerReport;
            }
        }
        context->SetTime(nextScheduleTime, nextMetricTime);
    }
    auto event = std::make_unique<HostMonitorTimerEvent>(context);
    Timer::GetInstance()->PushEvent(std::move(event));
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

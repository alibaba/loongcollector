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
#include "host_monitor/collector/SelfCheckCollector.h"
#include "host_monitor/collector/SystemCollector.h"
#include "logger/Logger.h"
#include "models/MetricEvent.h"
#include "models/PipelineEventGroup.h"
#include "monitor/MetricManager.h"
#include "monitor/Monitor.h"
#include "monitor/metric_constants/MetricConstants.h"
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
    RegisterCollector<SelfCheckCollector>();

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
            std::unique_lock<std::shared_mutex> lock(mRegisteredCollectorMutex);
            CollectorKey key{configName, collectorName};
            CollectorRunInfo runInfo;
            runInfo.startTime = collectContext->mStartTime;
            runInfo.lastRunTime = collectContext->mStartTime;
            runInfo.interval = std::chrono::seconds(newCollectorInfos[i].interval);
            mRegisteredCollector[key] = runInfo;
        }

        // add timer event
        auto event = std::make_unique<HostMonitorTimerEvent>(collectContext);
        Timer::GetInstance()->PushEvent(std::move(event));
        LOG_INFO(sLogger, ("host monitor", "add new collector")("collector", collectorName));
    }

    mRunningPipelineCount++;
    LoongCollectorMonitor::GetInstance()->SetAgentHostMonitorTotal(mRunningPipelineCount);
}

void HostMonitorInputRunner::RemoveCollector(const std::string& configName) {
    {
        std::unique_lock<std::shared_mutex> lock(mRegisteredCollectorMutex);
        auto it = mRegisteredCollector.begin();
        while (it != mRegisteredCollector.end()) {
            if (it->first.configName == configName) {
                it = mRegisteredCollector.erase(it);
            } else {
                ++it;
            }
        }
    }
    mRunningPipelineCount--;
    LoongCollectorMonitor::GetInstance()->SetAgentHostMonitorTotal(mRunningPipelineCount);
}

void HostMonitorInputRunner::RemoveAllCollector() {
    std::unique_lock<std::shared_mutex> lock(mRegisteredCollectorMutex);
    mRegisteredCollector.clear();
    LoongCollectorMonitor::GetInstance()->SetAgentHostMonitorTotal(0);
}

void HostMonitorInputRunner::Init() {
    if (mIsStarted.exchange(true)) {
        return;
    }

    InitMetrics();
    LOG_INFO(sLogger, ("HostMonitorInputRunner", "Start"));
#ifndef APSARA_UNIT_TEST_MAIN
    mThreadPool->Start();
    Timer::GetInstance()->Init();

    UpdateCollector("loongcollector-host-monitor-self-check",
                    {{SelfCheckCollector::sName, 0, HostMonitorCollectType::kSingleValue}},
                    QueueKey(),
                    0);
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
    std::shared_lock<std::shared_mutex> lock(mRegisteredCollectorMutex);
    return !mRegisteredCollector.empty();
}

bool HostMonitorInputRunner::IsCollectTaskValid(const std::chrono::steady_clock::time_point& startTime,
                                                const std::string& configName,
                                                const std::string& collectorName) {
    std::shared_lock<std::shared_mutex> lock(mRegisteredCollectorMutex);
    CollectorKey key{configName, collectorName};
    auto it = mRegisteredCollector.find(key);
    if (it == mRegisteredCollector.end()) {
        return false;
    }
    return it->second.startTime == startTime;
}

void HostMonitorInputRunner::ScheduleOnce(CollectContextPtr context) {
    auto collectFn = [this, context]() {
        auto startTime = std::chrono::steady_clock::now();
        bool isSelfCheckCollector = (context->mCollectorName == SelfCheckCollector::sName);

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
                        mOutItemsSize->Add(group.DataSize());
                        mOutItemsTotal->Add(group.GetEvents().size());
                    }
                } else {
                    LOG_ERROR(
                        sLogger,
                        ("host monitor collect data failed", "collect error")("collector", context->mCollectorName));
                    mDropItemsTotal->Add(group.GetEvents().size());
                    CollectorMetrics::GetInstance()->UpdateFailMetrics(context->mCollectorName);
                }
            } else {
                result = context->mCollector.Collect(*context, nullptr);
                if (!result) {
                    LOG_ERROR(
                        sLogger,
                        ("host monitor collect data failed", "collect error")("collector", context->mCollectorName));
                    CollectorMetrics::GetInstance()->UpdateFailMetrics(context->mCollectorName);
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR(sLogger,
                      ("host monitor collect data failed",
                       "collect error")("collector", context->mCollectorName)("error", e.what()));
            CollectorMetrics::GetInstance()->UpdateFailMetrics(context->mCollectorName);
        }

        if (!isSelfCheckCollector) {
            mLatencyTimeMs->Add(std::chrono::steady_clock::now() - startTime);
        }
        {
            std::shared_lock<std::shared_mutex> lock(mRegisteredCollectorMutex);
            CollectorKey key{context->mConfigName, context->mCollectorName};
            auto it = mRegisteredCollector.find(key);
            if (it == mRegisteredCollector.end() || it->second.startTime != context->mStartTime) {
                LOG_DEBUG(sLogger,
                          ("old collector is removed, will not collect again",
                           "discard data")("config", context->mConfigName)("collector", context->mCollectorName));
                return;
            }
            it->second.lastRunTime = std::chrono::steady_clock::now();
        }
        PushNextTimerEvent(context);
    };
    mThreadPool->Add(collectFn);
    mLastRunTime->Set(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

void HostMonitorInputRunner::PushQueue(CollectContextPtr context, PipelineEventGroup&& group) {
    std::shared_lock<std::shared_mutex> lock(mRegisteredCollectorMutex);
    CollectorKey key{context->mConfigName, context->mCollectorName};
    auto it = mRegisteredCollector.find(key);
    if (it == mRegisteredCollector.end() || it->second.startTime != context->mStartTime) {
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

void HostMonitorInputRunner::InitMetrics() {
    MetricLabels labels;
    labels.emplace_back(METRIC_LABEL_KEY_RUNNER_NAME, "host_monitor");
    WriteMetrics::GetInstance()->CreateMetricsRecordRef(
        mMetricsRecordRef, MetricCategory::METRIC_CATEGORY_RUNNER, std::move(labels));

    mOutItemsTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_HOST_MONITOR_OUT_ITEMS_TOTAL);
    mOutItemsSize = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_HOST_MONITOR_OUT_ITEMS_SIZE);
    mDropItemsTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_HOST_MONITOR_DROP_ITEMS_TOTAL);
    mLatencyTimeMs = mMetricsRecordRef.CreateTimeCounter(METRIC_RUNNER_HOST_MONITOR_LATENCY_TIME_MS);
    mLastRunTime = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_LAST_RUN_TIME);

    WriteMetrics::GetInstance()->CommitMetricsRecordRef(mMetricsRecordRef);

    // Initialize collector metrics
    CollectorMetrics::GetInstance()->Init();
}

void HostMonitorInputRunner::UpdateMetrics(uint64_t outItemsSize, uint64_t dropItems, uint64_t latencyMs) {
    if (mOutItemsSize) {
        mOutItemsSize->Add(outItemsSize);
    }
    if (mDropItemsTotal && dropItems > 0) {
        mDropItemsTotal->Add(dropItems);
    }
    if (mLatencyTimeMs) {
        mLatencyTimeMs->Add(std::chrono::nanoseconds(latencyMs * 1000000)); // Convert ms to ns
    }
}


} // namespace logtail

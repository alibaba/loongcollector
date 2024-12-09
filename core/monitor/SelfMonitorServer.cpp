/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "SelfMonitorServer.h"

#include "Monitor.h"
#include "PipelineManager.h"
#include "common/LogtailCommonFlags.h"
#include "runner/ProcessorRunner.h"

using namespace std;

namespace logtail {

SelfMonitorServer::SelfMonitorServer() {
}

SelfMonitorServer* SelfMonitorServer::GetInstance() {
    static SelfMonitorServer* ptr = new SelfMonitorServer();
    return ptr;
}

void SelfMonitorServer::Init() {
    mThreadRes = async(launch::async, &SelfMonitorServer::Monitor, this);
}

void SelfMonitorServer::Monitor() {
    LOG_INFO(sLogger, ("self-monitor", "started"));
    int32_t lastMonitorTime = time(NULL);
    {
        unique_lock<mutex> lock(mThreadRunningMux);
        while (mIsThreadRunning) {
            if (mStopCV.wait_for(lock, std::chrono::seconds(1), [this]() { return !mIsThreadRunning; })) {
                break;
            }
            int32_t monitorTime = time(NULL);
            if ((monitorTime - lastMonitorTime) >= 60) { // 60s
                lastMonitorTime = monitorTime;
                SendMetrics();
                SendAlarms();
            }
        }
    }
    SendMetrics();
    SendAlarms();
}

void SelfMonitorServer::Stop() {
    {
        lock_guard<mutex> lock(mThreadRunningMux);
        mIsThreadRunning = false;
    }
    mStopCV.notify_one();
    future_status s = mThreadRes.wait_for(chrono::seconds(1));
    if (s == future_status::ready) {
        LOG_INFO(sLogger, ("self-monitor", "stopped successfully"));
    } else {
        LOG_WARNING(sLogger, ("self-monitor", "forced to stopped"));
    }
}

void SelfMonitorServer::UpdateMetricPipeline(PipelineContext* ctx, SelfMonitorMetricRules* rules) {
    WriteLock lock(mMetricPipelineLock);
    mMetricPipelineCtx = ctx;
    mSelfMonitorMetricRules = rules;
    LOG_INFO(sLogger, ("self-monitor metrics pipeline", "updated"));
}

void SelfMonitorServer::RemoveMetricPipeline() {
    WriteLock lock(mMetricPipelineLock);
    mMetricPipelineCtx = nullptr;
    mSelfMonitorMetricRules = nullptr;
    LOG_INFO(sLogger, ("self-monitor metrics pipeline", "removed"));
}

void SelfMonitorServer::SendMetrics() {
    ReadMetrics::GetInstance()->UpdateMetrics();

    ReadLock lock(mMetricPipelineLock);
    if (mMetricPipelineCtx == nullptr || mSelfMonitorMetricRules == nullptr) {
        return;
    }
    // new pipeline
    vector<SelfMonitorMetricEvent> metricEventList;
    ReadMetrics::GetInstance()->ReadAsSelfMonitorMetricEvents(metricEventList);
    PushSelfMonitorMetricEvents(metricEventList);

    PipelineEventGroup pipelineEventGroup(std::make_shared<SourceBuffer>());
    pipelineEventGroup.SetTagNoCopy(LOG_RESERVED_KEY_SOURCE, LoongCollectorMonitor::mIpAddr);
    ReadAsPipelineEventGroup(pipelineEventGroup);

    shared_ptr<Pipeline> pipeline
        = PipelineManager::GetInstance()->FindConfigByName(mMetricPipelineCtx->GetConfigName());
    if (pipeline.get() != nullptr) {
        if (pipelineEventGroup.GetEvents().size() > 0) {
            ProcessorRunner::GetInstance()->PushQueue(
                pipeline->GetContext().GetProcessQueueKey(), 0, std::move(pipelineEventGroup));
        }
    }
}

bool SelfMonitorServer::ProcessSelfMonitorMetricEvent(SelfMonitorMetricEvent& event,
                                                      const SelfMonitorMetricRule& rule) {
    if (!rule.mEnable) {
        if (mSelfMonitorMetricEventMap.find(event.mKey) != mSelfMonitorMetricEventMap.end()) {
            mSelfMonitorMetricEventMap.erase(event.mKey);
        }
        return false;
    }
    event.SetInterval(rule.mInterval);
    return true;
}

void SelfMonitorServer::PushSelfMonitorMetricEvents(std::vector<SelfMonitorMetricEvent>& events) {
    for (auto event : events) {
        bool shouldSkip = false;
        if (event.mCategory == MetricCategory::METRIC_CATEGORY_AGENT) {
            shouldSkip = !ProcessSelfMonitorMetricEvent(event, mSelfMonitorMetricRules->mAgentMetricsRule);
        } else if (event.mCategory == MetricCategory::METRIC_CATEGORY_RUNNER) {
            shouldSkip = !ProcessSelfMonitorMetricEvent(event, mSelfMonitorMetricRules->mRunnerMetricsRule);
        } else if (event.mCategory == MetricCategory::METRIC_CATEGORY_COMPONENT) {
            shouldSkip = !ProcessSelfMonitorMetricEvent(event, mSelfMonitorMetricRules->mComponentMetricsRule);
        } else if (event.mCategory == MetricCategory::METRIC_CATEGORY_PIPELINE) {
            shouldSkip = !ProcessSelfMonitorMetricEvent(event, mSelfMonitorMetricRules->mPipelineMetricsRule);
        } else if (event.mCategory == MetricCategory::METRIC_CATEGORY_PLUGIN) {
            shouldSkip = !ProcessSelfMonitorMetricEvent(event, mSelfMonitorMetricRules->mPluginMetricsRule);
        } else if (event.mCategory == MetricCategory::METRIC_CATEGORY_PLUGIN_SOURCE) {
            shouldSkip = !ProcessSelfMonitorMetricEvent(event, mSelfMonitorMetricRules->mPluginSourceMetricsRule);
        }
        if (shouldSkip)
            continue;

        if (mSelfMonitorMetricEventMap.find(event.mKey) != mSelfMonitorMetricEventMap.end()) {
            mSelfMonitorMetricEventMap[event.mKey].Merge(event);
        } else {
            mSelfMonitorMetricEventMap[event.mKey] = std::move(event);
        }
    }
}

void SelfMonitorServer::ReadAsPipelineEventGroup(PipelineEventGroup& pipelineEventGroup) {
    for (auto event = mSelfMonitorMetricEventMap.begin(); event != mSelfMonitorMetricEventMap.end();) {
        if (event->second.ShouldSend()) {
            MetricEvent* metricEventPtr = pipelineEventGroup.AddMetricEvent();
            event->second.ReadAsMetricEvent(metricEventPtr);
        }
        if (event->second.ShouldDelete()) {
            event = mSelfMonitorMetricEventMap.erase(event);
        } else {
            event++;
        }
    }
}

void SelfMonitorServer::UpdateAlarmPipeline(PipelineContext* ctx) {
    lock_guard<mutex> lock(mAlarmPipelineMux);
    mAlarmPipelineCtx = ctx;
}

void SelfMonitorServer::SendAlarms() {
}

} // namespace logtail
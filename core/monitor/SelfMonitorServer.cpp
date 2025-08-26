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

#include "monitor/SelfMonitorServer.h"

#include <json/json.h>

#include <fstream>

#include "MetricConstants.h"
#include "Monitor.h"
#include "app_config/AppConfig.h"
#include "common/FileSystemUtil.h"
#include "runner/ProcessorRunner.h"

using namespace std;

namespace logtail {

const string SelfMonitorServer::INTERNAL_DATA_TYPE_ALARM = "__alarm__";
const string SelfMonitorServer::INTERNAL_DATA_TYPE_METRIC = "__metric__";

SelfMonitorServer::SelfMonitorServer() {
}

SelfMonitorServer* SelfMonitorServer::GetInstance() {
    static SelfMonitorServer* sPtr = new SelfMonitorServer();
    return sPtr;
}

void SelfMonitorServer::Init() {
    mThreadRes = async(launch::async, &SelfMonitorServer::Monitor, this);
}

void SelfMonitorServer::Monitor() {
    LOG_INFO(sLogger, ("self-monitor", "started"));
    int32_t lastMonitorTime = time(NULL);
    int32_t lastAlarmTime = time(NULL);
    {
        unique_lock<mutex> lock(mThreadRunningMux);
        while (mIsThreadRunning) {
            if (mStopCV.wait_for(lock, std::chrono::seconds(1), [this]() { return !mIsThreadRunning; })) {
                break;
            }
            int32_t nowTime = time(NULL);
            if ((nowTime - lastMonitorTime) >= 60) { // 60s
                lastMonitorTime = nowTime;
                SendMetrics();
            }
            if ((nowTime - lastAlarmTime) >= 3) { // 3s
                lastAlarmTime = nowTime;
                SendAlarms();
            }
        }
    }
    SendMetrics();
    SendAlarms();
}

void SelfMonitorServer::Stop() {
    AlarmManager::GetInstance()->ForceToSend();
    {
        lock_guard<mutex> lock(mThreadRunningMux);
        mIsThreadRunning = false;
    }
    mStopCV.notify_one();
    if (!mThreadRes.valid()) {
        return;
    }
    future_status s = mThreadRes.wait_for(chrono::seconds(1));
    if (s == future_status::ready) {
        LOG_INFO(sLogger, ("self-monitor", "stopped successfully"));
    } else {
        LOG_WARNING(sLogger, ("self-monitor", "forced to stopped"));
    }
}

void SelfMonitorServer::UpdateMetricPipeline(CollectionPipelineContext* ctx,
                                             size_t inputIndex,
                                             SelfMonitorMetricRules* rules) {
    WriteLock lock(mMetricPipelineLock);
    mMetricPipelineCtx = ctx;
    mMetricInputIndex = inputIndex;
    mSelfMonitorMetricRules = rules;
    LOG_INFO(sLogger, ("self-monitor metrics pipeline", "updated"));
}

void SelfMonitorServer::RemoveMetricPipeline() {
    WriteLock lock(mMetricPipelineLock);
    mMetricPipelineCtx = nullptr;
    mMetricInputIndex = 0;
    mSelfMonitorMetricRules = nullptr;
    LOG_INFO(sLogger, ("self-monitor metrics pipeline", "removed"));
}

void SelfMonitorServer::SendMetrics(bool updateAppInfo) {
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
    pipelineEventGroup.SetMetadata(EventGroupMetaKey::INTERNAL_DATA_TYPE, INTERNAL_DATA_TYPE_METRIC);
    ReadAsPipelineEventGroup(pipelineEventGroup);

    if (updateAppInfo) {
        UpdateAppInfoJson(pipelineEventGroup);
    }

    if (pipelineEventGroup.GetEvents().size() > 0) {
        ProcessorRunner::GetInstance()->PushQueue(
            mMetricPipelineCtx->GetProcessQueueKey(), mMetricInputIndex, std::move(pipelineEventGroup));
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
            LoongCollectorMonitor::GetInstance()->SetAgentMetric(event);
            shouldSkip = !ProcessSelfMonitorMetricEvent(event, mSelfMonitorMetricRules->mAgentMetricsRule);
        } else if (event.mCategory == MetricCategory::METRIC_CATEGORY_RUNNER) {
            LoongCollectorMonitor::GetInstance()->SetRunnerMetric(event.GetLabel(METRIC_LABEL_KEY_RUNNER_NAME), event);
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
        if (shouldSkip) {
            continue;
        }

        if (mSelfMonitorMetricEventMap.find(event.mKey) != mSelfMonitorMetricEventMap.end()) {
            mSelfMonitorMetricEventMap[event.mKey].Merge(event);
        } else {
            mSelfMonitorMetricEventMap[event.mKey] = event;
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

void SelfMonitorServer::UpdateAlarmPipeline(CollectionPipelineContext* ctx, size_t inputIndex) {
    WriteLock lock(mAlarmPipelineMux);
    mAlarmPipelineCtx = ctx;
    mAlarmInputIndex = inputIndex;
    LOG_INFO(sLogger, ("self-monitor alarms pipeline", "updated"));
}

void SelfMonitorServer::RemoveAlarmPipeline() {
    WriteLock lock(mAlarmPipelineMux);
    mAlarmPipelineCtx = nullptr;
    mAlarmInputIndex = 0;
    LOG_INFO(sLogger, ("self-monitor alarms pipeline", "removed"));
}

void SelfMonitorServer::SendAlarms() {
    // metadata:
    // INTERNAL_DATA_TARGET_REGION:${region}
    // INTERNAL_DATA_TYPE:__alarm__
    vector<PipelineEventGroup> pipelineEventGroupList;
    AlarmManager::GetInstance()->FlushAllRegionAlarm(pipelineEventGroupList);

    ReadLock lock(mAlarmPipelineMux);
    if (mAlarmPipelineCtx == nullptr) {
        return;
    }

    for (auto& pipelineEventGroup : pipelineEventGroupList) {
        if (pipelineEventGroup.GetEvents().size() > 0) {
            ProcessorRunner::GetInstance()->PushQueue(
                mAlarmPipelineCtx->GetProcessQueueKey(), mAlarmInputIndex, std::move(pipelineEventGroup));
        }
    }
}

void SelfMonitorServer::SendStartMetric() {
    // 等待mMetricPipelineCtx存在，最多等待1秒
    auto startTime = std::chrono::steady_clock::now();
    while (true) {
        ReadLock lock(mMetricPipelineLock);
        if (mMetricPipelineCtx != nullptr && mSelfMonitorMetricRules != nullptr) {
            auto endTime = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            LOG_INFO(sLogger,
                     ("send start metric", "wait metric pipeline ready")("cost milliseconds", duration.count()));
            break;
        }
        lock.unlock();

        // 检查是否超时
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
        if (elapsed.count() >= 1000) { // 1秒超时，实测正常启动应该1ms内完成
            LOG_WARNING(sLogger, ("send start metric", "wait metric pipeline ready timeout, skip"));
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    // 启动时需要立刻更新一下cpu和memory指标，因为正常启动时，cpu和memory指标会等30s更新一次
    LogtailMonitor::GetInstance()->UpdateCpuMem();

    // 调用SendMetrics，并启用UpdateAppInfo
    // TODO: SendMetrics(true);
    SendMetrics();
    LOG_INFO(sLogger, ("send start metric", "sent successfully"));
}

void SelfMonitorServer::UpdateAppInfoJson(const PipelineEventGroup& pipelineEventGroup) {
    // 读取现有的app_info.json
    string appInfoFile = GetAgentAppInfoFile();
    Json::Value appInfoJson;

    // 尝试读取现有文件
    ifstream file(appInfoFile);
    if (file.is_open()) {
        Json::CharReaderBuilder builder;
        string errors;
        if (Json::parseFromStream(builder, file, &appInfoJson, &errors)) {
            file.close();
        } else {
            file.close();
            LOG_WARNING(sLogger, ("failed to parse existing app_info.json", errors));
        }
    }

    const auto& events = pipelineEventGroup.GetEvents();
    for (const auto& eventPtr : events) {
        if (!eventPtr.Is<MetricEvent>()) {
            continue;
        }

        const auto& metricEvent = eventPtr.Cast<MetricEvent>();
        const std::string metricName = metricEvent.GetName().to_string();

        // 检查是否是agent指标
        if (metricName == MetricCategory::METRIC_CATEGORY_AGENT) {
            // TODO: 从metricEvent中提取agent指标设置到appInfoJson
        }
        // 检查是否是runner指标
        else if (metricName == MetricCategory::METRIC_CATEGORY_RUNNER) {
            // TODO: 从metricEvent中提取runner指标，例如所有已经启动的runner的名字
        }
    }
    // 写入更新后的app_info.json
    string appInfo = appInfoJson.toStyledString();
    OverwriteFile(appInfoFile, appInfo);
    LOG_INFO(sLogger, ("updated app_info.json with additional metric info", appInfo));
}

} // namespace logtail

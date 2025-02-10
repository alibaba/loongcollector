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

#pragma once

#include <string>

#include "collection_pipeline/CollectionPipeline.h"
#include "monitor/Monitor.h"

namespace logtail {

class SelfMonitorServer {
public:
    SelfMonitorServer(const SelfMonitorServer&) = delete;
    SelfMonitorServer& operator=(const SelfMonitorServer&) = delete;
    static SelfMonitorServer* GetInstance();

    void Init();
    void Monitor();
    void Stop();

    void UpdateMetricPipeline(CollectionPipelineContext* ctx, size_t inputIndex, SelfMonitorMetricRules* rules);
    void RemoveMetricPipeline();
    void UpdateAlarmPipeline(CollectionPipelineContext* ctx, size_t inputIndex);
    void RemoveAlarmPipeline();

    static const std::string INTERNAL_DATA_TYPE_ALARM;
    static const std::string INTERNAL_DATA_TYPE_METRIC;

private:
    SelfMonitorServer();
    ~SelfMonitorServer() = default;

    std::future<void> mThreadRes;
    std::mutex mThreadRunningMux;
    bool mIsThreadRunning = true;
    std::condition_variable mStopCV;

    // metrics
    void SendMetrics();
    bool ProcessSelfMonitorMetricEvent(SelfMonitorMetricEvent& event, const SelfMonitorMetricRule& rule);
    void PushSelfMonitorMetricEvents(std::vector<SelfMonitorMetricEvent>& events);
    void ReadAsPipelineEventGroup(PipelineEventGroup& pipelineEventGroup);

    mutable ReadWriteLock mMetricPipelineLock;
    CollectionPipelineContext* mMetricPipelineCtx = nullptr;
    size_t mMetricInputIndex = 0;
    SelfMonitorMetricRules* mSelfMonitorMetricRules = nullptr;
    SelfMonitorMetricEventMap mSelfMonitorMetricEventMap;

    // alarms
    void SendAlarms();

    mutable ReadWriteLock mAlarmPipelineMux;
    CollectionPipelineContext* mAlarmPipelineCtx = nullptr;
    size_t mAlarmInputIndex = 0;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class InputInternalAlarmsUnittest;
    friend class InputInternalMetricsUnittest;
#endif
};

} // namespace logtail

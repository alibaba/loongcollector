/*
 * Copyright 2023 iLogtail Authors
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

#include "pipeline/plugin/instance/ProcessorInstance.h"

#include <cstdint>

#include "common/TimeUtil.h"
#include "logger/Logger.h"
#include "monitor/metric_constants/MetricConstants.h"

using namespace std;

namespace logtail {

bool ProcessorInstance::Init(const Json::Value& config, PipelineContext& context) {
    mPlugin->SetContext(context);
    mPlugin->SetMetricsRecordRef(Name(), PluginID());
    if (!mPlugin->Init(config)) {
        return false;
    }

    // should init plugin first， then could GetMetricsRecordRef from plugin
    mInEventsTotal = mPlugin->GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_IN_EVENTS_TOTAL);
    mOutEventsTotal = mPlugin->GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_EVENTS_TOTAL);
    mInSizeBytes = mPlugin->GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_IN_SIZE_BYTES);
    mOutSizeBytes = mPlugin->GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_SIZE_BYTES);
    mTotalProcessTimeMs = mPlugin->GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_TOTAL_PROCESS_TIME_MS);

    return true;
}

void ProcessorInstance::Process(vector<PipelineEventGroup>& eventGroupList) {
    if (eventGroupList.empty()) {
        return;
    } 
    for (const auto& eventGroup : eventGroupList) {
        mInEventsTotal->Add(eventGroup.GetEvents().size());
        mInSizeBytes->Add(eventGroup.DataSize());
    }

    auto before = chrono::system_clock::now();
    mPlugin->Process(eventGroupList);
    mTotalProcessTimeMs->Add(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - before).count());

    for (const auto& eventGroup : eventGroupList) {
        mOutEventsTotal->Add(eventGroup.GetEvents().size());
        mOutSizeBytes->Add(eventGroup.DataSize());
    }    
}

} // namespace logtail

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

#include "plugin/instance/ProcessorInstance.h"

#include <cstdint>

#include "common/TimeUtil.h"
#include "logger/Logger.h"
#include "monitor/MetricConstants.h"

using namespace std;

namespace logtail {

bool ProcessorInstance::Init(const Json::Value& config, PipelineContext& context) {
    mPlugin->SetContext(context);
    mPlugin->SetMetricsRecordRef(Name(), PluginID(), NodeID(), ChildNodeID());
    if (!mPlugin->Init(config)) {
        return false;
    }

    // should init plugin first， then could GetMetricsRecordRef from plugin
    mProcInRecordsTotal = mPlugin->GetMetricsRecordRef().CreateCounter(METRIC_PROC_IN_RECORDS_TOTAL);
    mProcOutRecordsTotal = mPlugin->GetMetricsRecordRef().CreateCounter(METRIC_PROC_OUT_RECORDS_TOTAL);
    mProcTimeMS = mPlugin->GetMetricsRecordRef().CreateCounter(METRIC_PROC_TIME_MS);

    return true;
}

void ProcessorInstance::Process(vector<PipelineEventGroup>& logGroupList) {
    if (logGroupList.empty()) {
        return;
    } 
    for (const auto& logGroup : logGroupList) {
        mProcInRecordsTotal->Add(logGroup.GetEvents().size());
    }

    uint64_t startTime = GetCurrentTimeInMicroSeconds();
    mPlugin->Process(logGroupList);
    uint64_t durationTime = GetCurrentTimeInMicroSeconds() - startTime;

    mProcTimeMS->Add(durationTime);

    for (const auto& logGroup : logGroupList) {
        mProcOutRecordsTotal->Add(logGroup.GetEvents().size());
    }    
}

} // namespace logtail

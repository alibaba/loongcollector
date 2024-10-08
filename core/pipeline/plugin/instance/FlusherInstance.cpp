// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "pipeline/plugin/instance/FlusherInstance.h"

#include "monitor/metric_constants/MetricConstants.h"

namespace logtail {
bool FlusherInstance::Init(const Json::Value& config, PipelineContext& context, Json::Value& optionalGoPipeline) {
    mPlugin->SetContext(context);
    mPlugin->SetPluginID(PluginID());
    mPlugin->SetMetricsRecordRef(Name(), PluginID());
    if (!mPlugin->Init(config, optionalGoPipeline)) {
        return false;
    }

    mInEventsTotal = mPlugin->GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_IN_EVENTS_TOTAL);
    mInSizeBytes = mPlugin->GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_IN_SIZE_BYTES);
    return true;
}

bool FlusherInstance::Send(PipelineEventGroup&& g) {
    mInEventsTotal->Add(g.GetEvents().size());
    mInSizeBytes->Add(g.DataSize());
    return mPlugin->Send(std::move(g));
}

} // namespace logtail

// Copyright 2026 iLogtail Authors
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

#include "plugin/input/InputAgentSight.h"

#include <unordered_map>

#include "ebpf/EBPFServer.h"
#include "ebpf/include/export.h"
#include "logger/Logger.h"
#include "monitor/metric_models/ReentrantMetricsRecord.h"

namespace logtail {

const std::string InputAgentSight::sName = "input_agentsight";

bool InputAgentSight::Init(const Json::Value& config, Json::Value&) {
    // Only keys listed here get a counter: ReentrantMetricsRecord::Init builds mCounters from this map
    // alone, and GetCounter returns nullptr for anything else. Since ADD_COUNTER null-checks, a metric
    // the manager reads but this map omits degrades to a silent no-op rather than a build or run error
    // — so every key AgentsightManager fetches must appear here.
    static const std::unordered_map<std::string, MetricType> kMetricKeys = {
        {METRIC_PLUGIN_IN_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_EBPF_LOSS_KERNEL_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_OUT_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
    };

    mPluginMetricPtr = std::make_shared<PluginMetricManager>(
        GetMetricsRecordRef().GetLabels(), kMetricKeys, MetricCategory::METRIC_CATEGORY_PLUGIN_SOURCE);
    return mSecurityOptions.Init(ebpf::SecurityProbeType::AGENTSIGHT_OBSERVE, config, mContext, sName);
}

bool InputAgentSight::Start() {
    ebpf::EBPFServer::GetInstance()->Init();
    if (!ebpf::EBPFServer::GetInstance()->IsSupportedEnv(logtail::ebpf::PluginType::AGENTSIGHT_OBSERVE)) {
        return false;
    }
    return ebpf::EBPFServer::GetInstance()->EnablePlugin(mContext->GetConfigName(),
                                                         mIndex,
                                                         logtail::ebpf::PluginType::AGENTSIGHT_OBSERVE,
                                                         mContext,
                                                         &mSecurityOptions,
                                                         mPluginMetricPtr);
}

bool InputAgentSight::Stop(bool isPipelineRemoving) {
    if (!isPipelineRemoving) {
        ebpf::EBPFServer::GetInstance()->SuspendPlugin(mContext->GetConfigName(),
                                                       logtail::ebpf::PluginType::AGENTSIGHT_OBSERVE);
        return true;
    }
    return ebpf::EBPFServer::GetInstance()->DisablePlugin(mContext->GetConfigName(),
                                                          logtail::ebpf::PluginType::AGENTSIGHT_OBSERVE);
}

} // namespace logtail

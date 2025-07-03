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

#include "plugin/input/InputProcessSecurity.h"

#include "ebpf/EBPFServer.h"
#include "ebpf/include/export.h"

using namespace std;

namespace logtail {

const std::string InputProcessSecurity::sName = "input_process_security";

bool InputProcessSecurity::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    ebpf::EBPFServer::GetInstance()->Init();
    ebpf::EBPFServer::GetInstance()->SetPluginLifecycleState(logtail::ebpf::PluginType::PROCESS_SECURITY,
                                                             mContext->GetConfigName(),
                                                             logtail::ebpf::LifecycleState::INITIALIZED);
    static const std::unordered_map<std::string, MetricType> metricKeys = {
        {METRIC_PLUGIN_IN_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_EBPF_LOSS_KERNEL_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_OUT_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_EBPF_PROCESS_CACHE_ENTRIES_NUM, MetricType::METRIC_TYPE_INT_GAUGE},
        {METRIC_PLUGIN_EBPF_PROCESS_CACHE_MISS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
    };

    mPluginMetricPtr = std::make_shared<PluginMetricManager>(
        GetMetricsRecordRef().GetLabels(), metricKeys, MetricCategory::METRIC_CATEGORY_PLUGIN_SOURCE);
    return mSecurityOptions.Init(ebpf::SecurityProbeType::PROCESS, config, mContext, sName);
}

bool InputProcessSecurity::Start() {
    ebpf::EBPFServer::GetInstance()->Init();
    if (!ebpf::EBPFServer::GetInstance()->IsSupportedEnv(logtail::ebpf::PluginType::PROCESS_SECURITY)) {
        return false;
    }
    ebpf::EBPFServer::GetInstance()->SetPluginLifecycleState(logtail::ebpf::PluginType::PROCESS_SECURITY,
                                                             mContext->GetConfigName(),
                                                             logtail::ebpf::LifecycleState::RUNNING);
    return ebpf::EBPFServer::GetInstance()->EnablePlugin(mContext->GetConfigName(),
                                                         mIndex,
                                                         logtail::ebpf::PluginType::PROCESS_SECURITY,
                                                         mContext,
                                                         &mSecurityOptions,
                                                         mPluginMetricPtr);
}

bool InputProcessSecurity::Stop(bool isPipelineRemoving) {
    if (!isPipelineRemoving) {
        if(!ebpf::EBPFServer::GetInstance()->IsPluginInited(logtail::ebpf::PluginType::PROCESS_SECURITY, mContext->GetConfigName())) {
            // The input plugin type has been changed in the updated configuration,
            // so the plugin is disabled
            LOG_INFO(sLogger, ("detect plugin type change, disable ebpf process plugin, config", mContext->GetConfigName()));
            ebpf::EBPFServer::GetInstance()->DisablePlugin(mContext->GetConfigName(),
                                                          logtail::ebpf::PluginType::PROCESS_SECURITY);
            ebpf::EBPFServer::GetInstance()->SetPluginLifecycleState(logtail::ebpf::PluginType::PROCESS_SECURITY,
                                                                     mContext->GetConfigName(),
                                                                     logtail::ebpf::LifecycleState::STOPPED);
        } else {
            LOG_INFO(sLogger, ("suspend ebpf process plugin for config update, config", mContext->GetConfigName()));
            ebpf::EBPFServer::GetInstance()->SuspendPlugin(mContext->GetConfigName(),
                                                       logtail::ebpf::PluginType::PROCESS_SECURITY);
            ebpf::EBPFServer::GetInstance()->SetPluginLifecycleState(logtail::ebpf::PluginType::PROCESS_SECURITY,
                                                                     mContext->GetConfigName(),
                                                                     logtail::ebpf::LifecycleState::SUSPENDED);
        }
        return true;
    }

    ebpf::EBPFServer::GetInstance()->SetPluginLifecycleState(logtail::ebpf::PluginType::PROCESS_SECURITY,
                                                             mContext->GetConfigName(),
                                                             logtail::ebpf::LifecycleState::STOPPED);
    return ebpf::EBPFServer::GetInstance()->DisablePlugin(mContext->GetConfigName(),
                                                          logtail::ebpf::PluginType::PROCESS_SECURITY);
}

} // namespace logtail

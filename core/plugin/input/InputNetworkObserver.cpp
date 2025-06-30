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

#include "plugin/input/InputNetworkObserver.h"

#include "ebpf/Config.h"
#include "ebpf/EBPFServer.h"
#include "ebpf/include/export.h"
#include "logger/Logger.h"
#include "monitor/metric_models/ReentrantMetricsRecord.h"

using namespace std;

namespace logtail {

const std::string InputNetworkObserver::sName = "input_network_observer";

bool InputNetworkObserver::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    static const std::unordered_map<std::string, MetricType> metricKeys = {
        {METRIC_PLUGIN_IN_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_EBPF_LOSS_KERNEL_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_OUT_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_EBPF_PROCESS_CACHE_ENTRIES_NUM, MetricType::METRIC_TYPE_INT_GAUGE},
        {METRIC_PLUGIN_EBPF_PROCESS_CACHE_MISS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_CONNECTION_NUM, MetricType::METRIC_TYPE_INT_GAUGE},
        {METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_AGGREGATE_KEY_NUM, MetricType::METRIC_TYPE_INT_GAUGE},
        {METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_WORKER_HANDLE_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_PROTOCOL_PARSE_RECORDS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_EBPF_META_ATTACH_SUCCESS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_EBPF_META_ATTACH_FAILED_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_EBPF_META_ATTACH_ROLLBACK_TOTAL, MetricType::METRIC_TYPE_COUNTER},
    };

    mPluginMetricPtr = std::make_shared<PluginMetricManager>(
        GetMetricsRecordRef().GetLabels(), metricKeys, MetricCategory::METRIC_CATEGORY_PLUGIN_SOURCE);
    return ebpf::InitObserverNetworkOption(config, mNetworkOption, mContext, sName);
}

bool InputNetworkObserver::Start() {
    ebpf::EBPFServer::GetInstance()->Init();
    if (!ebpf::EBPFServer::GetInstance()->IsSupportedEnv(logtail::ebpf::PluginType::NETWORK_OBSERVE)) {
        return false;
    }
    return ebpf::EBPFServer::GetInstance()->EnablePlugin(mContext->GetConfigName(),
                                                         mIndex,
                                                         logtail::ebpf::PluginType::NETWORK_OBSERVE,
                                                         mContext,
                                                         &mNetworkOption,
                                                         mPluginMetricPtr);
}

bool InputNetworkObserver::Stop(bool isPipelineRemoving) {
    if (!isPipelineRemoving) {
        ebpf::EBPFServer::GetInstance()->SuspendPlugin(mContext->GetConfigName(),
                                                       logtail::ebpf::PluginType::NETWORK_OBSERVE);
        return true;
    }
    return ebpf::EBPFServer::GetInstance()->DisablePlugin(mContext->GetConfigName(),
                                                          logtail::ebpf::PluginType::NETWORK_OBSERVE);
}

} // namespace logtail

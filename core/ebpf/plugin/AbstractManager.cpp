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

#include "AbstractManager.h"

#include <coolbpf/security/type.h>

#include "common/TimeUtil.h"
#include "logger/Logger.h"
#include "monitor/metric_models/ReentrantMetricsRecord.h"

namespace logtail {
namespace ebpf {
AbstractManager::AbstractManager(std::shared_ptr<ProcessCacheManager> processCacheMgr,
                                 std::shared_ptr<SourceManager> sourceManager,
                                 moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                                 PluginMetricManagerPtr mgr)
    : mProcessCacheManager(processCacheMgr), mSourceManager(sourceManager), mCommonEventQueue(queue), mMetricMgr(mgr) {
    mTimeDiff = GetTimeDiffFromMonotonic();

    if (!mMetricMgr) {
        return;
    }

    // init metrics
    MetricLabels pollKernelEventsLabels
        = {{METRIC_LABEL_KEY_RECV_EVENT_STAGE, METRIC_LABEL_VALUE_RECV_EVENT_STAGE_POLL_KERNEL}};
    auto ref = mMetricMgr->GetOrCreateReentrantMetricsRecordRef(pollKernelEventsLabels);
    mRefAndLabels.emplace_back(pollKernelEventsLabels);
    mRecvKernelEventsTotal = ref->GetCounter(METRIC_PLUGIN_IN_EVENTS_TOTAL);
    mLossKernelEventsTotal = ref->GetCounter(METRIC_PLUGIN_EBPF_LOSS_KERNEL_EVENTS_TOTAL);

    MetricLabels eventTypeLabels = {{METRIC_LABEL_KEY_EVENT_TYPE, METRIC_LABEL_VALUE_EVENT_TYPE_LOG}};
    ref = mMetricMgr->GetOrCreateReentrantMetricsRecordRef(eventTypeLabels);
    mRefAndLabels.emplace_back(eventTypeLabels);
    mPushLogsTotal = ref->GetCounter(METRIC_PLUGIN_OUT_EVENTS_TOTAL);
    mPushLogGroupTotal = ref->GetCounter(METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL);

    eventTypeLabels = {{METRIC_LABEL_KEY_EVENT_TYPE, METRIC_LABEL_VALUE_EVENT_TYPE_METRIC}};
    ref = mMetricMgr->GetOrCreateReentrantMetricsRecordRef(eventTypeLabels);
    mRefAndLabels.emplace_back(eventTypeLabels);
    mPushMetricsTotal = ref->GetCounter(METRIC_PLUGIN_OUT_EVENTS_TOTAL);
    mPushMetricGroupTotal = ref->GetCounter(METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL);

    eventTypeLabels = {{METRIC_LABEL_KEY_EVENT_TYPE, METRIC_LABEL_VALUE_EVENT_TYPE_TRACE}};
    mRefAndLabels.emplace_back(eventTypeLabels);
    ref = mMetricMgr->GetOrCreateReentrantMetricsRecordRef(eventTypeLabels);
    mPushSpansTotal = ref->GetCounter(METRIC_PLUGIN_OUT_EVENTS_TOTAL);
    mPushSpanGroupTotal = ref->GetCounter(METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL);
}

AbstractManager::~AbstractManager() {
    for (auto& item : mRefAndLabels) {
        if (mMetricMgr) {
            mMetricMgr->ReleaseReentrantMetricsRecordRef(item);
        }
    }
}

int AbstractManager::GetCallNameIdx(const std::string& callName) {
    if (callName == "security_file_permission") {
        return SECURE_FUNC_TRACEPOINT_FUNC_SECURITY_FILE_PERMISSION;
    }
    if (callName == "security_mmap_file") {
        return SECURE_FUNC_TRACEPOINT_FUNC_SECURITY_MMAP_FILE;
    }
    if (callName == "security_path_truncate") {
        return SECURE_FUNC_TRACEPOINT_FUNC_SECURITY_PATH_TRUNCATE;
    }
    if (callName == "sys_write") {
        return SECURE_FUNC_TRACEPOINT_FUNC_SYS_WRITE;
    }
    if (callName == "sys_read") {
        return SECURE_FUNC_TRACEPOINT_FUNC_SYS_READ;
    }
    if (callName == "tcp_close") {
        return SECURE_FUNC_TRACEPOINT_FUNC_TCP_CLOSE;
    }
    if (callName == "tcp_connect") {
        return SECURE_FUNC_TRACEPOINT_FUNC_TCP_CONNECT;
    }
    if (callName == "tcp_sendmsg") {
        return SECURE_FUNC_TRACEPOINT_FUNC_TCP_SENDMSG;
    }
    LOG_WARNING(sLogger, ("unknown call name", callName));
    return -1;
}

} // namespace ebpf
} // namespace logtail

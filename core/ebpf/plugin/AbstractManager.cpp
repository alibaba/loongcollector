// Copyright 2025 iLogtail Authors
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

#include "monitor/metric_models/ReentrantMetricsRecord.h"

namespace logtail::ebpf {
AbstractManager::AbstractManager(const std::shared_ptr<ProcessCacheManager>& processCacheMgr,
                                 const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                                 moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                                 const PluginMetricManagerPtr& metricManager)
    : mProcessCacheManager(processCacheMgr),
      mEBPFAdapter(eBPFAdapter),
      mCommonEventQueue(queue),
      mMetricMgr(metricManager) {
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
}

AbstractManager::~AbstractManager() {
    for (auto& item : mRefAndLabels) {
        if (mMetricMgr) {
            mMetricMgr->ReleaseReentrantMetricsRecordRef(item);
        }
    }
}

} // namespace logtail::ebpf

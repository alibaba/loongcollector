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

#include "ProcessSecurityManager.h"

#include <coolbpf/security/type.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include "Lock.h"
#include "Logger.h"
#include "TimeKeeper.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/queue/ProcessQueueItem.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/HashUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/TimeUtil.h"
#include "common/magic_enum.hpp"
#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/Config.h"
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/plugin/ProcessCacheManager.h"
#include "ebpf/type/table/BaseElements.h"

namespace logtail::ebpf {
ProcessSecurityManager::ProcessSecurityManager(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
                                               const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                                               moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                                               EventPool* pool)
    : AbstractManager(processCacheManager, eBPFAdapter, queue, pool),
      mAggregateTree(
          4096,
          [](std::unique_ptr<ProcessEventGroup>& base, const std::shared_ptr<CommonEvent>& other) {
              base->mInnerEvents.emplace_back(other);
          },
          [](const std::shared_ptr<CommonEvent>& in,
             [[maybe_unused]] std::shared_ptr<SourceBuffer>& sourceBuffer) -> std::unique_ptr<ProcessEventGroup> {
              auto* processEvent = static_cast<ProcessEvent*>(in.get());
              if (processEvent) {
                  return std::make_unique<ProcessEventGroup>(processEvent->mPid, processEvent->mKtime);
              }
              return nullptr;
          }) {
}

int ProcessSecurityManager::Init() {
    if (mInited) {
        return 0;
    }
    mInited = true;
    return 0;
}

int ProcessSecurityManager::AddOrUpdateConfig(
    const CollectionPipelineContext* ctx,
    uint32_t index,
    const PluginMetricManagerPtr& metricMgr,
    [[maybe_unused]] const std::variant<SecurityOptions*, ObserverNetworkOption*>& opt) {
    if (!ctx) {
        LOG_ERROR(sLogger, ("ctx is null", ""));
        return -1;
    }

    if (metricMgr) {
        MetricLabels eventTypeLabels = {{METRIC_LABEL_KEY_EVENT_TYPE, METRIC_LABEL_VALUE_EVENT_TYPE_LOG}};
        auto ref = metricMgr->GetOrCreateReentrantMetricsRecordRef(eventTypeLabels);
        mRefAndLabels.emplace_back(eventTypeLabels);
        mPushLogsTotal = ref->GetCounter(METRIC_PLUGIN_OUT_EVENTS_TOTAL);
        mPushLogGroupTotal = ref->GetCounter(METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL);
    }

    auto processCacheMgr = GetProcessCacheManager();
    if (processCacheMgr == nullptr) {
        LOG_WARNING(sLogger, ("ProcessCacheManager is null", ""));
        return 1;
    }

    processCacheMgr->MarkProcessEventFlushStatus(true);
    if (RegisteredConfigCount() != 0) {
        // update
        LOG_DEBUG(sLogger, ("ProcessSecurity Update", ""));
        // update config (BPF tailcall, filter map etc.)
        if (update(opt)) {
            LOG_WARNING(sLogger, ("ProcessSecurity Update failed", ""));
            return 1;
        }
        if (resume(opt)) {
            LOG_WARNING(sLogger, ("ProcessSecurity Resume Failed", ""));
            return 1;
        }
        return 0;
    }

    mMetricMgr = metricMgr;
    mPluginIndex = index;
    mPipelineCtx = ctx;
    mQueueKey = ctx->GetProcessQueueKey();

    mRegisteredConfigCount = 1;
    return 0;
}

int ProcessSecurityManager::RemoveConfig(const std::string&) {
    for (auto& item : mRefAndLabels) {
        if (mMetricMgr) {
            mMetricMgr->ReleaseReentrantMetricsRecordRef(item);
        }
    }
    auto processCacheMgr = GetProcessCacheManager();
    if (processCacheMgr == nullptr) {
        LOG_WARNING(sLogger, ("ProcessCacheManager is null", ""));
        return 1;
    }
    processCacheMgr->MarkProcessEventFlushStatus(false);
    mRegisteredConfigCount = 0;
    return 0;
}

int ProcessSecurityManager::Destroy() {
    mInited = false;
    return 0;
}

std::array<size_t, 1> GenerateAggKeyForProcessEvent(ProcessEvent* event) {
    // calculate agg key
    std::array<size_t, 1> hashResult{};
    std::hash<uint64_t> hasher;

    std::array<uint64_t, 2> arr = {uint64_t(event->mPid), event->mKtime};
    for (uint64_t x : arr) {
        AttrHashCombine(hashResult[0], hasher(x));
    }
    return hashResult;
}

int ProcessSecurityManager::HandleEvent(const std::shared_ptr<CommonEvent>& event) {
    if (!event) {
        LOG_ERROR(sLogger, ("cannot handle", "event is null"));
        return 1;
    }
    auto* processEvent = static_cast<ProcessEvent*>(event.get());
    LOG_DEBUG(sLogger,
              ("receive event, pid", processEvent->mPid)("ktime", processEvent->mKtime)(
                  "eventType", magic_enum::enum_name(event->mEventType)));
    if (processEvent == nullptr) {
        LOG_ERROR(sLogger,
                  ("failed to convert CommonEvent to ProcessEvent, kernel event type",
                   magic_enum::enum_name(event->GetKernelEventType()))("PluginType",
                                                                       magic_enum::enum_name(event->GetPluginType())));
        return 1;
    }

    // calculate agg key
    std::array<size_t, 1> hashResult = GenerateAggKeyForProcessEvent(processEvent);
    bool ret = mAggregateTree.Aggregate(event, hashResult);
    LOG_DEBUG(sLogger, ("after aggregate", ret));

    return 0;
}

StringBuffer ToStringBuffer(const std::shared_ptr<SourceBuffer>& sourceBuffer, int32_t val) {
    auto buf = sourceBuffer->AllocateStringBuffer(kMaxInt32Width);
    auto end = fmt::format_to_n(buf.data, buf.capacity, "{}", val);
    *end.out = '\0';
    buf.size = end.size;
    return buf;
}

int ProcessSecurityManager::SendEvents() {
    if (!IsRunning()) {
        return 0;
    }
    auto nowMs = TimeKeeper::GetInstance()->NowMs();
    bool timeTriggered = nowMs - mLastSendTimeMs >= mSendIntervalMs;
    bool eventCountTriggered
        = mAggregateTree.EventCount() >= static_cast<size_t>(INT32_FLAG(ebpf_max_aggregate_events));
    if (!timeTriggered && !eventCountTriggered) {
        return 0;
    }
    mLastSendTimeMs = nowMs;

    SIZETAggTree<ProcessEventGroup, std::shared_ptr<CommonEvent>> aggTree = this->mAggregateTree.GetAndReset();

    // read aggregator
    auto nodes = aggTree.GetNodesWithAggDepth(1);
    LOG_DEBUG(sLogger, ("enter aggregator ...", nodes.size()));
    if (nodes.empty()) {
        LOG_DEBUG(sLogger, ("empty nodes...", ""));
        return 0;
    }

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup sharedEventGroup(sourceBuffer);
    PipelineEventGroup eventGroup(sourceBuffer);
    for (auto& node : nodes) {
        LOG_DEBUG(sLogger, ("child num", node->mChild.size()));
        // convert to a item and push to process queue
        aggTree.ForEach(node, [&](const ProcessEventGroup* group) {
            auto sharedEvent = sharedEventGroup.CreateLogEvent(true, mEventPool);
            // represent a process ...
            auto processCacheMgr = GetProcessCacheManager();
            if (processCacheMgr == nullptr) {
                LOG_WARNING(sLogger, ("ProcessCacheManager is null", ""));
                return;
            }
            auto hit = processCacheMgr->AttachProcessData(group->mPid, group->mKtime, *sharedEvent, eventGroup);
            if (!hit) {
                LOG_WARNING(sLogger, ("cannot find tags for pid", group->mPid)("ktime", group->mKtime));
                return;
            }

            for (const auto& innerEvent : group->mInnerEvents) {
                auto* logEvent = eventGroup.AddLogEvent(true, mEventPool);
                for (const auto& it : *sharedEvent) {
                    logEvent->SetContentNoCopy(it.first, it.second);
                }
                auto* processEvent = static_cast<ProcessEvent*>(innerEvent.get());
                if (processEvent == nullptr) {
                    LOG_WARNING(sLogger,
                                ("failed to convert innerEvent to processEvent",
                                 magic_enum::enum_name(innerEvent->GetKernelEventType())));
                    continue;
                }
                struct timespec ts = ConvertKernelTimeToUnixTime(processEvent->mTimestamp);
                logEvent->SetTimestamp(ts.tv_sec, ts.tv_nsec);
                switch (innerEvent->mEventType) {
                    case KernelEventType::PROCESS_EXECVE_EVENT: {
                        logEvent->SetContentNoCopy(kCallName.LogKey(), ProcessSecurityManager::kExecveValue);
                        // ? kprobe or execve
                        logEvent->SetContentNoCopy(kEventType.LogKey(), ProcessSecurityManager::kKprobeValue);
                        break;
                    }
                    case KernelEventType::PROCESS_EXIT_EVENT: {
                        CommonEvent* ce = innerEvent.get();
                        auto* exitEvent = static_cast<ProcessExitEvent*>(ce);
                        logEvent->SetContentNoCopy(kCallName.LogKey(), StringView(ProcessSecurityManager::kExitValue));
                        logEvent->SetContentNoCopy(kEventType.LogKey(), StringView(AbstractManager::kKprobeValue));
                        auto exitCode = ToStringBuffer(eventGroup.GetSourceBuffer(), exitEvent->mExitCode);
                        auto exitTid = ToStringBuffer(eventGroup.GetSourceBuffer(), exitEvent->mExitTid);
                        logEvent->SetContentNoCopy(ProcessSecurityManager::kExitCodeKey,
                                                   StringView(exitCode.data, exitCode.size));
                        logEvent->SetContentNoCopy(ProcessSecurityManager::kExitTidKey,
                                                   StringView(exitTid.data, exitTid.size));
                        break;
                    }
                    case KernelEventType::PROCESS_CLONE_EVENT: {
                        logEvent->SetContentNoCopy(kCallName.LogKey(), ProcessSecurityManager::kCloneValue);
                        logEvent->SetContentNoCopy(kEventType.LogKey(), ProcessSecurityManager::kKprobeValue);
                        break;
                    }
                    default:
                        break;
                }
            }
        });
    }
    {
        if (this->mPipelineCtx == nullptr) {
            return 0;
        }
        LOG_DEBUG(sLogger, ("event group size", eventGroup.GetEvents().size()));
        size_t eventCount = eventGroup.GetEvents().size();
        ADD_COUNTER(mPushLogsTotal, eventCount);
        ADD_COUNTER(mPushLogGroupTotal, 1);
        std::unique_ptr<ProcessQueueItem> item
            = std::make_unique<ProcessQueueItem>(std::move(eventGroup), this->mPluginIndex);
        if (QueueStatus::OK != ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
            LOG_WARNING(sLogger,
                        ("configName", mPipelineCtx->GetConfigName())("pluginIdx", this->mPluginIndex)(
                            "[ProcessSecurityEvent] push queue failed!", ""));
            ADD_COUNTER(mPushLogFailedTotal, eventCount);
        }
    }

    return 0;
}
} // namespace logtail::ebpf

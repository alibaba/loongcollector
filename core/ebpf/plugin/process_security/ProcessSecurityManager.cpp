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

#include "ProcessSecurityManager.h"

#include <coolbpf/security/type.h>

#include <memory>
#include <mutex>
#include <utility>

#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/queue/ProcessQueueItem.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/magic_enum.hpp"
#include "common/queue/blockingconcurrentqueue.h"
#include "common/timer/Timer.h"
#include "ebpf/Config.h"
#include "ebpf/eBPFServer.h"
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/plugin/ProcessCacheManager.h"
#include "ebpf/type/PeriodicalEvent.h"
#include "ebpf/type/table/BaseElements.h"

namespace logtail {
namespace ebpf {

class eBPFServer;

const std::string ProcessSecurityManager::kExitTidKey = "exit_tid";
const std::string ProcessSecurityManager::kExitCodeKey = "exit_code";
const std::string ProcessSecurityManager::kExecveValue = "value";
const std::string ProcessSecurityManager::kCloneValue = "clone";
const std::string ProcessSecurityManager::kExitValue = "exit";

ProcessSecurityManager::ProcessSecurityManager(std::shared_ptr<ProcessCacheManager>& baseMgr,
                                               std::shared_ptr<SourceManager> sourceManager,
                                               moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                                               PluginMetricManagerPtr mgr)
    : AbstractManager(baseMgr, std::move(sourceManager), queue, std::move(mgr)),
      mAggregateTree(
          4096,
          [](std::unique_ptr<ProcessEventGroup>& base, const std::shared_ptr<CommonEvent>& other) {
              base->mInnerEvents.emplace_back(other);
          },
          [](const std::shared_ptr<CommonEvent>& in) {
              return std::make_unique<ProcessEventGroup>(in->mPid, in->mKtime);
          }) {
}

bool ProcessSecurityManager::ConsumeAggregateTree(
    [[maybe_unused]] const std::chrono::steady_clock::time_point& execTime) {
    if (!mFlag || mSuspendFlag) {
        return false;
    }

    WriteLock lk(mLock);
    SIZETAggTree<ProcessEventGroup, std::shared_ptr<CommonEvent>> aggTree = this->mAggregateTree.GetAndReset();
    lk.unlock();

    // read aggregator
    auto nodes = aggTree.GetNodesWithAggDepth(1);
    LOG_DEBUG(sLogger, ("enter aggregator ...", nodes.size()));
    if (nodes.empty()) {
        LOG_DEBUG(sLogger, ("empty nodes...", ""));
        return true;
    }

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    for (auto& node : nodes) {
        LOG_DEBUG(sLogger, ("child num", node->child.size()));
        // convert to a item and push to process queue
        aggTree.ForEach(node, [&](const ProcessEventGroup* group) {
            SizedMap processTags;
            // represent a process ...
            auto processCacheMgr = GetProcessCacheManager();
            if (processCacheMgr == nullptr) {
                LOG_WARNING(sLogger, ("ProcessCacheManager is null", ""));
                return;
            }
            processTags = processCacheMgr->FinalizeProcessTags(sourceBuffer, group->mPid, group->mKtime);
            if (processTags.mInner.empty()) {
                LOG_WARNING(sLogger, ("cannot find tags for pid", group->mPid)("ktime", group->mKtime));
                return;
            }
            for (const auto& innerEvent : group->mInnerEvents) {
                auto* logEvent = eventGroup.AddLogEvent();
                for (auto& it : processTags.mInner) {
                    logEvent->SetContentNoCopy(it.first, it.second);
                }
                auto ts = innerEvent->mTimestamp + this->mTimeDiff.count();
                auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::nanoseconds(ts));
                logEvent->SetTimestamp(seconds.count(), ts);
                switch (innerEvent->mEventType) {
                    case KernelEventType::PROCESS_EXECVE_EVENT: {
                        logEvent->SetContentNoCopy(kCallName.LogKey(),
                                                   StringView(ProcessSecurityManager::kExecveValue));
                        // ? kprobe or execve
                        logEvent->SetContentNoCopy(kEventType.LogKey(),
                                                   StringView(ProcessSecurityManager::sKprobeValue));
                        break;
                    }
                    case KernelEventType::PROCESS_EXIT_EVENT: {
                        CommonEvent* ce = innerEvent.get();
                        auto* exitEvent = static_cast<ProcessExitEvent*>(ce);
                        logEvent->SetContentNoCopy(kCallName.LogKey(), StringView(ProcessSecurityManager::kExitValue));
                        logEvent->SetContentNoCopy(kEventType.LogKey(), StringView(AbstractManager::sKprobeValue));
                        logEvent->SetContent(ProcessSecurityManager::kExitCodeKey,
                                             std::to_string(exitEvent->mExitCode));
                        logEvent->SetContent(ProcessSecurityManager::kExitTidKey, std::to_string(exitEvent->mExitTid));
                        break;
                    }
                    case KernelEventType::PROCESS_CLONE_EVENT: {
                        logEvent->SetContentNoCopy(kCallName.LogKey(), StringView(ProcessSecurityManager::kCloneValue));
                        logEvent->SetContentNoCopy(kEventType.LogKey(),
                                                   StringView(ProcessSecurityManager::sKprobeValue));
                        break;
                    }
                    default:
                        break;
                }
            }
        });
    }
    {
        std::lock_guard lk(mContextMutex);
        if (this->mPipelineCtx == nullptr) {
            return true;
        }
        LOG_DEBUG(sLogger, ("event group size", eventGroup.GetEvents().size()));
        ADD_COUNTER(mPushLogsTotal, eventGroup.GetEvents().size());
        ADD_COUNTER(mPushLogGroupTotal, 1);
        std::unique_ptr<ProcessQueueItem> item
            = std::make_unique<ProcessQueueItem>(std::move(eventGroup), this->mPluginIndex);
        if (QueueStatus::OK != ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
            LOG_WARNING(sLogger,
                        ("configName", mPipelineCtx->GetConfigName())("pluginIdx", this->mPluginIndex)(
                            "[ProcessSecurityEvent] push queue failed!", ""));
        }
    }

    return true;
}

int ProcessSecurityManager::Init(
    [[maybe_unused]] const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*>& options) {
    // just set timer ...
    // register base manager ...
    mFlag = true;
    mSuspendFlag = false;

    auto processCacheMgr = GetProcessCacheManager();
    if (processCacheMgr == nullptr) {
        LOG_WARNING(sLogger, ("ProcessCacheManager is null", ""));
        return 1;
    }

    processCacheMgr->MarkProcessEventFlushStatus(true);

    std::shared_ptr<AbstractManager> managerPtr
        = eBPFServer::GetInstance()->GetPluginManager(PluginType::PROCESS_SECURITY);

    std::unique_ptr<AggregateEvent> event = std::make_unique<AggregateEvent>(
        2,
        [managerPtr](const std::chrono::steady_clock::time_point& execTime) { // handler
            ProcessSecurityManager* mgr = static_cast<ProcessSecurityManager*>(managerPtr.get());
            return mgr->ConsumeAggregateTree(execTime);
        },
        [managerPtr]() { // stop checker
            if (!managerPtr->IsExists()) {
                LOG_INFO(sLogger, ("plugin not exists", "stop schedule"));
                return true;
            }
            return false;
        });

    Timer::GetInstance()->PushEvent(std::move(event));

    return 0;
}

int ProcessSecurityManager::Destroy() {
    mFlag = false;
    auto processCacheMgr = GetProcessCacheManager();
    if (processCacheMgr == nullptr) {
        LOG_WARNING(sLogger, ("ProcessCacheManager is null", ""));
        return 1;
    }
    processCacheMgr->MarkProcessEventFlushStatus(false);
    return 0;
}

std::array<size_t, 1> GenerateAggKeyForProcessEvent(const std::shared_ptr<CommonEvent>& event) {
    // calculate agg key
    std::array<size_t, 1> hashResult{};
    std::hash<uint64_t> hasher;

    std::array<uint64_t, 2> arr = {uint64_t(event->mPid), event->mKtime};
    for (uint64_t x : arr) {
        hashResult[0] ^= hasher(x) + 0x9e3779b9 + (hashResult[0] << 6) + (hashResult[0] >> 2);
    }
    return hashResult;
}

int ProcessSecurityManager::HandleEvent(const std::shared_ptr<CommonEvent>& event) {
    if (!event) {
        return 1;
    }
    auto* processEvent = static_cast<ProcessEvent*>(event.get());
    LOG_DEBUG(sLogger,
              ("receive event, pid", event->mPid)("ktime", event->mKtime)("eventType",
                                                                          magic_enum::enum_name(event->mEventType)));
    if (processEvent == nullptr) {
        LOG_ERROR(sLogger,
                  ("failed to convert CommonEvent to ProcessEvent, kernel event type",
                   magic_enum::enum_name(event->GetKernelEventType()))("PluginType",
                                                                       magic_enum::enum_name(event->GetPluginType())));
        return 1;
    }

    // calculate agg key
    std::array<size_t, 1> hashResult = GenerateAggKeyForProcessEvent(event);
    {
        WriteLock lk(mLock);
        bool ret = mAggregateTree.Aggregate(event, hashResult);
        LOG_DEBUG(sLogger, ("after aggregate", ret));
    }

    return 0;
}

} // namespace ebpf
} // namespace logtail

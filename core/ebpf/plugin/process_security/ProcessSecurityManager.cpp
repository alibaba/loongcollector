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

#include <memory>
#include <mutex>
#include <thread>

#include "ebpf/driver/coolbpf/src/security/type.h"
#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/Config.h"
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/plugin/BaseManager.h"
#include "ebpf/type/NetworkObserverEvent.h"
#include "ProcessSecurityManager.h"
#include "common/magic_enum.hpp"
#include "ebpf/type/PeriodicalEvent.h"
#include "pipeline/PipelineContext.h"
#include "pipeline/queue/ProcessQueueItem.h"
#include "pipeline/queue/ProcessQueueManager.h"

namespace logtail {
namespace ebpf {

int ProcessSecurityManager::Init(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*>) {
    // just set timer ...
    // register base manager ...
    mFlag = true;
    mSuspendFlag = false;

    mBaseManager->MarkProcessEventFlushStatus(true);

    mAggregateTree = std::make_unique<SIZETAggTree<ProcessEventGroup, std::shared_ptr<ProcessEvent>>> (
        4096, 
        [this](std::unique_ptr<ProcessEventGroup> &base, const std::shared_ptr<ProcessEvent>& other) {
            base->mInnerEvents.emplace_back(std::move(other));
        }, 
        [this](const std::shared_ptr<ProcessEvent>& in) {
            // generate key
            // auto execId = this->mBaseManager->GenerateExecId(in->mPid, in->mKtime);
            
            return std::make_unique<ProcessEventGroup>(in->mPid, in->mKtime);
        }
    );

    mSafeAggregateTree = std::make_unique<SIZETAggTree<ProcessEventGroup, std::shared_ptr<ProcessEvent>>> (
        4096, 
        [this](std::unique_ptr<ProcessEventGroup> &base, const std::shared_ptr<ProcessEvent>& other) {
            base->mInnerEvents.emplace_back(std::move(other));
        }, 
        [this](const std::shared_ptr<ProcessEvent>& in) {
            // generate key
            // auto execId = this->mBaseManager->GenerateExecId(in->mPid, in->mKtime);
            
            return std::make_unique<ProcessEventGroup>(in->mPid, in->mKtime);
        }
    );

    std::unique_ptr<AggregateEvent> event = std::make_unique<AggregateEvent>(2, 
        [this](const std::chrono::steady_clock::time_point& execTime){ // handler
            if (!this->mFlag) {
                return false;
            }

            {
                WriteLock lk(this->mLock);
                std::swap(this->mSafeAggregateTree, this->mAggregateTree);
            }
            // this->mVec.push_back(1);
            // read aggregator
            auto nodes = this->mSafeAggregateTree->GetNodesWithAggDepth(1);
            LOG_DEBUG(sLogger, ("enter aggregator ...", nodes.size()));
            if (nodes.empty()) {
                LOG_DEBUG(sLogger, ("empty nodes...", ""));
                return true;
            }

            auto sourceBuffer = std::make_shared<SourceBuffer>();

            for (auto& node : nodes) {
                // convert to a item and push to process queue
                this->mSafeAggregateTree->ForEach(node, [&](const ProcessEventGroup* group) {
                    PipelineEventGroup eventGroup(std::make_shared<SourceBuffer>());
                    // represent a process ...
                    bool ok = this->mBaseManager->FinalizeProcessTags(eventGroup, group->mPid, group->mKtime);
                    if (!ok) {
                        return;
                    }
                    for (auto innerEvent : group->mInnerEvents) {
                        auto* logEvent = eventGroup.AddLogEvent();
                        auto ts = innerEvent->mTimestamp + this->mTimeDiff.count();
                        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::nanoseconds(ts));
                        logEvent->SetTimestamp(seconds.count(), ts);
                        switch (innerEvent->mEventType)
                        {
                        case KernelEventType::PROCESS_EXECVE_EVENT:{
                            logEvent->SetContent("call_name", std::string("execve"));
                            logEvent->SetContent("event_type", std::string("execve"));
                            break;
                        }
                        case KernelEventType::PROCESS_EXIT_EVENT:{
                            auto exitEvent = std::dynamic_pointer_cast<ProcessExitEvent>(innerEvent);
                            if (exitEvent == nullptr) {
                                LOG_ERROR(sLogger, ("cast to ProcessExitEvent faield", ""));
                                continue;
                            }
                            logEvent->SetContent("call_name", std::string("exit"));
                            logEvent->SetContent("event_type", std::string("kprobe"));
                            logEvent->SetContent("exit_code", std::to_string(exitEvent->mExitCode));
                            logEvent->SetContent("exit_tid", std::to_string(exitEvent->mExitTid));
                            break;
                        }
                        case KernelEventType::PROCESS_CLONE_EVENT:{
                            logEvent->SetContent("call_name", std::string("clone"));
                            logEvent->SetContent("event_type", std::string("kprobe"));
                            break;
                        }
                        default:
                            break;
                        }
                    }
                    
                    {
                        std::lock_guard lk(mContextMutex);
                        std::unique_ptr<ProcessQueueItem> item = std::make_unique<ProcessQueueItem>(std::move(eventGroup), this->mPluginIndex);
                        if (ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
                            LOG_WARNING(sLogger, 
                                ("configName", mPipelineCtx->GetConfigName())
                                ("pluginIdx", this->mPluginIndex)
                                ("[ProcessSecurityEvent] push queue failed!", ""));
                        }
                    }
                });
            }
            this->mSafeAggregateTree->Clear();
            
            return true;
        }, [this]() { // validator
            return !this->mFlag.load();
        }
    );

    mScheduler->PushEvent(std::move(event));

    return 0;
}

int ProcessSecurityManager::Destroy() {
    mBaseManager->MarkProcessEventFlushStatus(false);
    return 0;
}

std::array<size_t, 1> GenerateAggKey(const std::shared_ptr<ProcessEvent> event) {
    // calculate agg key
    std::array<size_t, 1> hash_result;
    hash_result.fill(0UL);
    std::hash<uint64_t> hasher;

    std::array<uint64_t, 2> arr = {uint64_t(event->mPid), event->mKtime};
    for (uint64_t x : arr) {
        hash_result[0] ^= hasher(x) +
                                0x9e3779b9 +
                                (hash_result[0] << 6) +
                                (hash_result[0] >> 2);
    }
    return hash_result;
}

int ProcessSecurityManager::HandleEvent(const std::shared_ptr<CommonEvent> event) {
    auto processEvent = std::dynamic_pointer_cast<ProcessEvent>(event);
    LOG_DEBUG(sLogger, ("receive event, pid", event->mPid) ("ktime", event->mKtime) ("eventType", magic_enum::enum_name(event->mEventType)));
    if (processEvent == nullptr) {
        LOG_ERROR(sLogger, ("failed to convert CommonEvent to ProcessEvent, kernel event type", 
            magic_enum::enum_name(event->GetKernelEventType()))
            ("PluginType", magic_enum::enum_name(event->GetPluginType()))
        );
        return 1;
    }
    
    // calculate agg key
    std::array<size_t, 1> hash_result = GenerateAggKey(processEvent);
    bool ret = mAggregateTree->Aggregate(processEvent, hash_result);
    LOG_DEBUG(sLogger, ("after aggregate", ret));

    return 0;
}

}
}
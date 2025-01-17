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

#include "NetworkSecurityManager.h"
#include "logger/Logger.h"
#include "common/MachineInfoUtil.h"
#include "models/PipelineEventGroup.h"
#include "common/magic_enum.hpp"
#include "ebpf/type/PeriodicalEvent.h"
#include "pipeline/PipelineContext.h"
#include "pipeline/queue/ProcessQueueItem.h"
#include "pipeline/queue/ProcessQueueManager.h"

namespace logtail {
namespace ebpf {

NetworkSecurityManager::NetworkSecurityManager(std::shared_ptr<BaseManager>& base,
                                             std::shared_ptr<SourceManager> sourceManager, moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue, std::shared_ptr<Timer> scheduler)
    : AbstractManager(base, sourceManager, queue, scheduler) {
}

int NetworkSecurityManager::Init(const std::variant<SecurityOptions*, ObserverNetworkOption*> options) {
    auto securityOpts = std::get_if<SecurityOptions*>(&options);
    if (!securityOpts) {
        LOG_ERROR(sLogger, ("Invalid options type for NetworkSecurityManager", ""));
        return -1;
    }

    mFlag = true;

    mAggregateTree = std::make_unique<SIZETAggTree<NetworkEventGroup, std::shared_ptr<NetworkEvent>>> (
        4096, 
        [this](std::unique_ptr<NetworkEventGroup> &base, const std::shared_ptr<NetworkEvent>& other) {
            base->mInnerEvents.emplace_back(std::move(other));
        }, 
        [this](const std::shared_ptr<NetworkEvent>& in) {
            return std::make_unique<NetworkEventGroup>(in->mPid, in->mKtime, in->mProtocol, in->mFamily, in->mSaddr, in->mDaddr, in->mSport, in->mDport, in->mNetns);
        }
    );

    std::unique_ptr<AggregateEvent> event = std::make_unique<AggregateEvent>(2, 
        [this](const std::chrono::steady_clock::time_point& execTime){ // handler
            if (!this->mFlag) {
                return false;
            }
            auto nodes = this->mAggregateTree->GetNodesWithAggDepth(1);
            LOG_DEBUG(sLogger, ("enter aggregator ...", nodes.size()));
            if (nodes.empty()) {
                LOG_DEBUG(sLogger, ("empty nodes...", ""));
                return true;
            }
            // do we need to aggregate all the events into a eventgroup??
            // use source buffer to hold the memory
            PipelineEventGroup eventGroup(std::make_shared<SourceBuffer>());
            for (auto& node : nodes) {
                // convert to a item and push to process queue
                this->mAggregateTree->ForEach(node, [&](const NetworkEventGroup* group) {
                    // set process tag
                    bool ok = this->mBaseManager->FinalizeProcessTags(eventGroup, group->mPid, group->mKtime);
                    if (!ok) {
                        return;
                    }

                    // set network tag
                    eventGroup.SetTag("protocol", std::to_string(group->mProtocol));
                    eventGroup.SetTag("family", std::to_string(group->mFamily));
                    eventGroup.SetTag("saddr", std::to_string(group->mSaddr));
                    eventGroup.SetTag("daddr", std::to_string(group->mDaddr));
                    eventGroup.SetTag("sport", std::to_string(group->mSport));
                    eventGroup.SetTag("dport", std::to_string(group->mDport));
                    eventGroup.SetTag("netns", std::to_string(group->mNetns));

                    for (auto innerEvent : group->mInnerEvents) {
                        auto* logEvent = eventGroup.AddLogEvent();
                        auto ts = innerEvent->mTimestamp + this->mTimeDiff.count();
                        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::nanoseconds(ts));
                        // set timestamp
                        logEvent->SetTimestamp(seconds.count(), ts);

                        // set callnames 
                        switch (innerEvent->mEventType)
                        {
                        case KernelEventType::TCP_SENDMSG_EVENT:{
                            logEvent->SetContent("call_name", std::string("tcp_sendmsg"));
                            logEvent->SetContent("event_type", std::string("kprobe"));
                            break;
                        }
                        case KernelEventType::TCP_CONNECT_EVENT:{
                            logEvent->SetContent("call_name", std::string("tcp_connect"));
                            logEvent->SetContent("event_type", std::string("kprobe"));
                            break;
                        }
                        case KernelEventType::TCP_CLOSE_EVENT:{
                            logEvent->SetContent("call_name", std::string("tcp_close"));
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
                                ("[NetworkSecurityEvent] push queue failed!", ""));
                        }
                    }
                });
            }
            this->mAggregateTree->Clear();
            
            return true;
        }, [this]() { // validator
            return !this->mFlag.load();
        }
    );
    mScheduler->PushEvent(std::move(event));

    std::unique_ptr<PluginConfig> pc = std::make_unique<PluginConfig>();
    pc->mPluginType = PluginType::NETWORK_SECURITY;
    // set configs
    NetworkSecurityConfig config;
    pc->mConfig = std::move(config);

    return mSourceManager->StartPlugin(PluginType::NETWORK_SECURITY, std::move(pc)) ? 0 : 1;
}

int NetworkSecurityManager::Destroy() {
    return mSourceManager->StopPlugin(PluginType::NETWORK_SECURITY) ? 0 : 1;
}

int NetworkSecurityManager::HandleEvent(const std::shared_ptr<CommonEvent> event) {
    return 0;
}

// TODO perf worker functions ...

} // namespace ebpf
} // namespace logtail

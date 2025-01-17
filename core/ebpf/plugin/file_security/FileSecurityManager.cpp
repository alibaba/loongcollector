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

#include "FileSecurityManager.h"

#include "ebpf/Config.h"
#include "logger/Logger.h"
#include "common/magic_enum.hpp"
#include "ebpf/type/PeriodicalEvent.h"
#include "pipeline/PipelineContext.h"
#include "pipeline/queue/ProcessQueueItem.h"
#include "pipeline/queue/ProcessQueueManager.h"

namespace logtail {
namespace ebpf {

int FileSecurityManager::Init(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*> options) {
    // set init flag ...
    mFlag = true;


    mAggregateTree = std::make_unique<SIZETAggTree<FileEventGroup, std::shared_ptr<FileEvent>>> (
        4096, 
        [this](std::unique_ptr<FileEventGroup> &base, const std::shared_ptr<FileEvent>& other) {
            base->mInnerEvents.emplace_back(std::move(other));
        }, 
        [this](const std::shared_ptr<FileEvent>& in) {
            return std::make_unique<FileEventGroup>(in->mPid, in->mKtime, in->mPath);
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

            PipelineEventGroup eventGroup(std::make_shared<SourceBuffer>());
            for (auto& node : nodes) {
                // convert to a item and push to process queue
                this->mAggregateTree->ForEach(node, [&](const FileEventGroup* group) {
                    // set process tag
                    bool ok = this->mBaseManager->FinalizeProcessTags(eventGroup, group->mPid, group->mKtime);
                    if (!ok) {
                        return;
                    }

                    // set file tag
                    eventGroup.SetTag("path", group->mPath);

                    for (auto innerEvent : group->mInnerEvents) {
                        auto* logEvent = eventGroup.AddLogEvent();
                        auto ts = innerEvent->mTimestamp + this->mTimeDiff.count();
                        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::nanoseconds(ts));
                        // set timestamp
                        logEvent->SetTimestamp(seconds.count(), ts);
                        // set callnames 
                        switch (innerEvent->mEventType)
                        {
                        case KernelEventType::FILE_PATH_TRUNCATE:{
                            logEvent->SetContent("call_name", std::string("security_path_truncate"));
                            logEvent->SetContent("event_type", std::string("kprobe"));
                            break;
                        }
                        case KernelEventType::FILE_MMAP:{
                            logEvent->SetContent("call_name", std::string("security_mmap_file"));
                            logEvent->SetContent("event_type", std::string("kprobe"));
                            break;
                        }
                        case KernelEventType::FILE_PERMISSION_EVENT:{
                            logEvent->SetContent("call_name", std::string("security_file_permission"));
                            logEvent->SetContent("event_type", std::string("kprobe"));
                            break;
                        }
                        default:
                            break;
                        }
                    }
                    // TODO @qianlu.kk add lock
                    std::unique_ptr<ProcessQueueItem> item = std::make_unique<ProcessQueueItem>(std::move(eventGroup), this->mPluginIndex);
                    if (ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
                        LOG_WARNING(sLogger, 
                            ("configName", mPipelineCtx->GetConfigName())
                            ("pluginIdx", this->mPluginIndex)
                            ("[FileSecurityEvent] push queue failed!", ""));
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
    pc->mPluginType = PluginType::FILE_SECURITY;
    // set configs

    FileSecurityConfig config;
    pc->mConfig = std::move(config);

    return mSourceManager->StartPlugin(PluginType::FILE_SECURITY, std::move(pc)) ? 0 : 1;

}

int FileSecurityManager::HandleEvent(const std::shared_ptr<CommonEvent> event) {
    return 0;
}

int FileSecurityManager::Destroy() {
    return mSourceManager->StopPlugin(PluginType::FILE_SECURITY) ? 0 : 1;
}

} // namespace ebpf
} // namespace logtail

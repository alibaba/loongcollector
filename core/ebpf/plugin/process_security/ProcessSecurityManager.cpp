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

namespace logtail {
namespace ebpf {

int ProcessSecurityManager::Init(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*>) {
    // just set timer ...
    // register base manager ...
    mFlag = true;
    mSuspendFlag = false;

    mBaseManager->MarkProcessEventFlushStatus(true);

    mAggregateTree = std::make_unique<SIZETAggTree<ProcessEventGroup, std::shared_ptr<ProcessEvent>>> (
        10, 
        [this](std::unique_ptr<ProcessEventGroup> &base, const std::shared_ptr<ProcessEvent>& other) {
            // TODO
            // gen attrs ... 
            switch (other->GetKernelEventType())
            {
            case KernelEventType::PROCESS_EXECVE_EVENT:
                
                break;
            case KernelEventType::PROCESS_CLONE_EVENT:

                break;
            case KernelEventType::PROCESS_EXIT_EVENT:

                break;
            default:
                LOG_ERROR(sLogger, ("kernel event type not supported", magic_enum::enum_name(other->GetKernelEventType())));
                break;
            }
            base->mInnerEvents.emplace_back(other->mTimestamp);
        }, 
        [this](const std::shared_ptr<ProcessEvent>& in) {
            // generate key
            // auto execId = this->mBaseManager->GenerateExecId(in->mPid, in->mKtime);
            
            return std::make_unique<ProcessEventGroup>(in->mPid, in->mKtime);
        }
    );

    return 0;
}

int ProcessSecurityManager::Destroy() {
    mBaseManager->MarkProcessEventFlushStatus(false);
    return 0;
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
    std::array<size_t, 1> hash_result;
    hash_result.fill(0UL);
    std::hash<uint64_t> hasher;
    
    hash_result[0] ^= hasher(event->mPid) +
                                            0x9e3779b9 +
                                            (hash_result[0] << 6) +
                                            (hash_result[0] >> 2);
    hash_result[0] ^= hasher(event->mKtime) +
                                            0x9e3779b9 +
                                            (hash_result[0] << 6) +
                                            (hash_result[0] >> 2);
    mAggregateTree->Aggregate(processEvent, hash_result);

    return 0;
}

}
}
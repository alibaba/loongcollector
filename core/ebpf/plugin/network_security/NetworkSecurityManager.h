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

#pragma once

#include "ebpf/plugin/AbstractManager.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "common/timer/Timer.h"
#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/type/NetworkEvent.h"


namespace logtail {
namespace ebpf {

class NetworkSecurityManager : public AbstractManager {
public:
    NetworkSecurityManager(std::shared_ptr<BaseManager>& base,
                          std::shared_ptr<SourceManager> sourceManager, moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue, std::shared_ptr<Timer> scheduler);
    ~NetworkSecurityManager() override {}

    int Init(const std::variant<SecurityOptions*, ObserverNetworkOption*> options) override;
    int Destroy() override;
    
    PluginType GetPluginType() override { return PluginType::NETWORK_SECURITY; }

    virtual int HandleEvent(const std::shared_ptr<CommonEvent> event) override;

private:
    
    std::unique_ptr<SIZETAggTree<NetworkEventGroup, std::shared_ptr<NetworkEvent>>> mAggregateTree;
};

} // namespace ebpf
} // namespace logtail

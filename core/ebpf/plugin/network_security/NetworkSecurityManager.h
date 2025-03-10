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

#include <coolbpf/security/type.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "common/queue/blockingconcurrentqueue.h"
#include "common/timer/Timer.h"
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/type/NetworkEvent.h"


namespace logtail {
namespace ebpf {

class NetworkSecurityManager : public AbstractManager {
public:
    static const std::string kTcpSendMsgValue;
    static const std::string kTcpCloseValue;
    static const std::string kTcpConnectValue;

    NetworkSecurityManager(std::shared_ptr<ProcessCacheManager>& base,
                           std::shared_ptr<SourceManager> sourceManager,
                           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                           PluginMetricManagerPtr mgr);
    ~NetworkSecurityManager() override {}

    static std::shared_ptr<NetworkSecurityManager>
    Create(std::shared_ptr<ProcessCacheManager>& mgr,
           std::shared_ptr<SourceManager> sourceManager,
           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
           PluginMetricManagerPtr metricMgr) {
        return std::make_shared<NetworkSecurityManager>(mgr, sourceManager, queue, metricMgr);
    }

    int Init(const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) override;
    int Destroy() override;

    void RecordNetworkEvent(tcp_data_t* event);

    void UpdateLossKernelEventsTotal(uint64_t cnt);

    bool ConsumeAggregateTree(const std::chrono::steady_clock::time_point& execTime);

    PluginType GetPluginType() override { return PluginType::NETWORK_SECURITY; }

    int HandleEvent(const std::shared_ptr<CommonEvent>& event) override;

    std::unique_ptr<PluginConfig>
    GeneratePluginConfig(const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) override {
        std::unique_ptr<PluginConfig> pc = std::make_unique<PluginConfig>();
        pc->mPluginType = PluginType::NETWORK_SECURITY;
        NetworkSecurityConfig config;
        SecurityOptions* opts = std::get<SecurityOptions*>(options);
        config.mOptions = opts->mOptionList;
        pc->mConfig = std::move(config);
        return pc;
    }

private:
    ReadWriteLock mLock;
    SIZETAggTree<NetworkEventGroup, std::shared_ptr<CommonEvent>> mAggregateTree; // guard by mLock
};

} // namespace ebpf
} // namespace logtail

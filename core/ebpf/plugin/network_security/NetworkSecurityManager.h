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

#pragma once

#include <coolbpf/security/type.h>

#include <utility>

#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/type/NetworkEvent.h"
#include "ebpf/util/AggregateTree.h"

namespace logtail::ebpf {

class NetworkSecurityManager : public AbstractManager {
public:
    static const std::string kTcpSendMsgValue;
    static const std::string kTcpCloseValue;
    static const std::string kTcpConnectValue;

    NetworkSecurityManager(const std::shared_ptr<ProcessCacheManager>& base,
                           const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue);
    ~NetworkSecurityManager() override {}

    static std::shared_ptr<NetworkSecurityManager>
    Create(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
           const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue) {
        return std::make_shared<NetworkSecurityManager>(processCacheManager, eBPFAdapter, queue);
    }

    int Init() override;
    int Destroy() override;

    void RecordNetworkEvent(tcp_data_t* event);

    void UpdateLossKernelEventsTotal(uint64_t cnt);

    PluginType GetPluginType() override { return PluginType::NETWORK_SECURITY; }

    int HandleEvent(const std::shared_ptr<CommonEvent>& event) override;

    int SendEvents() override;

    int RegisteredConfigCount() override { return mRegisteredConfigCount; }

    void SetMetrics(CounterPtr pollEventsTotal, CounterPtr lossEventsTotal) {
        mRecvKernelEventsTotal = std::move(pollEventsTotal);
        mLossKernelEventsTotal = std::move(lossEventsTotal);
    }

    int AddOrUpdateConfig(const CollectionPipelineContext*,
                          uint32_t,
                          const PluginMetricManagerPtr&,
                          const std::variant<SecurityOptions*, ObserverNetworkOption*>&) override;

    int RemoveConfig(const std::string&) override;

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
    int64_t mSendIntervalMs = 2000;
    int64_t mLastSendTimeMs = 0;
    SIZETAggTree<NetworkEventGroup, std::shared_ptr<CommonEvent>> mAggregateTree; // guard by mLock

    std::vector<MetricLabels> mRefAndLabels;
    PluginMetricManagerPtr mMetricMgr;
    std::string mConfigName;

    const CollectionPipelineContext* mPipelineCtx{nullptr};
    logtail::QueueKey mQueueKey = 0;
    uint32_t mPluginIndex{0};

    int mRegisteredConfigCount = 0;
    // plugin metrics, guarded by mContextMutex
    CounterPtr mPushLogsTotal;
    CounterPtr mPushLogGroupTotal;

    // runner metrics
    CounterPtr mRecvKernelEventsTotal;
    CounterPtr mLossKernelEventsTotal;
};

} // namespace logtail::ebpf

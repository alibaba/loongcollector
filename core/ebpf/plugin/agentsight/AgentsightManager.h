// Copyright 2026 iLogtail Authors
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

#include <cstdint>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "agentsight.h"
#include "collection_pipeline/queue/QueueKey.h"
#include "ebpf/EBPFAdapter.h"
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/plugin/agentsight/AgentsightMessageUtil.h"
#include "monitor/metric_models/ReentrantMetricsRecord.h"

namespace logtail::ebpf {

class AgentsightManager : public AbstractManager {
public:
    AgentsightManager() = delete;
    AgentsightManager(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
                      const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                      moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                      EventPool* pool);

    static std::shared_ptr<AgentsightManager>
    Create(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
           const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
           EventPool* pool) {
        return std::make_shared<AgentsightManager>(processCacheManager, eBPFAdapter, queue, pool);
    }

    ~AgentsightManager() override = default;

    int Init() override;
    int Destroy() override;

    PluginType GetPluginType() override { return PluginType::AGENTSIGHT_OBSERVE; }

    int HandleEvent(const std::shared_ptr<CommonEvent>& event) override;
    int SendEvents() override { return 0; }

    int PollPerfBuffer(int maxWaitTimeMs) override;
    int ConsumePerfBufferData() override { return 0; }

    int RegisteredConfigCount() override { return mRegisteredConfigCount; }

    int AddOrUpdateConfig(const CollectionPipelineContext*,
                          uint32_t,
                          const PluginMetricManagerPtr&,
                          const std::variant<SecurityOptions*, ObserverNetworkOption*>&) override;

    int RemoveConfig(const std::string&) override;

    int Suspend() override;

    int OnEpollReadable() override;

    std::unique_ptr<PluginConfig>
    GeneratePluginConfig(const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) override;

    void SetMetrics(CounterPtr lossKernelEventsTotal, CounterPtr pushLogFailedTotal) {
        mLossKernelEventsTotal = std::move(lossKernelEventsTotal);
        mPushLogFailedTotal = std::move(pushLogFailedTotal);
    }

protected:
    int update(const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) override;
    int resume(const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) override;

private:
    static void OnLlmCallback(const AgentsightLLMData* data, void* user_data);

    void StopAgentSightLocked();
    bool RestartAgentSightLocked(const SecurityOptions& opts);
    int DrainReadsLocked();
    void LogAgentSightError(const char* what);
    void releaseMetricRefs();
    void clearSessionInputStateLocked();

    static constexpr size_t kMaxSessionInputStates = 4096;

    std::string mConfigName;
    const CollectionPipelineContext* mPipelineCtx{nullptr};
    logtail::QueueKey mQueueKey{0};
    uint32_t mPluginIndex{0};
    int mRegisteredConfigCount{0};

    // Lib + routing fields; held during handle_read (DrainReadsLocked). Lock order: callers that take
    // EBPFServer's per-plugin mMtx do so before calling in (Enable/Disable/Suspend); the poller takes
    // shared_lock(mMtx) then this mutex. OnLlmCallback must not lock this (runs under handle_read).
    std::mutex mLibMutex;
    std::mutex mSessionInputMutex;
    /// Dedup key (`session_id`, else `turn.id`) -> last `gen_ai.input.messages` length and hash.
    std::unordered_map<std::string, AgentsightSessionInputState> mSessionInputState;

    AgentsightHandle* mHandle = nullptr;
    int mEventFd = -1;
    bool mRunning = false;
    bool mStreamModeFormat = true;
    bool mAutoMessageTrim = true;

    CounterPtr mLossKernelEventsTotal;
    CounterPtr mPushLogFailedTotal;
    CounterPtr mPluginInEventsTotal;
    CounterPtr mPushLogsTotal;
    CounterPtr mPushLogGroupTotal;
    std::vector<MetricLabels> mRefAndLabels;
    PluginMetricManagerPtr mMetricMgr;
};

} // namespace logtail::ebpf

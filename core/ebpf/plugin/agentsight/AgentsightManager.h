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

#include "agentsight.h"
#include "collection_pipeline/queue/QueueKey.h"
#include "common/LRUCache.h"
#include "ebpf/EBPFAdapter.h"
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/plugin/agentsight/AgentsightMessageUtil.h"
#include "monitor/metric_models/ReentrantMetricsRecord.h"

namespace logtail {
class PipelineEventGroup;
struct RawContainerInfo;
} // namespace logtail

namespace logtail::ebpf {

// Defined in AgentsightEvents.h; only used through pointers here.
class AgentsightLlmRecord;
class AgentsightHttpsRecord;

/// Sets the standard container metadata tags on @group from @info — the same keys
/// input_file / input_container_stdio emit (`_container_name_`, `_image_name_`, `_container_ip_`,
/// `_pod_name_`, `_namespace_`, `_pod_uid_` via GetDefaultTagKeyString, plus custom metadatas
/// verbatim). Pure function with no singleton access, so it is unit-testable on its own; the
/// ContainerManager lookup wrapper lives in AgentsightManager.cpp.
void AttachAgentsightContainerTagsFromInfo(PipelineEventGroup& group, const RawContainerInfo& info);

class AgentsightManager : public AbstractManager {
public:
    AgentsightManager() = delete;
    AgentsightManager(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
                      const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                      moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                      EventPool* pool,
                      size_t sessionInputCacheMaxSize = kMaxSessionInputStates);

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
                          const PluginOptions&) override;

    int RemoveConfig(const std::string&) override;

    int Suspend() override;

    int OnEpollReadable() override;

    std::unique_ptr<PluginConfig> GeneratePluginConfig(const PluginOptions& options) override;

    void SetMetrics(CounterPtr lossKernelEventsTotal, CounterPtr pushLogFailedTotal) {
        mLossKernelEventsTotal = std::move(lossKernelEventsTotal);
        mPushLogFailedTotal = std::move(pushLogFailedTotal);
    }

#ifdef APSARA_UNIT_TEST_MAIN
    size_t GetSessionInputCacheSizeForTest() const { return mSessionInputCache.size(); }

    bool SessionInputCacheContainsForTest(const std::string& sessionKey) const {
        return mSessionInputCache.contains(sessionKey);
    }
#endif

protected:
    int update(const PluginOptions& options) override;
    int resume(const PluginOptions& options) override;

private:
    static void OnLlmCallback(const AgentsightLLMData* data, void* user_data);
    /// Raw HTTP fallback for traffic AgentSight could not parse as an LLM call. Only registered with
    /// handle_read when mRawHttpsFallback is on; otherwise the Rust side never emits these events.
    static void OnHttpsCallback(const AgentsightHttpsData* data, void* user_data);

    int HandleLlmEvent(AgentsightLlmRecord* rec);
    int HandleHttpsEvent(const AgentsightHttpsRecord* rec);

    void StopAgentSightLocked();
    bool RestartAgentSightLocked(const SecurityOptions& opts);
    int DrainReadsLocked();
    void LogAgentSightError(const char* what);
    void releaseMetricRefs();
    void clearSessionInputState();

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
    /// `session_id` (or `turn.id` fallback) -> delta/dedup and per-turn step/sequence counters.
    lru11::Cache<std::string, AgentsightSessionInputState, std::mutex> mSessionInputCache;

    AgentsightHandle* mHandle = nullptr;
    int mEventFd = -1;
    bool mRunning = false;
    bool mEventStreamFormat = true;
    bool mMessageDeltaOnly = true;
    bool mRawHttpsFallback = false;

    /// Runner-level aggregates owned by EBPFServer and shared with the other eBPF plugins, so they
    /// stay unlabelled here — narrowing them would change a metric four managers report into.
    CounterPtr mLossKernelEventsTotal;
    CounterPtr mPushLogFailedTotal;

    /// Per-stream counters, one set per `record_type` label value.
    ///
    /// AgentSight produces two streams of wildly different volume: once a process is attached, *every*
    /// non-LLM HTTPS exchange it makes becomes a raw HTTP event, while gen_ai events are one per LLM
    /// call. Sharing one counter set made it impossible to tell whether a jump came from enabling
    /// RawHttpsFallback or from real LLM traffic growth, to size capacity for turning the switch on,
    /// or — when events are dropped — to tell which stream filled the shared mCommonEventQueue.
    struct StreamMetrics {
        CounterPtr inEventsTotal;
        CounterPtr pushLogsTotal;
        CounterPtr pushLogGroupTotal;
        /// Plugin-level counterpart of mLossKernelEventsTotal: same increments, but attributable.
        CounterPtr lossEventsTotal;

        void reset() {
            inEventsTotal.reset();
            pushLogsTotal.reset();
            pushLogGroupTotal.reset();
            lossEventsTotal.reset();
        }
    };
    StreamMetrics mRawHttpMetrics;
    StreamMetrics mGenAiMetrics;

    std::vector<MetricLabels> mRefAndLabels;
    PluginMetricManagerPtr mMetricMgr;
};

} // namespace logtail::ebpf

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

#include <atomic>
#include <vector>

#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/Config.h"
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/plugin/ProcessCacheManager.h"
#include "ebpf/plugin/RetryableEventCache.h"
#include "ebpf/plugin/network_observer/ConnectionManager.h"
#include "ebpf/type/CommonDataEvent.h"
#include "ebpf/type/NetworkObserverEvent.h"
#include "ebpf/util/AggregateTree.h"
#include "ebpf/util/FrequencyManager.h"
#include "ebpf/util/sampler/Sampler.h"

namespace logtail::ebpf {

enum class JobType {
    METRIC_AGG,
    SPAN_AGG,
    LOG_AGG,
    HOST_META_UPDATE,
};

inline size_t GenerateContainerKey(const std::string& cid) {
    std::hash<std::string> hasher;
    size_t key = 0;
    AttrHashCombine(key, hasher(cid));
    return key;
}

inline size_t
GenerateWorkloadKey(const std::string& ns, const std::string& workloadKind, const std::string& workloadName) {
    std::hash<std::string> hasher;
    size_t res = 0;
    AttrHashCombine(res, hasher(ns));
    AttrHashCombine(res, hasher(workloadKind));
    AttrHashCombine(res, hasher(workloadName));
    return res;
}

class NetworkObserverManager : public AbstractManager {
public:
    static std::shared_ptr<NetworkObserverManager>
    Create(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
           const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue) {
        return std::make_shared<NetworkObserverManager>(processCacheManager, eBPFAdapter, queue);
    }

    NetworkObserverManager() = delete;
    ~NetworkObserverManager() override { LOG_INFO(sLogger, ("begin destruct plugin", "network_observer")); }
    PluginType GetPluginType() override { return PluginType::NETWORK_OBSERVE; }
    NetworkObserverManager(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
                           const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue);

    int Init() override;

    int AddOrUpdateConfig(const CollectionPipelineContext*,
                          uint32_t,
                          const PluginMetricManagerPtr&,
                          const std::variant<SecurityOptions*, ObserverNetworkOption*>&) override;

    int RemoveConfig(const std::string&) override;

    int Destroy() override;

    void UpdateWhitelists(std::vector<std::pair<std::string, uint64_t>>&& enableCids,
                          std::vector<std::string>&& disableCids);

    int HandleEvent([[maybe_unused]] const std::shared_ptr<CommonEvent>& event) override;

    int SendEvents() override;

    int RegisteredConfigCount() override { return mConfigToWorkloads.size(); }
    int PollPerfBuffer(int maxWaitTimeMs) override;
    int ConsumePerfBufferData() override { return 0; }

    void RecordEventLost(enum callback_type_e type, uint64_t lostCount);
    void AcceptNetCtrlEvent(struct conn_ctrl_event_t* event);
    void AcceptNetStatsEvent(struct conn_stats_event_t* event);
    void AcceptDataEvent(struct conn_data_event_t* event);

    std::unique_ptr<PluginConfig> GeneratePluginConfig(
        [[maybe_unused]] const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) override {
        auto ebpfConfig = std::make_unique<PluginConfig>();
        ebpfConfig->mPluginType = PluginType::NETWORK_OBSERVE;
        return ebpfConfig;
    }

    int Update([[maybe_unused]] const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) override {
        return 0;
    }

    int Suspend() override {
        mSuspendFlag = true;
        return 0;
    }

    int Resume(const std::variant<SecurityOptions*, ObserverNetworkOption*>&) override {
        mSuspendFlag = false;
        return 0;
    }

    void SetMetrics(CounterPtr pollEventsTotal, CounterPtr lossEventsTotal, IntGaugePtr connCacheSize) {
        mRecvKernelEventsTotal = std::move(pollEventsTotal);
        mLossKernelEventsTotal = std::move(lossEventsTotal);
        mConnectionNum = std::move(connCacheSize);
    }

    // periodically tasks ...
    bool ConsumeLogAggregateTree();
    bool ConsumeMetricAggregateTree();
    bool ConsumeSpanAggregateTree();
    bool ConsumeNetMetricAggregateTree();
    bool UploadHostMetadataUpdateTask();
    void ReportAgentInfo();

    void HandleHostMetadataUpdate(const std::vector<std::string>& podCidVec);

private:
    // the following 3 methods are not thread safe ...
    std::shared_ptr<AppDetail>
    getWorkloadAppConfig(const std::string& ns, const std::string& workloadKind, const std::string& workloadName);
    std::shared_ptr<AppDetail> getWorkloadAppConfig(size_t workloadKey);
    std::shared_ptr<AppDetail> getContainerAppConfig(size_t containerIdKey);

    // thread safe, can be used in timer thread ...
    std::shared_ptr<AppDetail> getConnAppConfig(const std::shared_ptr<Connection>& conn);

    // only used in poller thread ...
    std::shared_ptr<AppDetail> getAppConfigFromReplica(const std::shared_ptr<Connection>& conn);

    std::array<size_t, 1> generateAggKeyForSpan(L7Record*, const std::shared_ptr<logtail::ebpf::AppDetail>&);
    std::array<size_t, 1> generateAggKeyForLog(L7Record*, const std::shared_ptr<logtail::ebpf::AppDetail>&);
    std::array<size_t, 2> generateAggKeyForAppMetric(L7Record*, const std::shared_ptr<logtail::ebpf::AppDetail>&);
    std::array<size_t, 2> generateAggKeyForNetMetric(ConnStatsRecord*,
                                                     const std::shared_ptr<logtail::ebpf::AppDetail>&);

    void processRecordAsLog(const std::shared_ptr<CommonEvent>& record,
                            const std::shared_ptr<logtail::ebpf::AppDetail>&);
    void processRecordAsSpan(const std::shared_ptr<CommonEvent>& record,
                             const std::shared_ptr<logtail::ebpf::AppDetail>&);
    void processRecordAsMetric(L7Record* record, const std::shared_ptr<logtail::ebpf::AppDetail>&);

    bool updateParsers(const std::vector<std::string>& protocols, const std::vector<std::string>& prevProtocols);

    enum class EventDataType {
        AGENT_INFO,
        APP_METRIC,
        NET_METRIC,
        APP_SPAN,
        LOG,
    };

    void pushEventsWithRetry(EventDataType dataType,
                             PipelineEventGroup&& eventGroup,
                             const StringView& configName,
                             QueueKey queueKey,
                             uint32_t pluginIdx,
                             CounterPtr& eventCounter,
                             CounterPtr& eventGroupCounter,
                             size_t retryTimes = 5);

    std::unique_ptr<ConnectionManager> mConnectionManager; // hold connection cache ...

    mutable std::atomic_long mDataEventsDropTotal = 0;

    mutable std::atomic_int64_t mConntrackerNum = 0;
    mutable std::atomic_int64_t mRecvConnStatEventsTotal = 0;
    mutable std::atomic_int64_t mRecvCtrlEventsTotal = 0;
    mutable std::atomic_int64_t mRecvHttpDataEventsTotal = 0;
    mutable std::atomic_int64_t mLostConnStatEventsTotal = 0;
    mutable std::atomic_int64_t mLostCtrlEventsTotal = 0;
    mutable std::atomic_int64_t mLostDataEventsTotal = 0;

    int mCidOffset = -1;

    // handler thread ...
    SIZETAggTreeWithSourceBuffer<AppMetricData, L7Record*> mAppAggregator;
    SIZETAggTreeWithSourceBuffer<NetMetricData, ConnStatsRecord*> mNetAggregator;
    SIZETAggTree<AppSpanGroup, std::shared_ptr<CommonEvent>> mSpanAggregator;
    SIZETAggTree<AppLogGroup, std::shared_ptr<CommonEvent>> mLogAggregator;

    void updateConfigVersionAndWhitelist(std::vector<std::pair<std::string, uint64_t>>&& newCids,
                                         std::vector<std::string>&& expiredCids) {
        if (!newCids.empty() || !expiredCids.empty()) {
            mConfigVersion++;
            UpdateWhitelists(std::move(newCids), std::move(expiredCids));
        }
    }

    void updateContainerConfigs(size_t workloadKey, const std::shared_ptr<AppDetail>& newConfig) {
        auto it = mWorkloadConfigs.find(workloadKey);
        if (it == mWorkloadConfigs.end()) {
            return;
        }

        for (const auto& cid : it->second.containerIds) {
            size_t cidKey = GenerateContainerKey(cid);
            mContainerConfigs[cidKey] = newConfig;
        }
    }

    struct WorkloadConfig {
        std::shared_ptr<AppDetail> config;
        std::set<std::string> containerIds;
    };

    mutable ReadWriteLock mAppConfigLock;
    std::atomic_int mConfigVersion = 0;
    std::atomic_int mLastConfigVersion = -1;
    std::unordered_map<size_t, WorkloadConfig> mWorkloadConfigs; // workloadKey => {config, containers}
    std::unordered_map<size_t, std::shared_ptr<AppDetail>> mContainerConfigs; // containerKey => config
    std::unordered_map<std::string, std::set<size_t>> mConfigToWorkloads; // configName => workloadKeys

    // replica of mContainerConfigs, only used in poller thread ...
    std::unordered_map<size_t, std::shared_ptr<AppDetail>> mContainerConfigsReplica;

    std::shared_ptr<Sampler> mSampler;

    std::string mClusterId; // inited in Init()
    std::string mHostName; // host
    std::string mHostIp; // host

    RetryableEventCache mRetryableEventCache;
    int64_t mLastUpdateHostMetaTimeMs = INT_MIN;
    int64_t mLastSendSpanTimeMs = INT_MIN;
    int64_t mLastSendMetricTimeMs = INT_MIN;
    int64_t mLastSendLogTimeMs = INT_MIN;
    int64_t mLastSendAgentInfoTimeMs = INT_MIN;

    int64_t mSendSpanIntervalMs = 2000;
    int64_t mSendLogIntervalMs = 2000;
    int64_t mSendMetricIntervalMs = 15000;
    int64_t mSendAgentInfoIntervalMs = 60000;

    // runner metrics
    CounterPtr mRecvKernelEventsTotal;
    CounterPtr mLossKernelEventsTotal;
    IntGaugePtr mConnectionNum;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class NetworkObserverManagerUnittest;
    friend class HttpRetryableEventUnittest;
    friend class NetworkObserverConfigUpdateUnittest;
    std::vector<PipelineEventGroup> mMetricEventGroups;
    std::vector<PipelineEventGroup> mLogEventGroups;
    std::vector<PipelineEventGroup> mSpanEventGroups;

    int mRollbackRecordTotal = 0;
    int mDropRecordTotal = 0;

    std::vector<std::pair<std::string, uint64_t>> mEnableCids;
    std::vector<std::string> mDisableCids;

    std::atomic_int mExecTimes = 0;
#endif
};

} // namespace logtail::ebpf

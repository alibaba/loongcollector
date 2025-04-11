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

#include <atomic>
#include <queue>
#include <vector>

#include "ConnectionManager.h"
#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/Config.h"
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/plugin/ProcessCacheManager.h"
#include "ebpf/type/CommonDataEvent.h"
#include "ebpf/type/NetworkObserverEvent.h"
#include "ebpf/util/FrequencyManager.h"
#include "ebpf/util/sampler/Sampler.h"

namespace logtail {
namespace ebpf {

class NetworkObserverManager : public AbstractManager {
public:
    static std::shared_ptr<NetworkObserverManager>
    Create(std::shared_ptr<ProcessCacheManager>& mgr,
           std::shared_ptr<SourceManager> sourceManager,
           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
           PluginMetricManagerPtr metricMgr) {
        return std::make_shared<NetworkObserverManager>(mgr, sourceManager, queue, metricMgr);
    }

    NetworkObserverManager() = delete;
    ~NetworkObserverManager() override { LOG_INFO(sLogger, ("begin destruct plugin", "network_observer")); }
    PluginType GetPluginType() override { return PluginType::NETWORK_OBSERVE; }
    NetworkObserverManager(std::shared_ptr<ProcessCacheManager>& baseMgr,
                           std::shared_ptr<SourceManager> sourceManager,
                           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                           PluginMetricManagerPtr mgr);

    int Init(const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) override;

    int Destroy() override;

    void UpdateWhitelists(std::vector<std::string>&& enableCids, std::vector<std::string>&& disableCids);

    int HandleEvent([[maybe_unused]] const std::shared_ptr<CommonEvent>& event) override { return 0; }

    int PollPerfBuffer() override { return 0; }

    void RecordEventLost(enum callback_type_e type, uint64_t lost_count);

    void AcceptNetCtrlEvent(struct conn_ctrl_event_t* event);
    void AcceptNetStatsEvent(struct conn_stats_event_t* event);
    void AcceptDataEvent(struct conn_data_event_t* event);

    void PollBufferWrapper();
    void ConsumeRecords();

    std::array<size_t, 1> GenerateAggKeyForSpan(const std::shared_ptr<AbstractRecord>&);
    std::array<size_t, 1> GenerateAggKeyForLog(const std::shared_ptr<AbstractRecord>&);
    std::array<size_t, 2> GenerateAggKeyForAppMetric(const std::shared_ptr<AbstractRecord>&);
    std::array<size_t, 2> GenerateAggKeyForNetMetric(const std::shared_ptr<AbstractRecord>&);

    std::unique_ptr<PluginConfig> GeneratePluginConfig(
        [[maybe_unused]] const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) override {
        auto ebpfConfig = std::make_unique<PluginConfig>();
        ebpfConfig->mPluginType = PluginType::NETWORK_OBSERVE;
        return ebpfConfig;
    }

    virtual int Update([[maybe_unused]] const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) override;

    virtual int Suspend() override {
        mSuspendFlag = true;
        return 0;
    }

    virtual int Resume(const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) override {
        mSuspendFlag = false;
        return 0;
    }

    bool ConsumeLogAggregateTree(const std::chrono::steady_clock::time_point& execTime);
    bool ConsumeMetricAggregateTree(const std::chrono::steady_clock::time_point& execTime);
    bool ConsumeSpanAggregateTree(const std::chrono::steady_clock::time_point& execTime);
    bool ConsumeNetMetricAggregateTree(const std::chrono::steady_clock::time_point& execTime);

    void HandleHostMetadataUpdate(const std::vector<std::string>& podIpVec);

private:
    void ProcessRecord(const std::shared_ptr<AbstractRecord>& record);
    void ProcessRecordAsLog(const std::shared_ptr<AbstractRecord>& record);
    void ProcessRecordAsSpan(const std::shared_ptr<AbstractRecord>& record);
    void ProcessRecordAsMetric(const std::shared_ptr<AbstractRecord>& record);

    void HandleRollback(const std::shared_ptr<AbstractRecord>& record, bool& drop);

    void RunInThread();

    bool UpdateParsers(const std::vector<std::string>& protocols, const std::vector<std::string>& prevProtocols);

    std::unique_ptr<ConnectionManager> mConnectionManager;

    mutable std::atomic_long mDataEventsDropTotal = 0;

    mutable std::atomic_int64_t mConntrackerNum = 0;
    mutable std::atomic_int64_t mRecvConnStatEventsTotal = 0;
    mutable std::atomic_int64_t mRecvCtrlEventsTotal = 0;
    mutable std::atomic_int64_t mRecvHttpDataEventsTotal = 0;
    mutable std::atomic_int64_t mLostConnStatEventsTotal = 0;
    mutable std::atomic_int64_t mLostCtrlEventsTotal = 0;
    mutable std::atomic_int64_t mLostDataEventsTotal = 0;

    // cache relative metric
    IntGaugePtr mConnectionNum;

    // metadata relative metric
    CounterPtr mNetMetaAttachSuccessTotal;
    CounterPtr mNetMetaAttachFailedTotal;
    CounterPtr mNetMetaAttachRollbackTotal;
    CounterPtr mAppMetaAttachSuccessTotal;
    CounterPtr mAppMetaAttachFailedTotal;
    CounterPtr mAppMetaAttachRollbackTotal;

    mutable ReadWriteLock mSamplerLock;
    std::shared_ptr<Sampler> mSampler;

    // store parsed records
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<AbstractRecord>> mRollbackQueue;
    std::deque<std::shared_ptr<AbstractRecord>> mRollbackRecords;

    // coreThread used for polling kernel event...
    std::thread mCoreThread;

    std::thread mRecordConsume;

    std::atomic_bool mEnableSpan;
    std::atomic_bool mEnableLog;
    std::atomic_bool mEnableMetric;

    FrequencyManager mPollKernelFreqMgr;
    FrequencyManager mConsumerFreqMgr;

    std::unique_ptr<ObserverNetworkOption> mPreviousOpt;

    int mCidOffset = -1;
    std::unordered_set<std::string> mEnabledCids;

    ReadWriteLock mAppAggLock;
    SIZETAggTreeWithSourceBuffer<AppMetricData, std::shared_ptr<AbstractRecord>> mAppAggregator;


    ReadWriteLock mNetAggLock;
    SIZETAggTreeWithSourceBuffer<NetMetricData, std::shared_ptr<AbstractRecord>> mNetAggregator;


    ReadWriteLock mSpanAggLock;
    SIZETAggTree<AppSpanGroup, std::shared_ptr<AbstractRecord>> mSpanAggregator;

    ReadWriteLock mLogAggLock;
    SIZETAggTree<AppLogGroup, std::shared_ptr<AbstractRecord>> mLogAggregator;

    std::string mClusterId;
    std::string mAppId;
    std::string mAppName;
    std::string mHostName;
    std::string mHostIp;

    template <typename T, typename Func>
    void CompareAndUpdate(const std::string& fieldName, const T& oldValue, const T& newValue, Func onUpdate) {
        if (oldValue != newValue) {
            LOG_INFO(sLogger, ("config change!, fieldName", fieldName));
            onUpdate(oldValue, newValue);
        }
    }
#ifdef APSARA_UNIT_TEST_MAIN
    friend class NetworkObserverManagerUnittest;
    std::vector<PipelineEventGroup> mMetricEventGroups;
    std::vector<PipelineEventGroup> mLogEventGroups;
    std::vector<PipelineEventGroup> mSpanEventGroups;

    int mRollbackRecordTotal = 0;
    int mDropRecordTotal = 0;

    std::vector<std::string> mEnableCids;
    std::vector<std::string> mDisableCids;
#endif
};

} // namespace ebpf
} // namespace logtail

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
#include <queue>
#include <vector>

#include "ProcParser.h"
#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/Config.h"
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/plugin/ProcessCacheManager.h"
#include "ebpf/plugin/network_observer/ConnectionManager.h"
#include "ebpf/type/CommonDataEvent.h"
#include "ebpf/type/NetworkObserverEvent.h"
#include "ebpf/util/FrequencyManager.h"
#include "ebpf/util/sampler/Sampler.h"


namespace logtail::ebpf {

enum class JobType {
    METRIC_AGG,
    SPAN_AGG,
    LOG_AGG,
    HOST_META_UPDATE,
};

class NetworkObserverScheduleConfig : public ScheduleConfig {
public:
    NetworkObserverScheduleConfig(const std::chrono::seconds& interval, JobType jobType)
        : ScheduleConfig(PluginType::NETWORK_OBSERVE, interval), mJobType(jobType) {}

    JobType mJobType;
};

class NetworkObserverManager : public AbstractManager {
public:
    static std::shared_ptr<NetworkObserverManager>
    Create(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
           const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
           const PluginMetricManagerPtr& metricManager) {
        return std::make_shared<NetworkObserverManager>(processCacheManager, eBPFAdapter, queue, metricManager);
    }

    NetworkObserverManager() = delete;
    ~NetworkObserverManager() override { LOG_INFO(sLogger, ("begin destruct plugin", "network_observer")); }
    PluginType GetPluginType() override { return PluginType::NETWORK_OBSERVE; }
    NetworkObserverManager(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
                           const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                           const PluginMetricManagerPtr& metricManager);

    int Init(const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) override;

    int Destroy() override;

    void UpdateWhitelists(std::vector<std::string>&& enableCids, std::vector<std::string>&& disableCids);

    int HandleEvent([[maybe_unused]] const std::shared_ptr<CommonEvent>& event) override { return 0; }

    int PollPerfBuffer() override { return 0; }

    void RecordEventLost(enum callback_type_e type, uint64_t lostCount);

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

    int Update([[maybe_unused]] const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) override;

    int Suspend() override {
        mSuspendFlag = true;
        return 0;
    }

    int Resume(const std::variant<SecurityOptions*, ObserverNetworkOption*>&) override {
        mSuspendFlag = false;
        return 0;
    }

    bool ConsumeLogAggregateTree(const std::chrono::steady_clock::time_point& execTime);
    bool ConsumeMetricAggregateTree(const std::chrono::steady_clock::time_point& execTime);
    bool ConsumeSpanAggregateTree(const std::chrono::steady_clock::time_point& execTime);
    bool ConsumeNetMetricAggregateTree(const std::chrono::steady_clock::time_point& execTime);
    bool UploadHostMetadataUpdateTask();

    void HandleHostMetadataUpdate(const std::vector<std::string>& podCidVec);

    bool ScheduleNext(const std::chrono::steady_clock::time_point& execTime,
                      const std::shared_ptr<ScheduleConfig>& config) override;

private:
    void processRecord(const std::shared_ptr<AbstractRecord>& record);
    void processRecordAsLog(const std::shared_ptr<AbstractRecord>& record);
    void processRecordAsSpan(const std::shared_ptr<AbstractRecord>& record);
    void processRecordAsMetric(const std::shared_ptr<AbstractRecord>& record);

    void handleRollback(const std::shared_ptr<AbstractRecord>& record, bool& drop);

    void runInThread();

    bool updateParsers(const std::vector<std::string>& protocols, const std::vector<std::string>& prevProtocols);

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

    CounterPtr mPushSpansTotal;
    CounterPtr mPushSpanGroupTotal;
    CounterPtr mPushMetricsTotal;
    CounterPtr mPushMetricGroupTotal;

    mutable ReadWriteLock mSamplerLock;
    std::shared_ptr<Sampler> mSampler;

    // store parsed records
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<AbstractRecord>> mRollbackQueue;
    std::deque<std::shared_ptr<AbstractRecord>> mRollbackRecords;

    // coreThread used for polling kernel event...
    std::thread mCoreThread;

    std::thread mRecordConsume;

    std::atomic_bool mEnableSpan = false;
    std::atomic_bool mEnableLog = false;
    std::atomic_bool mEnableMetric = false;

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
    void compareAndUpdate(const std::string& fieldName, const T& oldValue, const T& newValue, Func onUpdate) {
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

    std::atomic_int mExecTimes = 0;
#endif
};

} // namespace logtail::ebpf

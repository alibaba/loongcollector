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
#include <set>
#include <unordered_map>
#include <vector>

#include "MetricTypes.h"
#include "common/ProcParser.h"
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

class AppManager {
public:
};

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

inline size_t GenerateContainerKey(const std::string& cid) {
    std::hash<std::string> hasher;
    size_t key = 0;
    AttrHashCombine(key, hasher(cid));
    return key;
}

class NetworkObserverManager : public AbstractManager {
public:
    static std::shared_ptr<NetworkObserverManager>
    Create(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
           const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
           std::unique_ptr<ThreadPool>& threadPool,
           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
           const PluginMetricManagerPtr& metricManager,
           IntGaugePtr connNum = nullptr,
           CounterPtr pollEvents = nullptr,
           CounterPtr lossEvents = nullptr,
           CounterPtr attachSuccess = nullptr,
           CounterPtr attachFailed = nullptr,
           CounterPtr rollbackTotal = nullptr) {
        return std::make_shared<NetworkObserverManager>(processCacheManager,
                                                        eBPFAdapter,
                                                        threadPool,
                                                        queue,
                                                        metricManager,
                                                        connNum,
                                                        pollEvents,
                                                        lossEvents,
                                                        attachSuccess,
                                                        attachFailed,
                                                        rollbackTotal);
    }

    NetworkObserverManager() = delete;
    ~NetworkObserverManager() override {}
    PluginType GetPluginType() override { return PluginType::NETWORK_OBSERVE; }
    NetworkObserverManager(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
                           const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                           std::unique_ptr<ThreadPool>& threadPool,
                           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                           const PluginMetricManagerPtr& metricManager,
                           IntGaugePtr connNum,
                           CounterPtr pollEvents,
                           CounterPtr lossEvents,
                           CounterPtr attachSuccess,
                           CounterPtr attachFailed,
                           CounterPtr rollbackTotal);

    int Init(const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) override;

    int Destroy() override;

    bool SupportRegisterMultiConfig() override { return true; }

    int AddOrUpdateConfig(const CollectionPipelineContext*,
                          uint32_t,
                          const PluginMetricManagerPtr&,
                          const std::variant<SecurityOptions*, ObserverNetworkOption*>&) override;

    int RemoveConfig(const std::string&) override;

    int RegisteredConfigCount() override {
        ReadLock lk(mAppConfigLock);
        // return mWorkloads.size();
        return mWorkloadConfigs.size();
    }

    void UpdateWhitelists(std::vector<std::string>&& enableCids, std::vector<std::string>&& disableCids);

    int HandleEvent([[maybe_unused]] const std::shared_ptr<CommonEvent>& event) override { return 0; }

    int PollPerfBuffer() override { return 0; }

    void RecordEventLost(enum callback_type_e type, uint64_t lostCount);

    void AcceptNetCtrlEvent(struct conn_ctrl_event_t* event);
    void AcceptNetStatsEvent(struct conn_stats_event_t* event);
    void AcceptDataEvent(struct conn_data_event_t* event);

    void PollBufferWrapper();
    void ConsumeRecords();


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
    // the following 3 methods are not thread safe ...
    std::shared_ptr<AppDetail>
    getWorkloadAppConfig(const std::string& ns, const std::string& workloadKind, const std::string& workloadName);
    std::shared_ptr<AppDetail> getWorkloadAppConfig(size_t workloadKey);
    std::shared_ptr<AppDetail> getContainerAppConfig(size_t containerIdKey);

    // thread safe, can be used in timer thread ...
    std::shared_ptr<AppDetail> getConnAppConfig(const std::shared_ptr<Connection>& conn);

    // only used in poller thread ...
    std::shared_ptr<AppDetail> getAppConfigFromReplica(const std::shared_ptr<Connection>& conn);

    std::array<size_t, 1> generateAggKeyForSpan(const std::shared_ptr<AbstractRecord>&,
                                                const std::shared_ptr<logtail::ebpf::AppDetail>&);
    std::array<size_t, 1> generateAggKeyForLog(const std::shared_ptr<AbstractRecord>&,
                                               const std::shared_ptr<logtail::ebpf::AppDetail>&);
    std::array<size_t, 2> generateAggKeyForAppMetric(const std::shared_ptr<AbstractRecord>&,
                                                     const std::shared_ptr<logtail::ebpf::AppDetail>&);
    std::array<size_t, 2> generateAggKeyForNetMetric(const std::shared_ptr<AbstractRecord>&,
                                                     const std::shared_ptr<logtail::ebpf::AppDetail>&);

    void processRecord(const std::unordered_map<size_t, std::shared_ptr<AppDetail>>& currentContainerConfigs,
                       const std::shared_ptr<AbstractRecord>& record);
    void processRecordAsLog(const std::shared_ptr<AbstractRecord>& record,
                            const std::shared_ptr<logtail::ebpf::AppDetail>&);
    void processRecordAsSpan(const std::shared_ptr<AbstractRecord>& record,
                             const std::shared_ptr<logtail::ebpf::AppDetail>&);
    void processRecordAsMetric(const std::shared_ptr<AbstractRecord>& record,
                               const std::shared_ptr<logtail::ebpf::AppDetail>&);
    void handleRollback(const std::shared_ptr<AbstractRecord>& record, bool& drop);

    void runInThread();

    bool updateParsers(const std::vector<std::string>& protocols, const std::vector<std::string>& prevProtocols);

    void executeTimerTask(JobType type, const std::chrono::steady_clock::time_point& execTime);

    std::unique_ptr<ConnectionManager> mConnectionManager;

    mutable std::atomic_long mDataEventsDropTotal = 0;

    mutable std::atomic_int64_t mConntrackerNum = 0;
    mutable std::atomic_int64_t mRecvConnStatEventsTotal = 0;
    mutable std::atomic_int64_t mRecvCtrlEventsTotal = 0;
    mutable std::atomic_int64_t mRecvHttpDataEventsTotal = 0;
    mutable std::atomic_int64_t mLostConnStatEventsTotal = 0;
    mutable std::atomic_int64_t mLostCtrlEventsTotal = 0;
    mutable std::atomic_int64_t mLostDataEventsTotal = 0;

    // store parsed records
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<AbstractRecord>> mRollbackQueue;
    std::deque<std::shared_ptr<AbstractRecord>> mRollbackRecords;

    // coreThread used for polling kernel event...
    std::thread mCoreThread;

    std::thread mRecordConsume;

    FrequencyManager mPollKernelFreqMgr;
    FrequencyManager mConsumerFreqMgr;

    int mCidOffset = -1;

    ReadWriteLock mAppAggLock;
    SIZETAggTreeWithSourceBuffer<AppMetricData, std::shared_ptr<AbstractRecord>> mAppAggregator;

    ReadWriteLock mNetAggLock;
    SIZETAggTreeWithSourceBuffer<NetMetricData, std::shared_ptr<AbstractRecord>> mNetAggregator;

    ReadWriteLock mSpanAggLock;
    SIZETAggTree<AppSpanGroup, std::shared_ptr<AbstractRecord>> mSpanAggregator;

    ReadWriteLock mLogAggLock;
    SIZETAggTree<AppLogGroup, std::shared_ptr<AbstractRecord>> mLogAggregator;

    // cache relative metric
    IntGaugePtr mConnectionNum;
    CounterPtr mPollNetEventsTotal;
    CounterPtr mLossNetEventsTotal;
    CounterPtr mNetMetaAttachSuccessTotal;
    CounterPtr mNetMetaAttachFailedTotal;
    CounterPtr mNetMetaAttachRollbackTotal;

    void UpdateConfigVersionAndWhitelist(std::vector<std::string>&& newCids, std::vector<std::string>&& expiredCids) {
        if (!newCids.empty() || !expiredCids.empty()) {
            mConfigVersion++;
            UpdateWhitelists(std::move(newCids), std::move(expiredCids));
        }
    }

    void UpdateContainerConfigs(size_t workloadKey, std::shared_ptr<AppDetail> newConfig) {
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

    mutable ReadWriteLock
        mAppConfigLock; // protect mConfigVersion/mWorkloadConfigs/mContainerConfigs/mConfigToWorkloads
    int mConfigVersion = 0;
    std::unordered_map<size_t, WorkloadConfig> mWorkloadConfigs; // workloadKey => {config, containers}
    std::unordered_map<size_t, std::shared_ptr<AppDetail>> mContainerConfigs; // containerKey => config
    std::unordered_map<std::string, std::set<size_t>> mConfigToWorkloads; // configName => workloadKeys

    // replica of mContainerConfigs, only used in poller thread ...
    std::unordered_map<size_t, std::shared_ptr<AppDetail>> mContainerConfigsReplica;

    std::shared_ptr<Sampler> mSampler;

    std::string mClusterId;

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

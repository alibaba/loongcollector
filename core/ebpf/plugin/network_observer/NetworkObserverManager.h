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
#include <vector>

#include "ConnTrackerManager.h"
#include "Worker.h"
#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/Config.h"
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/plugin/BaseManager.h"
#include "ebpf/type/CommonDataEvent.h"
#include "ebpf/type/NetworkObserverEvent.h"
#include "ebpf/util/FrequencyManager.h"
#include "ebpf/util/sampler/Sampler.h"

namespace logtail {
namespace ebpf {


template class WorkerFunc<std::unique_ptr<NetDataEvent>, std::unique_ptr<AbstractRecord>>;

class NetworkObserverManager : public AbstractManager {
public:
    static std::shared_ptr<NetworkObserverManager>
    Create(std::shared_ptr<BaseManager>& mgr,
           std::shared_ptr<SourceManager> sourceManager,
           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
           std::shared_ptr<Timer> scheduler) {
        return std::make_shared<NetworkObserverManager>(mgr, sourceManager, queue, scheduler);
    }

    NetworkObserverManager() = delete;
    ~NetworkObserverManager() { Destroy(); }
    virtual PluginType GetPluginType() override { return PluginType::NETWORK_OBSERVE; }
    NetworkObserverManager(std::shared_ptr<BaseManager>& baseMgr,
                           std::shared_ptr<SourceManager> sourceManager,
                           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                           std::shared_ptr<Timer> scheduler);

    int Init(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*> options) override;
    int Destroy() override;
    void UpdateWhitelists(std::vector<std::string>&& enableCids, std::vector<std::string>&& disableCids);

    virtual int HandleEvent(const std::shared_ptr<CommonEvent> event) override { return 0; }

    virtual int PollPerfBuffer() override { return 0; }

    void Stop();

    void RecordEventLost(enum callback_type_e type, uint64_t lost_count);

    void AcceptNetCtrlEvent(struct conn_ctrl_event_t* event);
    void AcceptNetStatsEvent(struct conn_stats_event_t* event);

    void EnqueueDataEvent(std::unique_ptr<NetDataEvent> data_event) const;

    void PollBufferWrapper();
    void ConsumeRecords();

    std::array<size_t, 1> GenerateAggKeyForSpan(const std::shared_ptr<AbstractAppRecord> event);
    std::array<size_t, 1> GenerateAggKeyForLog(const std::shared_ptr<AbstractAppRecord> event);
    std::array<size_t, 2> GenerateAggKeyForAppMetric(const std::shared_ptr<AbstractAppRecord> event);

    virtual std::unique_ptr<PluginConfig>
    GeneratePluginConfig(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*> options) {
        auto ebpfConfig = std::make_unique<PluginConfig>();
        ebpfConfig->mPluginType = PluginType::NETWORK_OBSERVE;
        return ebpfConfig;
    }

    virtual int Update(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*> options) override {
        // TODO
        LOG_WARNING(sLogger, ("TODO", "not support yet"));
        return 0;
    }

private:
    //   std::string Record2FileLog(const std::shared_ptr<HttpRecord> &);
    //   void ConsumeRecordsAsFileLogs(const std::vector<std::shared_ptr<AbstractRecord>> &records, size_t count);
    void ConsumeRecordsAsEvent(std::vector<std::shared_ptr<AbstractRecord>>& records, size_t count);
    void ConsumeRecordsAsMetric(std::vector<std::shared_ptr<AbstractRecord>>& records, size_t count);
    void ConsumeRecordsAsTrace(std::vector<std::shared_ptr<AbstractRecord>>& records, size_t count);

    bool ConsumeLogAggregateTree(const std::chrono::steady_clock::time_point& execTime);
    bool ConsumeMetricAggregateTree(const std::chrono::steady_clock::time_point& execTime);
    bool ConsumeSpanAggregateTree(const std::chrono::steady_clock::time_point& execTime);

    void RunInThread();

    bool UpdateParsers(const std::vector<std::string>& protocols);

    std::shared_ptr<ConnTrackerManager> mConnTrackerMgr;

    // TODO @qianlu.kk modify T for abstract event
    // store raw events
    mutable moodycamel::BlockingConcurrentQueue<std::unique_ptr<NetDataEvent>> mRawEventQueue;

    mutable std::atomic_long mDataEventsDropTotal = 0;

    mutable std::atomic_int64_t mConntrackerNum = 0;
    mutable std::atomic_int64_t mRecvConnStatEventsTotal = 0;
    mutable std::atomic_int64_t mRecvCtrlEventsTotal = 0;
    mutable std::atomic_int64_t mRecvHttpDataEventsTotal = 0;
    mutable std::atomic_int64_t mLostConnStatEventsTotal = 0;
    mutable std::atomic_int64_t mLostCtrlEventsTotal = 0;
    mutable std::atomic_int64_t mLostDataEventsTotal = 0;

    mutable std::atomic_int64_t mParseHttpRecordsSuccessTotal = 0;
    mutable std::atomic_int64_t mParseHttpRecordsFailedTotal = 0;
    mutable std::atomic_int64_t mAggMapEntitiesNum = 0;

    double mSampleRate = 1.0;

    std::unique_ptr<Sampler> mSampler;

    // store parsed records
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<AbstractRecord>> mRecordQueue;

    // WorkerPool: used to parse protocols from raw record
    // NetDataHandler netDataHandler_;
    std::unique_ptr<WorkerPool<std::unique_ptr<NetDataEvent>, std::shared_ptr<AbstractRecord>>> mWorkerPool;

    // coreThread used for polling kernel event...
    std::thread mCoreThread;

    // recordConsume used for polling kernel event...
    std::thread mRecordConsume;

    std::atomic_bool mEnableSpan;
    std::atomic_bool mEnableLog;
    std::atomic_bool mEnableMetric;

    FrequencyManager mPollKernelFreqMgr;
    FrequencyManager mConsumerFreqMgr;

    std::unique_ptr<ObserverNetworkOption> mPreviousOpt;

    ReadWriteLock mAppAggLock;
    SIZETAggTree<AppMetricData, std::shared_ptr<AbstractAppRecord>> mAppAggregator;


    ReadWriteLock mNetAggLock;
    SIZETAggTree<NetMetricData, std::shared_ptr<ConnStatsRecord>> mNetAggregator;


    ReadWriteLock mSpanAggLock;
    SIZETAggTree<AppSpanGroup, std::shared_ptr<AbstractAppRecord>> mSpanAggregator;

    ReadWriteLock mLogAggLock;
    SIZETAggTree<AppLogGroup, std::shared_ptr<AbstractAppRecord>> mLogAggregator;


    template <typename T, typename Func>
    void CompareAndUpdate(const std::string& fieldName, const T& oldValue, const T& newValue, Func onUpdate) {
        if (oldValue != newValue) {
            LOG_INFO(sLogger, ("config change!, fieldName", fieldName));
            onUpdate(oldValue, newValue);
        }
    }
};

} // namespace ebpf
} // namespace logtail

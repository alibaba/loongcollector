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
#include "ebpf/type/NetworkObserverEvent.h"
#include "ebpf/util/FrequencyManager.h"
#include "ebpf/util/sampler/Sampler.h"
#include "ebpf/type/CommonDataEvent.h"

namespace logtail {
namespace ebpf {


template class WorkerFunc<std::unique_ptr<NetDataEvent>, std::unique_ptr<AbstractRecord>>;

class NetworkObserverManager : public AbstractManager {
public:
    using LogHandler = std::function<void(const std::vector<std::unique_ptr<ApplicationBatchEvent>>&)>;
    using MeterHandler = std::function<void(const std::vector<std::unique_ptr<ApplicationBatchMeasure>>&)>;
    using SpanHandler = std::function<void(const std::vector<std::unique_ptr<ApplicationBatchSpan>>&)>;

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
    explicit NetworkObserverManager(
        std::shared_ptr<BaseManager>& baseMgr,
        std::shared_ptr<SourceManager> sourceManager,
        moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue, std::shared_ptr<Timer> scheduler)
        : AbstractManager(baseMgr, sourceManager, queue, scheduler) {}

    int Init(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*> options) override;
    int Destroy() override;
    void UpdateWhitelists(std::vector<std::string>&& enableCids, std::vector<std::string>&& disableCids);

    virtual int HandleEvent(const std::shared_ptr<CommonEvent> event) override {return 0;}

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

private:
    //   std::string Record2FileLog(const std::shared_ptr<HttpRecord> &);
    //   void ConsumeRecordsAsFileLogs(const std::vector<std::shared_ptr<AbstractRecord>> &records, size_t count);
    void ConsumeRecordsAsEvent(const std::vector<std::shared_ptr<AbstractRecord>>& records, size_t count);
    void ConsumeRecordsAsMetric(const std::vector<std::shared_ptr<AbstractRecord>>& records, size_t count);
    void ConsumeRecordsAsTrace(const std::vector<std::shared_ptr<AbstractRecord>>& records, size_t count);

    void RunInThread();

    bool UpdateParsers(const std::vector<std::string>& protocols);

    int StartAggregator();
    int StopAggregator();

    std::shared_ptr<ConnTrackerManager> mConnTrackerMgr;

    // TODO @qianlu.kk modify T for abstract event
    // store raw events
    mutable moodycamel::BlockingConcurrentQueue<std::unique_ptr<NetDataEvent>> mRawEventQueue;

    mutable std::atomic_long mDataEventsDropTotal = 0;

    mutable std::atomic_int64_t mConntrackerNum_ = 0;
    mutable std::atomic_int64_t mRecvConnStatEventsTotal_ = 0;
    mutable std::atomic_int64_t mRecvCtrlEventsTotal_ = 0;
    mutable std::atomic_int64_t mRecvHttpDataEventsTotal_ = 0;
    mutable std::atomic_int64_t mLostConnStatEventsTotal_ = 0;
    mutable std::atomic_int64_t mLostCtrlEventsTotal_ = 0;
    mutable std::atomic_int64_t mLostDataEventsTotal_ = 0;

    mutable std::atomic_int64_t mParseHttpRecordsSuccessTotal_ = 0;
    mutable std::atomic_int64_t mParseHttpRecordsFailedTotal_ = 0;
    mutable std::atomic_int64_t mAggMapEntitiesNum_ = 0;

    int mConsumeWaitMs_ = 200;
    int mConsumeBatchMax_ = 4096;
    int mMetricConsumeWaitMs_ = 200;
    int mMetricConsumeBatchMax_ = 4096;
    bool mIsProd_ = false;

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

    std::unique_ptr<SIZETAggTree<AppMetricData, std::shared_ptr<AbstractAppRecord>>> mAppAggregator;
    std::unique_ptr<SIZETAggTree<NetMetricData, std::shared_ptr<ConnStatsRecord>>> mNetAggregator;
    std::unique_ptr<SIZETAggTree<NetMetricData, std::shared_ptr<AbstractAppRecord>>> mSpanAggregator;

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

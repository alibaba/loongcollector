//
// Created by qianlu on 2024/7/1.
//

#pragma once

#include <atomic>
#include <vector>

#include "ebpf/util/sampler/Sampler.h"
#include "ebpf/plugin/AbstractManager.h"
#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/type/NetworkObserverEvent.h"
#include "ConnTrackerManager.h"
#include "Worker.h"
#include "ebpf/plugin/BaseManager.h"
#include "ebpf/util/FrequencyManager.h"

namespace logtail {
namespace ebpf {


template class WorkerFunc<std::unique_ptr<NetDataEvent>, std::unique_ptr<AbstractRecord>>;

class NetworkObserverManager : public AbstractManager {
public:
    static std::shared_ptr<NetworkObserverManager> Create(std::unique_ptr<BaseManager>& mgr, std::shared_ptr<SourceManager> sourceManager, 
        std::function<void(const std::vector<std::unique_ptr<ApplicationBatchEvent>>& events)> flusher) {
        return std::make_shared<NetworkObserverManager>(mgr, sourceManager, flusher);
    }
    NetworkObserverManager() = delete;
    ~NetworkObserverManager() {}
    virtual PluginType GetPluginType() override { return PluginType::NETWORK_OBSERVE; }
    explicit NetworkObserverManager(std::unique_ptr<BaseManager>& baseMgr, std::shared_ptr<SourceManager> sourceManager, std::function<void(const std::vector<std::unique_ptr<ApplicationBatchEvent>>&)> flusher) 
        : AbstractManager(baseMgr, sourceManager), mFlusher(flusher) {}
    
    int Init(std::unique_ptr<logtail::ebpf::PluginConfig>) override;
    int Destroy() override;
    int EnableCallName(const std::string& call_name, const configType config) override { return 0; }
    int DisableCallName(const std::string& call_name) override { return 0; }
    void UpdateWhitelists(std::vector<std::string>&& enableCids, std::vector<std::string>&& disableCids);

    void Stop();

    void RecordEventLost(enum callback_type_e type, uint64_t lost_count);

    void AcceptNetCtrlEvent(struct conn_ctrl_event_t* event);
    void AcceptNetStatsEvent(struct conn_stats_event_t *event);

    void EnqueueDataEvent(std::unique_ptr<NetDataEvent> data_event) const;

    void PollBufferWrapper();
    void ConsumeRecords();
    
private:
    //   std::string Record2FileLog(const std::shared_ptr<HttpRecord> &);
    //   void ConsumeRecordsAsFileLogs(const std::vector<std::shared_ptr<AbstractRecord>> &records, size_t count);
    void ConsumeRecordsAsEvent(const std::vector<std::shared_ptr<AbstractRecord>> &records, size_t count);
    
    void RunInThread();

    std::function<void(const std::vector<std::unique_ptr<ApplicationBatchEvent>>& events)> mFlusher;

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

    std::thread mCoreThread;
    std::thread mRecordConsume;

    std::atomic_bool mEnableSpan;
    std::atomic_bool mEnableLog;
    std::atomic_bool mEnableMetric;

    FrequencyManager mPollKernelFreqMgr;
    FrequencyManager mConsumerFreqMgr;

  // TODO @qianlu.kk 
//   std::unique_ptr<Aggregator> metric_aggregator_ = nullptr;
};

}
}
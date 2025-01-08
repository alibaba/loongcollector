
#include "NetworkObserverManager.h"
#include "ebpf/include/export.h"
#include "logger/Logger.h"

extern "C" {
#include <net.h>
}

namespace logtail {
namespace ebpf {

void NetworkObserverManager::EnqueueDataEvent(std::unique_ptr<NetDataEvent> data_event) const {
    mRecvHttpDataEventsTotal_.fetch_add(1);
    auto ct = mConnTrackerMgr->GetOrCreateConntracker(data_event->conn_id);
    if (ct) {
        ct->RecordActive();
        ct->SafeUpdateRole(data_event->role);
        ct->SafeUpdateProtocol(data_event->protocol);
    } else {
        mDataEventsDropTotal.fetch_add(1);
        LOG_DEBUG(sLogger, ("cannot find or create conn_tracker, skip data event ... ", ""));
    }
    mRawEventQueue.enqueue(std::move(data_event));
}

int NetworkObserverManager::Init(std::unique_ptr<logtail::ebpf::PluginConfig> arg) {
    LOG_INFO(sLogger, ("init protocol parser", "begin"));
    ProtocolParserManager::GetInstance().Init();
    LOG_INFO(sLogger, ("init protocol parser", "done"));

    mConnTrackerMgr = ConnTrackerManager::Create();
    mConnTrackerMgr->Start();

    mPollKernelFreqMgr.SetPeriod(std::chrono::milliseconds(200));
    mConsumerFreqMgr.SetPeriod(std::chrono::milliseconds(200));

    // init converger

    // init sampler

    // call ebpf_init
    std::unique_ptr<PluginConfig> pc = std::make_unique<PluginConfig>();
    pc->mPluginType = PluginType::NETWORK_OBSERVE;
    NetworkObserveConfig config;
    config.mCustomCtx = (void*)this;
    config.mStatsHandler = [](void *custom_data, struct conn_stats_event_t *event){
        auto mgr = static_cast<NetworkObserverManager*>(custom_data);
        if (mgr) {
            mgr->mRecvConnStatEventsTotal_.fetch_add(1);
            mgr->AcceptNetStatsEvent(event);
        }
    };
    config.mDataHandler = [](void *custom_data, struct conn_data_event_t *event) {
        LOG_DEBUG(sLogger, ("[DUMP] stats event handle, fd", event->conn_id.fd)
            ("pid", event->conn_id.tgid)
            ("start", event->conn_id.start)
            ("role", int(event->role))
            ("startTs", event->start_ts)
            ("endTs", event->end_ts)
            ("protocol", int(event->protocol))
            ("req_len", event->request_len)
            ("resp_len", event->response_len)
            ("data", event->msg)
        );
        auto mgr = static_cast<NetworkObserverManager*>(custom_data);
        if (mgr == nullptr) return;

        if (event->request_len == 0 || event->response_len == 0) {
            // LOG_DEBUG(sLogger, ("[DUMP] [ProcessNetDataEventHandler] request_len or response_len assert failed!", ""));
            return;
        }

        // will do deepcopy
        auto data_event_ptr = std::make_unique<NetDataEvent>(event);

        // enqueue, waiting workers to process data event ...
        mgr->EnqueueDataEvent(std::move(data_event_ptr));
    };
    config.mCtrlHandler = [](void *custom_data, struct conn_ctrl_event_t *event) {
        auto mgr = static_cast<NetworkObserverManager*>(custom_data);
        if (mgr) {
            mgr->mRecvCtrlEventsTotal_.fetch_add(1);
            mgr->AcceptNetCtrlEvent(event);
        }
    };
    config.mLostHandler = [](void *custom_data, enum callback_type_e type, uint64_t lost_count){
        // LOG(INFO) <<  "========= [DUMP] net event lost, type:" << int(type) << " count" << lost_count << "========" ;
        auto mgr = static_cast<NetworkObserverManager*>(custom_data);
        if (!mgr) {
            return;
        }
        mgr->RecordEventLost(type, lost_count);
    };

    pc->mConfig = config;
    auto ret = mSourceManager->StartPlugin(PluginType::NETWORK_OBSERVE, std::move(pc));
    if (!ret) {
        return -1;
    }

    // call ebpf start
    mRawEventQueue = moodycamel::BlockingConcurrentQueue<std::unique_ptr<NetDataEvent>>(1024);
    mRecordQueue = moodycamel::BlockingConcurrentQueue<std::shared_ptr<AbstractRecord>>(4096);
    // TODO @qianlu.kk
    mWorkerPool =
        std::make_unique<WorkerPool<std::unique_ptr<NetDataEvent>, std::shared_ptr<AbstractRecord>>>
        (mRawEventQueue, mRecordQueue, NetDataHandler(), 1);
    
    LOG_INFO(sLogger, ("begin to start ebpf ... ", ""));
    this->flag_ = true;
    this->RunInThread();
    return 0;
}

void NetworkObserverManager::RunInThread() {
    // periodically poll perf buffer ...
    LOG_INFO(sLogger, ("enter core thread ", ""));
    // start a new thread to poll perf buffer ...
    mCoreThread = std::thread(&NetworkObserverManager::PollBufferWrapper, this);
    mRecordConsume = std::thread(&NetworkObserverManager::ConsumeRecords, this);

    LOG_INFO(sLogger, ("sockettracer plugin install.", ""));
}


void NetworkObserverManager::ConsumeRecordsAsEvent(
    const std::vector<std::shared_ptr<AbstractRecord>> &records, size_t count) {
    if (count == 0) return;
    static const int32_t sbucket = 100;
    auto bucket_count = (count / sbucket) + 1;
    auto events = std::vector<std::unique_ptr<ApplicationBatchEvent>>(bucket_count);
    int nn = 0;
    for (size_t n = 0 ; n < bucket_count; n ++) {
      auto batch_events = std::make_unique<ApplicationBatchEvent>();
      for (size_t j = 0; j < sbucket; j++) {
        size_t i = n * sbucket + j;
        if (i >= count) break;
        std::shared_ptr<HttpRecord> http_record = std::dynamic_pointer_cast<HttpRecord>(records[i]);
        if (http_record == nullptr) continue;

        auto conn_id = http_record->GetConnId();
        auto common_attrs = mConnTrackerMgr->GetConnTrackerAttrs(conn_id);
        std::string app_id = std::to_string(conn_id.tgid);

        auto single_event = std::make_unique<SingleEvent>();
        for (size_t i = 0 ; i < kConnTrackerElementsTableSize; i ++) {
          if(kConnTrackerTable.ColLogKey(i) == "" || common_attrs[i] == "") {
            continue;
          }
          single_event->AppendTags({std::string(kConnTrackerTable.ColLogKey(i)), common_attrs[i]});
        }
        // set time stamp
        auto start_ts_ns = http_record->GetStartTimeStamp();
        single_event->SetTimestamp(start_ts_ns + time_diff_.count());

        single_event->AppendTags({"latency", std::to_string(http_record->GetLatencyMs())});
        single_event->AppendTags({"http.method", http_record->GetMethod()});
        single_event->AppendTags({"http.path", http_record->GetPath()});
        single_event->AppendTags({"http.protocol", http_record->GetProtocolVersion()});
        single_event->AppendTags({"http.latency", std::to_string(http_record->GetLatencyMs())});
        single_event->AppendTags(
            {"http.status_code", std::to_string(http_record->GetStatusCode())});
        single_event->AppendTags({"http.request.body", http_record->GetReqBody()});
        single_event->AppendTags({"http.response.body", http_record->GetRespBody()});
        for (auto &h : http_record->GetReqHeaderMap()) {
            single_event->AppendTags({"http.request.header." + h.first, h.second});
        }
        for (auto &h : http_record->GetRespHeaderMap()) {
            single_event->AppendTags({"http.response.header." + h.first, h.second});
        }
        batch_events->AppendEvent(std::move(single_event));
        nn++;
        batch_events->tags_ = {{"bucket", std::to_string(n)}, {"version", "v2"}};
      }
      if (batch_events->events_.size()) {
        events[n] = std::move(batch_events);
      }
    }
    
    {
        // TODO @qianlu.kk 
        mFlusher(events);
        LOG_WARNING(sLogger, ("call event cb, total event count", count) ("bucket count", bucket_count) ("events size", events.size()) ("real size", nn));
        // send data ...
    //   std::shared_lock<std::shared_mutex> lock(event_cb_mutex_);
    //   if (event_cb_) {
    //     LOG(WARNING) << "call event cb, total event:" << count << " bucket_count:" << bucket_count << " events size:" << events.size() << " real size:" << nn;
    //     event_cb_(events);
    //     UpdatePushEventTotal(count);
    //   }
    }
}

// TODO @qianlu.kk
void NetworkObserverManager::ConsumeRecords() {
    std::vector<std::shared_ptr<AbstractRecord>> items(1024);
    while (flag_) {
        // poll event from
        auto now = std::chrono::steady_clock::now();
        auto next_window = mConsumerFreqMgr.Next();
        if (!mConsumerFreqMgr.Expired(now)) {
            std::this_thread::sleep_until(next_window);
            mConsumerFreqMgr.Reset(next_window);
        } else {
            mConsumerFreqMgr.Reset(now);
        }
        size_t count = mRecordQueue.wait_dequeue_bulk_timed(
            items.data(),
            1024,
            std::chrono::milliseconds(200));
        LOG_INFO(sLogger, ("get records:", count));
        // handle ....
        

        if (count > 0) {
            // mFlusher(items);
            ConsumeRecordsAsEvent(items, count);
        }

        items.clear();
        items.resize(1024);
    }
}


void NetworkObserverManager::PollBufferWrapper() {
    int32_t flag = 0;
    int cnt = 0;
    while(this->flag_) {
        // poll event from
        auto now = std::chrono::steady_clock::now();
        auto next_window = mPollKernelFreqMgr.Next();
        if (!mPollKernelFreqMgr.Expired(now)) {
            std::this_thread::sleep_until(next_window);
            mPollKernelFreqMgr.Reset(next_window);
        } else {
            mPollKernelFreqMgr.Reset(now);
        }

        // poll stats -> ctrl -> info
        int ret = mSourceManager->PollPerfBuffers(PluginType::NETWORK_OBSERVE, 4096, &flag, 0);
        if (ret < 0) {
            LOG_WARNING(sLogger, ("poll event err, ret", ret));
        }
        mConnTrackerMgr->IterationsInternal(cnt++);

        LOG_DEBUG(sLogger, 
            ("===== statistic =====>> total data events:", mRecvHttpDataEventsTotal_.load())
            (" total conn stats events:", mRecvConnStatEventsTotal_.load())
            (" total ctrl events:", mRecvCtrlEventsTotal_.load())
            (" lost data events:", mLostDataEventsTotal_.load())
            (" lost stats events:", mLostConnStatEventsTotal_.load())
            (" lost ctrl events:", mLostCtrlEventsTotal_.load())
        );
    }
}

void NetworkObserverManager::RecordEventLost(enum callback_type_e type, uint64_t lost_count) {
    switch (type)
    {
    case STAT_HAND:
        mLostConnStatEventsTotal_.fetch_add(lost_count);
        return;
    case INFO_HANDLE:
        mLostDataEventsTotal_.fetch_add(lost_count);
        return;
    case CTRL_HAND:
        mLostCtrlEventsTotal_.fetch_add(lost_count);
        return;
    default:
        return;
    }
}

void NetworkObserverManager::AcceptNetStatsEvent(struct conn_stats_event_t *event) {
    LOG_DEBUG(sLogger, ("[DUMP] stats event handle, fd", event->conn_id.fd)
        ("pid", event->conn_id.tgid)
        ("start", event->conn_id.start)
        ("role", int(event->role))
        ("state", int(event->conn_events))
        ("eventTs", event->ts));
    mConnTrackerMgr->AcceptNetStatsEvent(event);
}

void NetworkObserverManager::AcceptNetCtrlEvent(struct conn_ctrl_event_t *event) {
    LOG_DEBUG(sLogger, ("[DUMP] ctrl event handle, fd", event->conn_id.fd)
        ("pid", event->conn_id.tgid)
        ("start", event->conn_id.start)
        ("type", int(event->type))
        ("eventTs", event->ts));
    mConnTrackerMgr->AcceptNetCtrlEvent(event);
}


void NetworkObserverManager::Stop() {
    // TODO shutdown aggregator ...
    mSourceManager->StopPlugin(PluginType::NETWORK_OBSERVE);
    mConnTrackerMgr->Stop();
    this->flag_ = false;
    // destroy thread
    if (this->mCoreThread.joinable()) {
        this->mCoreThread.join();
    }

    if (this->mRecordConsume.joinable()) {
        this->mRecordConsume.join();
    }
}

int NetworkObserverManager::Destroy() {
    return 0;
}

void NetworkObserverManager::UpdateWhitelists(std::vector<std::string>&& enableCids, std::vector<std::string>&& disableCids) {}

}
}
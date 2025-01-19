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

#include "NetworkObserverManager.h"

#include "common/magic_enum.hpp"
#include "ebpf/Config.h"
#include "ebpf/include/export.h"
#include "ebpf/util/TraceId.h"
#include "logger/Logger.h"
#include "ebpf/type/PeriodicalEvent.h"
#include "pipeline/queue/ProcessQueueItem.h"
#include "pipeline/queue/ProcessQueueManager.h"

extern "C" {
#include <net.h>
}

namespace logtail {
namespace ebpf {

std::array<size_t, 2> NetworkObserverManager::GenerateAggKeyForAppMetric(const std::shared_ptr<AbstractAppRecord> record) {
    // calculate agg key
    std::array<size_t, 2> hash_result;
    hash_result.fill(0UL);
    std::hash<std::string> hasher;
    auto connTrackerAttrs = mConnTrackerMgr->GetConnTrackerAttrs(record->GetConnId());

    for (size_t j = 0; j < kAppMetricsTable.elements().size(); j++) {
        int agg_level = static_cast<int>(kAppMetricsTable.elements()[j].agg_type());
        if (agg_level >= MinAggregationLevel && agg_level <= MaxAggregationLevel) {
            bool ok = kConnTrackerTable.HasCol(kAppMetricsTable.elements()[j].name());
            std::string attr;
            if (ok) {
                attr = connTrackerAttrs[kConnTrackerTable.ColIndex(kAppMetricsTable.elements()[j].name())];
            } else {
                // record dedicated ... 
            }
                
            int hash_result_index = agg_level - MinAggregationLevel;
            // TODO @qianlu.kk we should not to use record->GetMetricAttribute(j)
            // because we already have entity relative attributes in conn tracker, and we already get these attributes
            hash_result[hash_result_index] ^= hasher(attr) +
                                                0x9e3779b9 +
                                                (hash_result[hash_result_index] << 6) +
                                                (hash_result[hash_result_index] >> 2);
        }
    }

    return hash_result;
}

std::array<size_t, 1> NetworkObserverManager::GenerateAggKeyForSpan(const std::shared_ptr<AbstractAppRecord> record) {
    // calculate agg key
    // just appid
    std::array<size_t, 1> hash_result;
    hash_result.fill(0UL);
    // std::hash<uint64_t> hasher;

    return hash_result;
}

std::array<size_t, 1> NetworkObserverManager::GenerateAggKeyForLog(const std::shared_ptr<AbstractAppRecord> event) {
    // just appid
    std::array<size_t, 1> hash_result;
    hash_result.fill(0UL);
    // std::hash<uint64_t> hasher;

    return hash_result;
}

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

bool NetworkObserverManager::UpdateParsers(const std::vector<std::string>& protocols) {
    std::set<std::string> enableProtocols;
    for (auto protocol : protocols) {
        // to upper
        std::transform(
            protocol.begin(), protocol.end(), protocol.begin(), [](unsigned char c) { return std::toupper(c); });
        // unique
        enableProtocols.insert(protocol);
    }

    LOG_DEBUG(sLogger, ("init protocol parser", "begin"));
    for (auto& protocol : enableProtocols) {
        auto pro = magic_enum::enum_cast<ProtocolType>(protocol).value_or(ProtocolType::UNKNOWN);
        if (pro != ProtocolType::UNKNOWN) {
            LOG_DEBUG(sLogger,
                      ("add protocol parser", protocol)("protocol type", std::string(magic_enum::enum_name(pro))));
            ProtocolParserManager::GetInstance().AddParser(pro);
        }
    }
    LOG_DEBUG(sLogger, ("init protocol parser", "done"));
    return true;
}

int NetworkObserverManager::Init(const std::variant<SecurityOptions*, ObserverNetworkOption*> options) {
    auto opt = std::get<ObserverNetworkOption*>(options);
    if (!opt) {
        LOG_ERROR(sLogger, ("invalid options", ""));
        return -1;
    }

    if (!UpdateParsers(opt->mEnableProtocols)) {
        LOG_ERROR(sLogger, ("update parsers", "failed"));
    }

    // start conntracker ...
    mConnTrackerMgr = ConnTrackerManager::Create();
    mConnTrackerMgr->Start();

    mPollKernelFreqMgr.SetPeriod(std::chrono::milliseconds(200));
    mConsumerFreqMgr.SetPeriod(std::chrono::milliseconds(200));

    // TODO @qianlu.kk init converger

    // TODO @qianlu.kk init aggregator ...
    // SIZETAggTree<AppMetricData, std::shared_ptr<AbstractAppRecord>> mAppAggregator;
    mAppAggregator = std::make_unique<SIZETAggTree<AppMetricData, std::shared_ptr<AbstractAppRecord>>>(
        4096,
        [this](std::unique_ptr<AppMetricData>& base, const std::shared_ptr<AbstractAppRecord>& other) {
            base->m2xxCount += other->GetStatusCode() / 100 == 2;
            base->m3xxCount += other->GetStatusCode() / 100 == 3;
            base->m4xxCount += other->GetStatusCode() / 100 == 4;
            base->m5xxCount += other->GetStatusCode() / 100 == 5;
            base->mCount++;
            base->mErrCount += other->IsError();
            base->mSlowCount += other->IsSlow();
        },
        [this](const std::shared_ptr<AbstractAppRecord>& in) {
            return std::make_unique<AppMetricData>(in->GetConnId());
        });

    mNetAggregator = std::make_unique<SIZETAggTree<NetMetricData, std::shared_ptr<ConnStatsRecord>>>(
        4096,
        [this](std::unique_ptr<NetMetricData>& base, const std::shared_ptr<ConnStatsRecord>& other) {
            // TODO aggregate
            base->mDropCount += other->drop_count_;
            base->mRetransCount += other->retrans_count_;
            base->mRecvBytes += other->recv_bytes_;
            base->mSendBytes += other->send_bytes_;
            base->mRecvPkts += other->recv_packets_;
            base->mSendPkts += other->send_packets_;
        },
        [this](const std::shared_ptr<ConnStatsRecord>& in) {
            return std::make_unique<NetMetricData>(in->GetConnId());
        });

    std::unique_ptr<AggregateEvent> appTraceEvent = std::make_unique<AggregateEvent>(
        1,
        [this](const std::chrono::steady_clock::time_point& execTime) { // handler
            if (!this->mFlag || this->mSuspendFlag) {
                return false;
            }

            auto nodes = this->mSpanAggregator->GetNodesWithAggDepth(1);
            LOG_DEBUG(sLogger, ("enter aggregator ...", nodes.size()));
            if (nodes.empty()) {
                LOG_DEBUG(sLogger, ("empty nodes...", ""));
                return true;
            }

            for (auto& node : nodes) {
                // convert to a item and push to process queue
                PipelineEventGroup eventGroup(std::make_shared<SourceBuffer>()); // per node represent an APP ... 
                bool init = false;
                this->mSpanAggregator->ForEach(node, [&](const NetMetricData* group) {
                    // set process tag
                    if (!init) {
                        // set app attrs ... 
                        eventGroup.SetTag(std::string("service.name"), ""); // app name
                        eventGroup.SetTag(std::string("arms.appId"), ""); // app id
                        eventGroup.SetTag(std::string("host.ip"), ""); // pod ip
                        eventGroup.SetTag(std::string("host.name"), ""); // pod name
                        eventGroup.SetTag(std::string("arms.app.type"), "ebpf"); // 
                        eventGroup.SetTag(std::string("data_type"), "span");
                        init = true;
                    }

                    // generate tag metric
                    // auto* tagMetric = eventGroup.AddMetricEvent();
                    // auto* requestsMetric = eventGroup.AddMetricEvent();
                    // auto* errorMetric = eventGroup.AddMetricEvent();
                    // auto* slowMetric = eventGroup.AddMetricEvent();
                    // auto* latencyMetric = eventGroup.AddMetricEvent();
                    // auto* statusMetric = eventGroup.AddMetricEvent();

                    {
                        std::lock_guard lk(mContextMutex);
                        std::unique_ptr<ProcessQueueItem> item
                            = std::make_unique<ProcessQueueItem>(std::move(eventGroup), this->mPluginIndex);
                        if (QueueStatus::OK != ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
                            LOG_WARNING(sLogger,
                                        ("configName", mPipelineCtx->GetConfigName())("pluginIdx", this->mPluginIndex)(
                                            "[FileSecurityEvent] push queue failed!", ""));
                        }
                    }
                });
            }
            this->mAppAggregator->Clear();

            return true;
        },

        [this]() { // validator
            return !this->mFlag.load();
        });

    std::unique_ptr<AggregateEvent> appMetricEvent = std::make_unique<AggregateEvent>(
        15,
        [this](const std::chrono::steady_clock::time_point& execTime) { // handler
            if (!this->mFlag || this->mSuspendFlag) {
                return false;
            }

            auto nodes = this->mAppAggregator->GetNodesWithAggDepth(1);
            LOG_DEBUG(sLogger, ("enter aggregator ...", nodes.size()));
            if (nodes.empty()) {
                LOG_DEBUG(sLogger, ("empty nodes...", ""));
                return true;
            }

            for (auto& node : nodes) {
                // convert to a item and push to process queue
                PipelineEventGroup eventGroup(std::make_shared<SourceBuffer>()); // per node represent an APP ... 
                bool init = false;
                this->mAppAggregator->ForEach(node, [&](const AppMetricData* group) {
                    // set process tag
                    if (!init) {
                        // set app attrs ... 
                        eventGroup.SetTag(std::string("pid"), ""); // app id 
                        eventGroup.SetTag(std::string("serverIp"), ""); // pod ip
                        eventGroup.SetTag(std::string("source"), "ebpf");
                        eventGroup.SetTag(std::string("service"), ""); // app name
                        eventGroup.SetTag(std::string("host"), ""); // pod name
                        eventGroup.SetTag(std::string("data_type"), "metric");
                        init = true;
                    }

                    // generate tag metric
                    // {
                    //     auto* tagMetric = eventGroup.AddMetricEvent();

                    // }
                    // if (group->mCount) {
                    //     auto* requestsMetric = eventGroup.AddMetricEvent();
                    //     auto* latencyMetric = eventGroup.AddMetricEvent();
                    // }
                    // if (group->mErrCount) {
                    //     auto* errorMetric = eventGroup.AddMetricEvent();
                    // }
                    // if (group->mSlowCount) {
                    //     auto* slowMetric = eventGroup.AddMetricEvent();
                    // }
                    // if (group->m2xxCount || group->m3xxCount || group->m4xxCount || group->m5xxCount) {
                    //     auto* statusMetric = eventGroup.AddMetricEvent();
                    // }   

                    {
                        std::lock_guard lk(mContextMutex);
                        std::unique_ptr<ProcessQueueItem> item
                            = std::make_unique<ProcessQueueItem>(std::move(eventGroup), this->mPluginIndex);
                        if (QueueStatus::OK != ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
                            LOG_WARNING(sLogger,
                                        ("configName", mPipelineCtx->GetConfigName())("pluginIdx", this->mPluginIndex)(
                                            "[FileSecurityEvent] push queue failed!", ""));
                        }
                    }
                });
            }
            this->mAppAggregator->Clear();

            return true;
        },

        [this]() { // validator
            return !this->mFlag.load();
        });

    mScheduler->PushEvent(std::move(appMetricEvent));

    // init sampler
    mSampler = std::make_unique<HashRatioSampler>(0.01);

    mEnableLog = opt->mEnableLog;
    mEnableSpan = opt->mEnableSpan;
    mEnableMetric = opt->mEnableMetric;

    // TODO @qianlu.kk
    if (StartAggregator()) {
        LOG_ERROR(sLogger, ("failed to start aggregator", ""));
    }

    // diff opt
    if (mPreviousOpt) {
        CompareAndUpdate("mDisableConnStats",
                         mPreviousOpt->mDisableConnStats,
                         opt->mDisableConnStats,
                         [this](bool oldValue, bool newValue) {
                             // TODO @qianlu.kk update conn stats ...
                         });
        CompareAndUpdate("mEnableProtocols",
                         mPreviousOpt->mEnableProtocols,
                         opt->mEnableProtocols,
                         [this](const std::vector<std::string>& oldValue, const std::vector<std::string>& newValue) {
                             // TODO @qianlu.kk update conn stats ...
                         });
    }
    mPreviousOpt = std::make_unique<ObserverNetworkOption>(*opt);

    std::unique_ptr<PluginConfig> pc = std::make_unique<PluginConfig>();
    pc->mPluginType = PluginType::NETWORK_OBSERVE;
    NetworkObserveConfig config;
    config.mCustomCtx = (void*)this;
    config.mStatsHandler = [](void* custom_data, struct conn_stats_event_t* event) {
        auto mgr = static_cast<NetworkObserverManager*>(custom_data);
        if (mgr) {
            mgr->mRecvConnStatEventsTotal_.fetch_add(1);
            mgr->AcceptNetStatsEvent(event);
        }
    };

    config.mDataHandler = [](void* custom_data, struct conn_data_event_t* event) {
        LOG_DEBUG(sLogger,
                  ("[DUMP] stats event handle, fd", event->conn_id.fd)("pid", event->conn_id.tgid)(
                      "start", event->conn_id.start)("role", int(event->role))("startTs", event->start_ts)(
                      "endTs", event->end_ts)("protocol", int(event->protocol))("req_len", event->request_len)(
                      "resp_len", event->response_len)("data", event->msg));
        auto mgr = static_cast<NetworkObserverManager*>(custom_data);
        if (mgr == nullptr) {
            LOG_ERROR(sLogger, ("assert network observer handler failed", ""));
            return;
        }

        if (event->request_len == 0 || event->response_len == 0) {
            return;
        }

        // will do deepcopy
        auto data_event_ptr = std::make_unique<NetDataEvent>(event);

        // enqueue, waiting workers to process data event ...
        // @qianlu.kk Can we hold for a while and enqueue bulk ???
        mgr->EnqueueDataEvent(std::move(data_event_ptr));
    };

    config.mCtrlHandler = [](void* custom_data, struct conn_ctrl_event_t* event) {
        auto mgr = static_cast<NetworkObserverManager*>(custom_data);
        if (!mgr) {
            LOG_ERROR(sLogger, ("assert network observer handler failed", ""));
        }

        mgr->mRecvCtrlEventsTotal_.fetch_add(1);
        mgr->AcceptNetCtrlEvent(event);
    };

    config.mLostHandler = [](void* custom_data, enum callback_type_e type, uint64_t lost_count) {
        LOG_DEBUG(sLogger, ("========= [DUMP] net event lost, type", int(type))("count", lost_count));
        auto mgr = static_cast<NetworkObserverManager*>(custom_data);
        if (!mgr) {
            LOG_ERROR(sLogger, ("assert network observer handler failed", ""));
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
    mWorkerPool = std::make_unique<WorkerPool<std::unique_ptr<NetDataEvent>, std::shared_ptr<AbstractRecord>>>(
        mRawEventQueue, mRecordQueue, NetDataHandler(), 1);

    LOG_INFO(sLogger, ("begin to start ebpf ... ", ""));
    this->mFlag = true;
    this->RunInThread();
    return 0;
}

int NetworkObserverManager::StartAggregator() {
    return 0;
}
int NetworkObserverManager::StopAggregator() {
    return 0;
}

void NetworkObserverManager::RunInThread() {
    // periodically poll perf buffer ...
    LOG_INFO(sLogger, ("enter core thread ", ""));
    // start a new thread to poll perf buffer ...
    mCoreThread = std::thread(&NetworkObserverManager::PollBufferWrapper, this);
    mRecordConsume = std::thread(&NetworkObserverManager::ConsumeRecords, this);

    LOG_INFO(sLogger, ("network observer plugin installed.", ""));
}

void NetworkObserverManager::ConsumeRecordsAsTrace(const std::vector<std::shared_ptr<AbstractRecord>>& records,
                                                   size_t count) {
    for (size_t i = 0; i < count; i++) {
        auto record = records[i];
        // handle app record ...
        std::shared_ptr<AbstractAppRecord> appRecord = std::dynamic_pointer_cast<AbstractAppRecord>(record);
        if (appRecord != nullptr) {
            // generate traceId
            auto spanId = GenerateSpanID();
            if (!mSampler->ShouldSample(spanId)) {
                LOG_DEBUG(sLogger, ("sampler", "reject"));
                continue;
            }
            auto traceId = GenerateTraceID();
            LOG_DEBUG(sLogger, ("spanId", FromSpanId(spanId))("traceId", FromTraceId(traceId)));

            auto conn_id = appRecord->GetConnId();
            auto conn_tracker = mConnTrackerMgr->GetConntracker(conn_id);
            if (conn_tracker && !conn_tracker->MetaAttachReadyForApp()) {
                LOG_WARNING(sLogger,
                            ("app meta not ready, rollback app record, times",
                             record->Rollback())("pid", conn_id.tgid)("fd", conn_id.fd)("start", conn_id.start));
                // TODO @qianlu.kk we need rollback logic ...
                // mRecordQueue.enqueue(std::move(record));
                continue;
            }

            auto res = mSpanAggregator->Aggregate(appRecord, GenerateAggKeyForSpan(appRecord));
            LOG_DEBUG(sLogger, ("agg res", res));

            // Aggregator::GetInstance().Aggregate(appRecord);

            continue;
        }
    }
}

void NetworkObserverManager::ConsumeRecordsAsMetric(const std::vector<std::shared_ptr<AbstractRecord>>& records,
                                                    size_t count) {
    for (size_t i = 0; i < count; i++) {
        auto record = records[i];
        // handle app record ...
        std::shared_ptr<AbstractAppRecord> appRecord = std::dynamic_pointer_cast<AbstractAppRecord>(record);
        if (appRecord != nullptr) {
            auto conn_id = appRecord->GetConnId();
            auto conn_tracker = mConnTrackerMgr->GetConntracker(conn_id);
            if (conn_tracker && !conn_tracker->MetaAttachReadyForApp()) {
                LOG_WARNING(sLogger,
                            ("app meta not ready, rollback app record, times",
                             record->Rollback())("pid", conn_id.tgid)("fd", conn_id.fd)("start", conn_id.start));
                // TODO @qianlu.kk we need rollback logic ...
                // mRecordQueue.enqueue(std::move(record));
                continue;
            }

            LOG_WARNING(sLogger,
                        ("app meta ready, rollback:",
                         record->Rollback())("pid", conn_id.tgid)("fd", conn_id.fd)("start", conn_id.start));

            auto res = mAppAggregator->Aggregate(appRecord, GenerateAggKeyForAppMetric(appRecord));
            LOG_DEBUG(sLogger, ("agg res", res));
            continue;
        }

        std::shared_ptr<ConnStatsRecord> netRecord = std::dynamic_pointer_cast<ConnStatsRecord>(record);
        if (netRecord != nullptr) {
            auto conn_id = appRecord->GetConnId();
            auto conn_tracker = mConnTrackerMgr->GetConntracker(conn_id);
            if (conn_tracker && !conn_tracker->MetaAttachReadyForNet()) {
                // roll back
                LOG_WARNING(sLogger,
                            ("net meta not ready, rollback app record, times",
                             record->Rollback())("pid", conn_id.tgid)("fd", conn_id.fd)("start", conn_id.start));
                mRecordQueue.enqueue(std::move(record));
                continue;
            }
            // TODO @qianlu.kk we need converge logic ...
            // ConvergeAbstractRecord(netRecord);
            // Aggregator::GetInstance().Aggregate(netRecord);
            continue;
        }
    }

    // flush ...
}

void NetworkObserverManager::ConsumeRecordsAsEvent(const std::vector<std::shared_ptr<AbstractRecord>>& records,
                                                   size_t count) {
    if (count == 0)
        return;
    static const int32_t sbucket = 100;
    auto bucket_count = (count / sbucket) + 1;
    auto events = std::vector<std::unique_ptr<ApplicationBatchEvent>>(bucket_count);
    int nn = 0;
    for (size_t n = 0; n < bucket_count; n++) {
        auto batch_events = std::make_unique<ApplicationBatchEvent>();
        for (size_t j = 0; j < sbucket; j++) {
            size_t i = n * sbucket + j;
            if (i >= count)
                break;
            std::shared_ptr<HttpRecord> http_record = std::dynamic_pointer_cast<HttpRecord>(records[i]);
            if (http_record == nullptr)
                continue;

            auto conn_id = http_record->GetConnId();
            auto common_attrs = mConnTrackerMgr->GetConnTrackerAttrs(conn_id);
            std::string app_id = std::to_string(conn_id.tgid);

            auto single_event = std::make_unique<SingleEvent>();
            for (size_t i = 0; i < kConnTrackerElementsTableSize; i++) {
                if (kConnTrackerTable.ColLogKey(i) == "" || common_attrs[i] == "") {
                    continue;
                }

                single_event->AppendTags({std::string(kConnTrackerTable.ColLogKey(i)), common_attrs[i]});
            }
            // set time stamp
            auto start_ts_ns = http_record->GetStartTimeStamp();
            single_event->SetTimestamp(start_ts_ns + mTimeDiff.count());

            single_event->AppendTags({"latency", std::to_string(http_record->GetLatencyMs())});
            single_event->AppendTags({"http.method", http_record->GetMethod()});
            single_event->AppendTags({"http.path", http_record->GetPath()});
            single_event->AppendTags({"http.protocol", http_record->GetProtocolVersion()});
            single_event->AppendTags({"http.latency", std::to_string(http_record->GetLatencyMs())});
            single_event->AppendTags({"http.status_code", std::to_string(http_record->GetStatusCode())});
            single_event->AppendTags({"http.request.body", http_record->GetReqBody()});
            single_event->AppendTags({"http.response.body", http_record->GetRespBody()});
            for (auto& h : http_record->GetReqHeaderMap()) {
                single_event->AppendTags({"http.request.header." + h.first, h.second});
            }
            for (auto& h : http_record->GetRespHeaderMap()) {
                single_event->AppendTags({"http.response.header." + h.first, h.second});
            }
            for (auto& tag : single_event->GetAllTags()) {
                LOG_DEBUG(sLogger, ("tag key", tag.first)("tag value", tag.second));
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
        std::lock_guard lk(mContextMutex);
        // TODO @qianlu.kk we need flush records like security plugins

        LOG_DEBUG(sLogger,
                  ("call event cb, total event count",
                   count)("bucket count", bucket_count)("events size", events.size())("real size", nn));
        // send data ...
        //   std::shared_lock<std::shared_mutex> lock(event_cb_mutex_);
        //   if (event_cb_) {
        //     LOG(WARNING) << "call event cb, total event:" << count << " bucket_count:" << bucket_count << " events
        //     size:" << events.size() << " real size:" << nn; event_cb_(events); UpdatePushEventTotal(count);
        //   }
    }
}

// TODO @qianlu.kk
void NetworkObserverManager::ConsumeRecords() {
    std::vector<std::shared_ptr<AbstractRecord>> items(1024);
    while (mFlag) {
        // poll event from
        auto now = std::chrono::steady_clock::now();
        auto next_window = mConsumerFreqMgr.Next();
        if (!mConsumerFreqMgr.Expired(now)) {
            std::this_thread::sleep_until(next_window);
            mConsumerFreqMgr.Reset(next_window);
        } else {
            mConsumerFreqMgr.Reset(now);
        }
        size_t count = mRecordQueue.wait_dequeue_bulk_timed(items.data(), 1024, std::chrono::milliseconds(200));
        LOG_DEBUG(sLogger, ("get records:", count));
        // handle ....
        if (count == 0) {
            continue;
        }

        // TODO ConvergeAbstractRecord(appRecord);
        if (mEnableLog) {
            ConsumeRecordsAsEvent(items, count);
        }
        if (mEnableMetric) {
            // do converge ...
            // aggregate ...
            ConsumeRecordsAsMetric(items, count);
        }
        if (mEnableSpan) {
            ConsumeRecordsAsTrace(items, count);
        }

        items.clear();
        items.resize(1024);
    }
}


void NetworkObserverManager::PollBufferWrapper() {
    int32_t flag = 0;
    int cnt = 0;
    while (this->mFlag) {
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
                  ("===== statistic =====>> total data events:",
                   mRecvHttpDataEventsTotal_.load())(" total conn stats events:", mRecvConnStatEventsTotal_.load())(
                      " total ctrl events:", mRecvCtrlEventsTotal_.load())(" lost data events:",
                                                                           mLostDataEventsTotal_.load())(
                      " lost stats events:", mLostConnStatEventsTotal_.load())(" lost ctrl events:",
                                                                               mLostCtrlEventsTotal_.load()));
    }
}

void NetworkObserverManager::RecordEventLost(enum callback_type_e type, uint64_t lost_count) {
    switch (type) {
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

void NetworkObserverManager::AcceptNetStatsEvent(struct conn_stats_event_t* event) {
    LOG_DEBUG(
        sLogger,
        ("[DUMP] stats event handle, fd", event->conn_id.fd)("pid", event->conn_id.tgid)("start", event->conn_id.start)(
            "role", int(event->role))("state", int(event->conn_events))("eventTs", event->ts));
    mConnTrackerMgr->AcceptNetStatsEvent(event);
}

void NetworkObserverManager::AcceptNetCtrlEvent(struct conn_ctrl_event_t* event) {
    LOG_DEBUG(sLogger,
              ("[DUMP] ctrl event handle, fd", event->conn_id.fd)("pid", event->conn_id.tgid)(
                  "start", event->conn_id.start)("type", int(event->type))("eventTs", event->ts));
    mConnTrackerMgr->AcceptNetCtrlEvent(event);
}


void NetworkObserverManager::Stop() {
    LOG_INFO(sLogger, ("prepare to destroy", ""));
    mSourceManager->StopPlugin(PluginType::NETWORK_OBSERVE);
    LOG_INFO(sLogger, ("destroy stage", "shutdown ebpf prog"));
    mConnTrackerMgr->Stop();
    LOG_INFO(sLogger, ("destroy stage", "stop conn tracker"));
    this->mFlag = false;

    if (this->mCoreThread.joinable()) {
        this->mCoreThread.join();
    }
    LOG_INFO(sLogger, ("destroy stage", "release core thread"));

    if (this->mRecordConsume.joinable()) {
        this->mRecordConsume.join();
    }
    LOG_INFO(sLogger, ("destroy stage", "release consumer thread"));
}

int NetworkObserverManager::Destroy() {
    Stop();
    return 0;
}

void NetworkObserverManager::UpdateWhitelists(std::vector<std::string>&& enableCids,
                                              std::vector<std::string>&& disableCids) {
    for (auto& cid : enableCids) {
        LOG_INFO(sLogger, ("UpdateWhitelists cid", cid));
        mSourceManager->SetNetworkObserverCidFilter(cid, true);
    }

    for (auto& cid : disableCids) {
        LOG_INFO(sLogger, ("UpdateBlacklists cid", cid));
        mSourceManager->SetNetworkObserverCidFilter(cid, false);
    }
}

} // namespace ebpf
} // namespace logtail

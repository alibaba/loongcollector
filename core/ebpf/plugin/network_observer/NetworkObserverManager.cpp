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

#include <random>

#include "collection_pipeline/queue/ProcessQueueItem.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/magic_enum.hpp"
#include "ebpf/Config.h"
#include "ebpf/include/export.h"
#include "ebpf/type/PeriodicalEvent.h"
#include "ebpf/util/TraceId.h"
#include "logger/Logger.h"
#include "models/StringView.h"

extern "C" {
#include <net.h>
}

namespace logtail {
namespace ebpf {

NetworkObserverManager::NetworkObserverManager(std::shared_ptr<BaseManager>& baseMgr,
                                               std::shared_ptr<SourceManager> sourceManager,
                                               moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                                               std::shared_ptr<Timer> scheduler)
    : AbstractManager(baseMgr, sourceManager, queue, scheduler),
      mAppAggregator(
          4096,
          [this](std::unique_ptr<AppMetricData>& base, const std::shared_ptr<AbstractAppRecord>& other) {
              base->m2xxCount += other->GetStatusCode() / 100 == 2;
              base->m3xxCount += other->GetStatusCode() / 100 == 3;
              base->m4xxCount += other->GetStatusCode() / 100 == 4;
              base->m5xxCount += other->GetStatusCode() / 100 == 5;
              base->mCount++;
              base->mErrCount += other->IsError();
              base->mSlowCount += other->IsSlow();
              base->mSum += double(other->GetLatencyMs() / 1000);
          },
          [this](const std::shared_ptr<AbstractAppRecord>& in) {
              auto data = std::make_unique<AppMetricData>(in->GetConnId(), in->GetSpanName());
              auto ctAttrs = this->mConnTrackerMgr->GetConnTrackerAttrs(in->GetConnId());

              data->mAppId = ctAttrs[kConnTrackerTable.ColIndex(kAppId.name())];
              data->mAppName = ctAttrs[kConnTrackerTable.ColIndex(kAppName.name())];
              data->mHost = ctAttrs[kConnTrackerTable.ColIndex(kHost.name())];
              data->mIp = ctAttrs[kConnTrackerTable.ColIndex(kIp.name())];

              data->mWorkloadKind = ctAttrs[kConnTrackerTable.ColIndex(kWorkloadKind.name())];
              data->mWorkloadName = ctAttrs[kConnTrackerTable.ColIndex(kWorkloadName.name())];

              data->mRpcType = ctAttrs[kConnTrackerTable.ColIndex(kRpcType.name())];
              data->mCallType = ctAttrs[kConnTrackerTable.ColIndex(kCallType.name())];
              data->mCallKind = ctAttrs[kConnTrackerTable.ColIndex(kCallKind.name())];

              data->mDestId = ctAttrs[kConnTrackerTable.ColIndex(kDestId.name())];
              data->mEndpoint = ctAttrs[kConnTrackerTable.ColIndex(kEndpoint.name())];
              data->mNamespace = ctAttrs[kConnTrackerTable.ColIndex(kNamespace.name())];

              return data;
          }),
      mNetAggregator(
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
          }),
      mSpanAggregator(
          4096,
          [this](std::unique_ptr<AppSpanGroup>& base, const std::shared_ptr<AbstractAppRecord>& other) {
              base->mRecords.push_back(other);
          },
          [this](const std::shared_ptr<AbstractAppRecord>& in) { return std::make_unique<AppSpanGroup>(); }),
      mLogAggregator(
          4096,
          [this](std::unique_ptr<AppLogGroup>& base, const std::shared_ptr<AbstractAppRecord>& other) {
              base->mRecords.push_back(other);
          },
          [this](const std::shared_ptr<AbstractAppRecord>& in) { return std::make_unique<AppLogGroup>(); }) {
}

// done
std::array<size_t, 2>
NetworkObserverManager::GenerateAggKeyForAppMetric(const std::shared_ptr<AbstractAppRecord> record) {
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
            } else if (kAppMetricsTable.elements()[j].name() == kRpc.name()) {
                // record dedicated ...
                attr = record->GetSpanName();
            } else {
                LOG_WARNING(sLogger, ("unknown elements", kAppMetricsTable.elements()[j].name()));
                continue;
            }

            int hash_result_index = agg_level - MinAggregationLevel;
            hash_result[hash_result_index] ^= hasher(attr) + 0x9e3779b9 + (hash_result[hash_result_index] << 6)
                + (hash_result[hash_result_index] >> 2);
        }
    }

    return hash_result;
}

std::array<size_t, 1> NetworkObserverManager::GenerateAggKeyForSpan(const std::shared_ptr<AbstractAppRecord> record) {
    // calculate agg key
    // just appid
    std::array<size_t, 1> hash_result;
    hash_result.fill(0UL);
    std::hash<std::string> hasher;
    auto connTrackerAttrs = mConnTrackerMgr->GetConnTrackerAttrs(record->GetConnId());
    auto elements = {kAppId, kIp, kHost};
    for (auto& x : elements) {
        auto attr = connTrackerAttrs[kConnTrackerTable.ColIndex(x.name())];
        hash_result[0] ^= hasher(attr) + 0x9e3779b9 + (hash_result[0] << 6) + (hash_result[0] >> 2);
    }

    return hash_result;
}

std::array<size_t, 1> NetworkObserverManager::GenerateAggKeyForLog(const std::shared_ptr<AbstractAppRecord> record) {
    // just appid
    std::array<size_t, 1> hash_result;
    hash_result.fill(0UL);
    std::hash<uint64_t> hasher;
    auto connId = record->GetConnId();

    std::array<uint64_t, 3> elements = {uint64_t(connId.fd), uint64_t(connId.tgid), connId.start};
    for (auto& x : elements) {
        hash_result[0] ^= hasher(x) + 0x9e3779b9 + (hash_result[0] << 6) + (hash_result[0] >> 2);
    }

    return hash_result;
}

void NetworkObserverManager::EnqueueDataEvent(std::unique_ptr<NetDataEvent> data_event) const {
    mRecvHttpDataEventsTotal.fetch_add(1);
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

bool NetworkObserverManager::ConsumeLogAggregateTree(const std::chrono::steady_clock::time_point& execTime) { // handler
    if (!this->mFlag || this->mSuspendFlag) {
        return false;
    }

    WriteLock lk(mLogAggLock);
    SIZETAggTree<AppLogGroup, std::shared_ptr<AbstractAppRecord>> aggTree(this->mLogAggregator.GetRootNodeAndClear());
    lk.unlock();

    auto nodes = aggTree.GetNodesWithAggDepth(1);
    LOG_DEBUG(sLogger, ("enter aggregator ...", nodes.size()));
    if (nodes.empty()) {
        LOG_DEBUG(sLogger, ("empty nodes...", ""));
        return true;
    }

    for (auto& node : nodes) {
        // convert to a item and push to process queue
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer); // per node represent an APP ...
        bool init = false;
        bool needPush = false;
        aggTree.ForEach(node, [&](const AppLogGroup* group) {
            // set process tag
            if (group->mRecords.empty()) {
                LOG_DEBUG(sLogger, ("", "no records .."));
                return;
            }
            std::array<StringView, kConnTrackerElementsTableSize> ctAttrVal;
            for (auto record : group->mRecords) {
                if (!init) {
                    auto ct = this->mConnTrackerMgr->GetConntracker(record->GetConnId());
                    auto ctAttrs = this->mConnTrackerMgr->GetConnTrackerAttrs(record->GetConnId());
                    if (ct == nullptr) {
                        LOG_DEBUG(sLogger, ("ct is null, skip, spanname ", record->GetSpanName()));
                        continue;
                    }
                    // set conn tracker attrs ...
                    eventGroup.SetTag(std::string("service.name"),
                                      ctAttrs[kConnTrackerTable.ColIndex(kAppName.name())]); // app name
                    eventGroup.SetTag(std::string("arms.appId"),
                                      ctAttrs[kConnTrackerTable.ColIndex(kAppId.name())]); // app id
                    eventGroup.SetTag(std::string("host.ip"),
                                      ctAttrs[kConnTrackerTable.ColIndex(kPodIp.name())]); // pod ip
                    eventGroup.SetTag(std::string("host.name"),
                                      ctAttrs[kConnTrackerTable.ColIndex(kPodName.name())]); // pod name
                    eventGroup.SetTag(std::string("arms.app.type"), "ebpf"); //
                    eventGroup.SetTag(std::string("data_type"), "trace");
                    for (auto tag = eventGroup.GetTags().begin(); tag != eventGroup.GetTags().end(); tag++) {
                        LOG_DEBUG(sLogger, ("record span tags", "")(std::string(tag->first), std::string(tag->second)));
                    }

                    for (size_t i = 0; i < kConnTrackerElementsTableSize; i++) {
                        auto sb = sourceBuffer->CopyString(ctAttrs[i]);
                        ctAttrVal[i] = StringView(sb.data, sb.size);
                    }

                    init = true;
                }
                auto* logEvent = eventGroup.AddLogEvent();
                for (size_t i = 0; i < kConnTrackerElementsTableSize; i++) {
                    if (kConnTrackerTable.ColLogKey(i) == "" || ctAttrVal[i] == "") {
                        continue;
                    }
                    logEvent->SetContentNoCopy(kConnTrackerTable.ColLogKey(i), ctAttrVal[i]);
                }
                // set time stamp
                std::shared_ptr<HttpRecord> httpRecord = std::dynamic_pointer_cast<HttpRecord>(record);
                auto ts = httpRecord->GetStartTimeStamp();
                logEvent->SetTimestamp(ts + mTimeDiff.count());
                logEvent->SetContent("latency", std::to_string(httpRecord->GetLatencyMs()));
                logEvent->SetContent("http.method", httpRecord->GetMethod());
                logEvent->SetContent("http.path", httpRecord->GetPath());
                logEvent->SetContent("http.protocol", httpRecord->GetProtocolVersion());
                logEvent->SetContent("http.latency", std::to_string(httpRecord->GetLatencyMs()));
                logEvent->SetContent("http.status_code", std::to_string(httpRecord->GetStatusCode()));
                logEvent->SetContent("http.request.body", httpRecord->GetReqBody());
                logEvent->SetContent("http.response.body", httpRecord->GetRespBody());

                for (auto& h : httpRecord->GetReqHeaderMap()) {
                    logEvent->SetContent("http.request.header." + h.first, h.second);
                }
                for (auto& h : httpRecord->GetRespHeaderMap()) {
                    logEvent->SetContent("http.response.header." + h.first, h.second);
                }

                LOG_DEBUG(sLogger, ("add one log, log timestamp", ts));
                needPush = true;
            }
        });
        if (init && needPush) {
            std::lock_guard lk(mContextMutex);
            if (this->mPipelineCtx == nullptr) {
                return true;
            }
            auto eventSize = eventGroup.GetEvents().size();
            LOG_DEBUG(sLogger, ("event group size", eventGroup.GetEvents().size()));
            std::unique_ptr<ProcessQueueItem> item
                = std::make_unique<ProcessQueueItem>(std::move(eventGroup), this->mPluginIndex);
            if (QueueStatus::OK != ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
                LOG_WARNING(sLogger,
                            ("configName", mPipelineCtx->GetConfigName())("pluginIdx", this->mPluginIndex)(
                                "[NetworkObserver] push span to queue failed!", ""));
            } else {
                LOG_DEBUG(sLogger, ("NetworkObserver push span successful, events:", eventSize));
            }
        } else {
            LOG_DEBUG(sLogger, ("NetworkObserver skip push span ", ""));
        }
    }
    aggTree.Clear();

    return true;
}

bool NetworkObserverManager::ConsumeMetricAggregateTree(
    const std::chrono::steady_clock::time_point& execTime) { // handler
    if (!this->mFlag || this->mSuspendFlag) {
        return false;
    }

    WriteLock lk(this->mAppAggLock);
    SIZETAggTree<AppMetricData, std::shared_ptr<AbstractAppRecord>> aggTree(this->mAppAggregator.GetRootNodeAndClear());
    lk.unlock();

    auto nodes = aggTree.GetNodesWithAggDepth(1);
    LOG_DEBUG(sLogger, ("enter aggregator ...", nodes.size()));
    if (nodes.empty()) {
        LOG_DEBUG(sLogger, ("empty nodes...", ""));
        return true;
    }

    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

    for (auto& node : nodes) {
        LOG_DEBUG(sLogger, ("node child size", node->child.size()));
        // convert to a item and push to process queue
        // every node represent an instance of an arms app ...
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer); // per node represent an APP ...
        eventGroup.SetTag(std::string("source"), "ebpf");
        eventGroup.SetTag(std::string("data_type"), "metric");
        eventGroup.SetTag(std::string("qianlu_tag"), "v3");

        bool needPush = false;

        bool init = false;
        aggTree.ForEach(node, [&](const AppMetricData* group) {
            // instance dim
            if (group->mAppId.size()) {
                needPush = true;
            }

            auto workloadKind = sourceBuffer->CopyString(group->mWorkloadKind);
            auto workloadName = sourceBuffer->CopyString(group->mWorkloadName);

            auto rpcType = sourceBuffer->CopyString(group->mRpcType);
            auto callType = sourceBuffer->CopyString(group->mCallType);
            auto callKind = sourceBuffer->CopyString(group->mCallKind);

            if (!init) {
                // set app attrs ...
                eventGroup.SetTag(std::string(kAppId.metric_key()), group->mAppId); // app id
                eventGroup.SetTag(std::string(kIp.metric_key()), group->mIp); // pod ip
                eventGroup.SetTag(std::string(kAppName.metric_key()), group->mAppName); // app name
                eventGroup.SetTag(std::string(kHost.metric_key()), group->mHost); // pod name

                auto* tagMetric = eventGroup.AddMetricEvent();
                tagMetric->SetName("arms_tag_entity");
                tagMetric->SetValue(UntypedSingleValue{1.0});
                tagMetric->SetTimestamp(seconds, 0);
                tagMetric->SetTag("agentVersion", std::string("v1"));
                tagMetric->SetTag("app", group->mAppName); // app ===> appname
                tagMetric->SetTag("resourceid", group->mAppId); // resourceid -==> pid
                tagMetric->SetTag("resourcetype", std::string("APPLICATION")); // resourcetype ===> APPLICATION
                tagMetric->SetTag("version", std::string("v1")); // version ===> v1
                tagMetric->SetTag(
                    "clusterId",
                    std::string("c0748d004a7ce431d8da62ed8f6134879")); // clusterId ===> TODO read from env _cluster_id_
                tagMetric->SetTag("host", group->mIp); // host ===>
                tagMetric->SetTag("hostname", group->mHost); // hostName ===>
                tagMetric->SetTag("namespace", group->mNamespace); // namespace ===>
                tagMetric->SetTag("workloadKind", group->mWorkloadKind); // workloadKind ===>
                tagMetric->SetTag("workloadName", group->mWorkloadName); // workloadName ===>
                init = true;
            }

            LOG_DEBUG(sLogger,
                      ("node app", group->mAppName)("group span", group->mSpanName)("node size", nodes.size())(
                          "rpcType", group->mRpcType)("callType", group->mCallType)("callKind", group->mCallKind)(
                          "appName", group->mAppName)("appId", group->mAppId)("host", group->mHost)("ip", group->mIp)(
                          "namespace", group->mNamespace)("wk", group->mWorkloadKind)("wn", group->mWorkloadName)(
                          "reqCnt", group->mCount)("latencySum", group->mSum)("errCnt", group->mErrCount)(
                          "slowCnt", group->mSlowCount));

            std::vector<MetricEvent*> metrics;
            if (group->mCount) {
                auto* requestsMetric = eventGroup.AddMetricEvent();
                requestsMetric->SetName("arms_rpc_requests_count");
                requestsMetric->SetValue(UntypedSingleValue{double(group->mCount)});
                metrics.push_back(requestsMetric);

                auto* latencyMetric = eventGroup.AddMetricEvent();
                latencyMetric->SetName("arms_rpc_requests_seconds");
                latencyMetric->SetValue(UntypedSingleValue{double(group->mSum)});
                metrics.push_back(latencyMetric);
            }
            if (group->mErrCount) {
                auto* errorMetric = eventGroup.AddMetricEvent();
                errorMetric->SetName("arms_rpc_requests_error_count");
                errorMetric->SetValue(UntypedSingleValue{double(group->mErrCount)});
                metrics.push_back(errorMetric);
            }
            if (group->mSlowCount) {
                auto* slowMetric = eventGroup.AddMetricEvent();
                slowMetric->SetName("arms_rpc_requests_slow_count");
                slowMetric->SetValue(UntypedSingleValue{double(group->mSlowCount)});
                metrics.push_back(slowMetric);
            }

            if (group->m2xxCount) {
                auto* statusMetric = eventGroup.AddMetricEvent();
                statusMetric->SetValue(UntypedSingleValue{double(group->m2xxCount)});
                statusMetric->SetName("arms_rpc_requests_by_status_count");
                statusMetric->SetTag("status", std::string("2xx"));
                metrics.push_back(statusMetric);
            }
            if (group->m3xxCount) {
                auto* statusMetric = eventGroup.AddMetricEvent();
                statusMetric->SetValue(UntypedSingleValue{double(group->m3xxCount)});
                statusMetric->SetName("arms_rpc_requests_by_status_count");
                statusMetric->SetTag("status", std::string("3xx"));
                metrics.push_back(statusMetric);
            }
            if (group->m4xxCount) {
                auto* statusMetric = eventGroup.AddMetricEvent();
                statusMetric->SetValue(UntypedSingleValue{double(group->m4xxCount)});
                statusMetric->SetName("arms_rpc_requests_by_status_count");
                statusMetric->SetTag("status", std::string("4xx"));
                metrics.push_back(statusMetric);
            }
            if (group->m5xxCount) {
                auto* statusMetric = eventGroup.AddMetricEvent();
                statusMetric->SetValue(UntypedSingleValue{double(group->m5xxCount)});
                statusMetric->SetName("arms_rpc_requests_by_status_count");
                statusMetric->SetTag("status", std::string("5xx"));
                metrics.push_back(statusMetric);
            }

            auto rpc = sourceBuffer->CopyString(group->mSpanName);

            auto destId = sourceBuffer->CopyString(group->mDestId);
            auto endpoint = sourceBuffer->CopyString(group->mEndpoint);
            for (auto* metricsEvent : metrics) {
                // set tags
                metricsEvent->SetTimestamp(seconds, 0);
                metricsEvent->SetTagNoCopy(kWorkloadName.metric_key(),
                                           StringView(workloadName.data, workloadName.size));
                metricsEvent->SetTagNoCopy(kWorkloadKind.metric_key(),
                                           StringView(workloadKind.data, workloadKind.size));
                metricsEvent->SetTagNoCopy(kRpc.metric_key(), StringView(rpc.data, rpc.size));
                metricsEvent->SetTagNoCopy(kRpcType.metric_key(), StringView(rpcType.data, rpcType.size));
                metricsEvent->SetTagNoCopy(kCallType.metric_key(), StringView(callType.data, callType.size));
                metricsEvent->SetTagNoCopy(kCallKind.metric_key(), StringView(callKind.data, callKind.size));
                metricsEvent->SetTagNoCopy(kEndpoint.metric_key(), StringView(endpoint.data, endpoint.size));
                metricsEvent->SetTagNoCopy(kDestId.metric_key(), StringView(destId.data, destId.size));
            }
        });
        if (needPush) {
            std::lock_guard lk(mContextMutex);
            if (this->mPipelineCtx == nullptr) {
                return true;
            }
            auto eventSize = eventGroup.GetEvents().size();
            LOG_DEBUG(sLogger, ("event group size", eventGroup.GetEvents().size()));
            std::unique_ptr<ProcessQueueItem> item
                = std::make_unique<ProcessQueueItem>(std::move(eventGroup), this->mPluginIndex);

            if (QueueStatus::OK != ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
                LOG_WARNING(sLogger,
                            ("configName", mPipelineCtx->GetConfigName())("pluginIdx", this->mPluginIndex)(
                                "[NetworkObserver] push queue failed!", ""));
            } else {
                LOG_DEBUG(sLogger, ("NetworkObserver push metric successful, events:", eventSize));
            }
        } else {
            LOG_DEBUG(sLogger, ("appid is empty, no need to push", ""));
        }
    }

    aggTree.Clear();

    return true;
}

bool NetworkObserverManager::ConsumeSpanAggregateTree(
    const std::chrono::steady_clock::time_point& execTime) { // handler
    if (!this->mFlag || this->mSuspendFlag) {
        return false;
    }

    WriteLock lk(mSpanAggLock);
    SIZETAggTree<AppSpanGroup, std::shared_ptr<AbstractAppRecord>> aggTree(this->mSpanAggregator.GetRootNodeAndClear());
    lk.unlock();

    auto nodes = aggTree.GetNodesWithAggDepth(1);
    LOG_DEBUG(sLogger, ("enter aggregator ...", nodes.size()));
    if (nodes.empty()) {
        LOG_DEBUG(sLogger, ("empty nodes...", ""));
        return true;
    }

    for (auto& node : nodes) {
        // convert to a item and push to process queue
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer); // per node represent an APP ...
        bool init = false;
        bool needPush = false;
        aggTree.ForEach(node, [&](const AppSpanGroup* group) {
            // set process tag
            if (group->mRecords.empty()) {
                LOG_DEBUG(sLogger, ("", "no records .."));
                return;
            }
            for (auto record : group->mRecords) {
                auto ct = this->mConnTrackerMgr->GetConntracker(record->GetConnId());
                auto ctAttrs = this->mConnTrackerMgr->GetConnTrackerAttrs(record->GetConnId());
                auto appname = ctAttrs[kConnTrackerTable.ColIndex(kAppName.name())];
                if (appname.empty() || ct == nullptr) {
                    LOG_DEBUG(sLogger,
                              ("no app name or ct null, skip, spanname ",
                               record->GetSpanName())("appname", appname)("ct null", ct == nullptr));
                    continue;
                }
                if (!init) {
                    // set app attrs ...
                    eventGroup.SetTag(std::string("service.name"),
                                      ctAttrs[kConnTrackerTable.ColIndex(kAppName.name())]); // app name
                    eventGroup.SetTag(std::string("arms.appId"),
                                      ctAttrs[kConnTrackerTable.ColIndex(kAppId.name())]); // app id
                    eventGroup.SetTag(std::string("host.ip"),
                                      ctAttrs[kConnTrackerTable.ColIndex(kPodIp.name())]); // pod ip
                    eventGroup.SetTag(std::string("host.name"),
                                      ctAttrs[kConnTrackerTable.ColIndex(kPodName.name())]); // pod name
                    eventGroup.SetTag(std::string("arms.app.type"), "ebpf"); //
                    eventGroup.SetTag(std::string("data_type"), "trace");
                    for (auto tag = eventGroup.GetTags().begin(); tag != eventGroup.GetTags().end(); tag++) {
                        LOG_DEBUG(sLogger, ("record span tags", "")(std::string(tag->first), std::string(tag->second)));
                    }
                    init = true;
                }
                auto* spanEvent = eventGroup.AddSpanEvent();
                spanEvent->SetTag("app", ctAttrs[kConnTrackerTable.ColIndex(kWorkloadName.name())]);
                spanEvent->SetTag("host", ctAttrs[kConnTrackerTable.ColIndex(kHost.name())]);
                for (auto element : kAppTraceTable.elements()) {
                    auto sb = sourceBuffer->CopyString(ctAttrs[kConnTrackerTable.ColIndex(element.name())]);
                    spanEvent->SetTagNoCopy(element.span_key(), StringView(sb.data, sb.size));
                    LOG_DEBUG(sLogger, ("record span tags", "")(std::string(element.span_key()), sb.data));
                }
                spanEvent->SetTraceId(record->mTraceId);
                spanEvent->SetSpanId(record->mSpanId);
                spanEvent->SetStatus(record->IsError() ? SpanEvent::StatusCode::Error : SpanEvent::StatusCode::Ok);
                auto role = ct->GetRole();
                if (role == support_role_e::IsClient) {
                    spanEvent->SetKind(SpanEvent::Kind::Client);
                } else if (role == support_role_e::IsServer) {
                    spanEvent->SetKind(SpanEvent::Kind::Server);
                } else {
                    spanEvent->SetKind(SpanEvent::Kind::Unspecified);
                }

                auto now = std::chrono::system_clock::now();
                auto duration = now.time_since_epoch();
                auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

                spanEvent->SetName(record->GetSpanName());
                spanEvent->SetTag(std::string("req.body"), record->GetReqBody());
                spanEvent->SetTag(std::string("resp.body"), record->GetRespBody());
                spanEvent->SetTag(std::string("protocol.version"), record->GetProtocolVersion());

                auto startTime = record->GetStartTimeStamp() + this->mTimeDiff.count();
                spanEvent->SetStartTimeNs(startTime);
                auto endTime = record->GetEndTimeStamp() + this->mTimeDiff.count();
                spanEvent->SetEndTimeNs(endTime);
                spanEvent->SetTimestamp(seconds);
                LOG_DEBUG(sLogger, ("add one span, startTs", startTime)("entTs", endTime));
                needPush = true;
            }
        });
        if (init && needPush) {
            std::lock_guard lk(mContextMutex);
            if (this->mPipelineCtx == nullptr) {
                return true;
            }
            auto eventSize = eventGroup.GetEvents().size();
            LOG_DEBUG(sLogger, ("event group size", eventGroup.GetEvents().size()));
            std::unique_ptr<ProcessQueueItem> item
                = std::make_unique<ProcessQueueItem>(std::move(eventGroup), this->mPluginIndex);
            if (QueueStatus::OK != ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
                LOG_WARNING(sLogger,
                            ("configName", mPipelineCtx->GetConfigName())("pluginIdx", this->mPluginIndex)(
                                "[NetworkObserver] push span to queue failed!", ""));
            } else {
                LOG_DEBUG(sLogger, ("NetworkObserver push span successful, events:", eventSize));
            }
        } else {
            LOG_DEBUG(sLogger, ("NetworkObserver skip push span ", ""));
        }
    }
    aggTree.Clear();

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

    mFlag = true;
    mStartUid++;

    // start conntracker ...
    mConnTrackerMgr = ConnTrackerManager::Create();
    mConnTrackerMgr->Start();

    mPollKernelFreqMgr.SetPeriod(std::chrono::milliseconds(200));
    mConsumerFreqMgr.SetPeriod(std::chrono::milliseconds(200));

    // TODO @qianlu.kk init converger later ...

    std::unique_ptr<AggregateEvent> appTraceEvent = std::make_unique<AggregateEvent>(
        1,
        [this](const std::chrono::steady_clock::time_point& execTime) {
            return this->ConsumeSpanAggregateTree(execTime);
        },
        [this](int currentUid) { // validator
            auto isStop = !this->mFlag.load() || currentUid != this->mStartUid;
            if (isStop) {
                LOG_INFO(sLogger,
                         ("stop schedule, mflag", this->mFlag)("currentUid", currentUid)("pluginUid", this->mStartUid));
            }
            return isStop;
        },
        mStartUid);

    std::unique_ptr<AggregateEvent> appLogEvent = std::make_unique<AggregateEvent>(
        1,
        [this](const std::chrono::steady_clock::time_point& execTime) {
            return this->ConsumeLogAggregateTree(execTime);
        },
        [this](int currentUid) { // validator
            auto isStop = !this->mFlag.load() || currentUid != this->mStartUid;
            if (isStop) {
                LOG_INFO(sLogger,
                         ("stop schedule, mflag", this->mFlag)("currentUid", currentUid)("pluginUid", this->mStartUid));
            }
            return isStop;
        },
        mStartUid);

    std::unique_ptr<AggregateEvent> appMetricEvent = std::make_unique<AggregateEvent>(
        15,
        [this](const std::chrono::steady_clock::time_point& execTime) {
            return this->ConsumeMetricAggregateTree(execTime);
        },
        [this](int currentUid) { // validator
            auto isStop = !this->mFlag.load() || currentUid != this->mStartUid;
            if (isStop) {
                LOG_INFO(sLogger,
                         ("stop schedule, mflag", this->mFlag)("currentUid", currentUid)("pluginUid", this->mStartUid));
            }
            return isStop;
        },
        mStartUid);

    mScheduler->PushEvent(std::move(appMetricEvent));
    mScheduler->PushEvent(std::move(appTraceEvent));
    mScheduler->PushEvent(std::move(appLogEvent));
    // init sampler
    mSampler = std::make_unique<HashRatioSampler>(1);

    mEnableLog = opt->mEnableLog;
    mEnableSpan = opt->mEnableSpan;
    mEnableMetric = opt->mEnableMetric;

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
            mgr->mRecvConnStatEventsTotal.fetch_add(1);
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

        mgr->mRecvCtrlEventsTotal.fetch_add(1);
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

void NetworkObserverManager::RunInThread() {
    // periodically poll perf buffer ...
    LOG_INFO(sLogger, ("enter core thread ", ""));
    // start a new thread to poll perf buffer ...
    mCoreThread = std::thread(&NetworkObserverManager::PollBufferWrapper, this);
    mRecordConsume = std::thread(&NetworkObserverManager::ConsumeRecords, this);

    LOG_INFO(sLogger, ("network observer plugin installed.", ""));
}

void NetworkObserverManager::ConsumeRecordsAsTrace(std::vector<std::shared_ptr<AbstractRecord>>& records,
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
            appRecord->SetTraceId(FromTraceId(traceId));
            appRecord->SetSpanId(FromSpanId(spanId));
            auto conn_id = appRecord->GetConnId();
            auto conn_tracker = mConnTrackerMgr->GetConntracker(conn_id);
            if (conn_tracker && !conn_tracker->MetaAttachReadyForApp()) {
                LOG_WARNING(sLogger,
                            ("app meta not ready, rollback app record, times",
                             record->Rollback())("pid", conn_id.tgid)("fd", conn_id.fd)("start", conn_id.start));
                // TODO @qianlu.kk we need rollback logic ...
                continue;
            }

            {
                WriteLock lk(mSpanAggLock);
                auto res = mSpanAggregator.Aggregate(appRecord, GenerateAggKeyForSpan(appRecord));
                LOG_DEBUG(sLogger, ("agg res", res));
            }

            continue;
        }
    }
}

void NetworkObserverManager::ConsumeRecordsAsMetric(std::vector<std::shared_ptr<AbstractRecord>>& records,
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
            {
                WriteLock lk(mAppAggLock);
                auto res = mAppAggregator.Aggregate(appRecord, GenerateAggKeyForAppMetric(appRecord));
                LOG_DEBUG(sLogger, ("agg res", res));
            }
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
            // TODO @qianlu.kk we need converge logic later ...
            // ConvergeAbstractRecord(netRecord);

            continue;
        }
    }
}

void NetworkObserverManager::ConsumeRecordsAsEvent(std::vector<std::shared_ptr<AbstractRecord>>& records,
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
                continue;
            }

            {
                WriteLock lk(mLogAggLock);
                auto res = mLogAggregator.Aggregate(appRecord, GenerateAggKeyForLog(appRecord));
                LOG_DEBUG(sLogger, ("agg res", res));
            }

            continue;
        }
    }

    return;
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

        LOG_DEBUG(
            sLogger,
            ("===== statistic =====>> total data events:",
             mRecvHttpDataEventsTotal.load())(" total conn stats events:", mRecvConnStatEventsTotal.load())(
                " total ctrl events:", mRecvCtrlEventsTotal.load())(" lost data events:", mLostDataEventsTotal.load())(
                " lost stats events:", mLostConnStatEventsTotal.load())(" lost ctrl events:",
                                                                        mLostCtrlEventsTotal.load()));
    }
}

void NetworkObserverManager::RecordEventLost(enum callback_type_e type, uint64_t lost_count) {
    switch (type) {
        case STAT_HAND:
            mLostConnStatEventsTotal.fetch_add(lost_count);
            return;
        case INFO_HANDLE:
            mLostDataEventsTotal.fetch_add(lost_count);
            return;
        case CTRL_HAND:
            mLostCtrlEventsTotal.fetch_add(lost_count);
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

    // destroy worker pool ...
    mWorkerPool.reset();
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

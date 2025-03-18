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
#include "common/HashUtil.h"
#include "common/StringTools.h"
#include "common/magic_enum.hpp"
#include "ebpf/Config.h"
#include "ebpf/eBPFServer.h"
#include "ebpf/include/export.h"
#include "ebpf/protocol/ProtocolParser.h"
#include "ebpf/type/PeriodicalEvent.h"
#include "ebpf/util/TraceId.h"
#include "logger/Logger.h"
#include "metadata/K8sMetadata.h"
#include "models/StringView.h"

extern "C" {
#include <coolbpf/net.h>
}

namespace logtail {
namespace ebpf {

class eBPFServer;

inline constexpr int kNetObserverMaxBatchConsumeSize = 4096;
inline constexpr int kNetObserverMaxWaitTimeMS = 0;

static constexpr uint32_t APP_ID_INDEX = kConnTrackerTable.ColIndex(kAppId.Name());
static constexpr uint32_t APP_NAME_INDEX = kConnTrackerTable.ColIndex(kAppName.Name());
static constexpr uint32_t HOST_NAME_INDEX = kConnTrackerTable.ColIndex(kHostName.Name());
static constexpr uint32_t HOST_IP_INDEX = kConnTrackerTable.ColIndex(kIp.Name());

static constexpr uint32_t WORKLOAD_KIND_INDEX = kConnTrackerTable.ColIndex(kWorkloadKind.Name());
static constexpr uint32_t WORKLOAD_NAME_INDEX = kConnTrackerTable.ColIndex(kWorkloadName.Name());
static constexpr uint32_t NAMESPACE_INDEX = kConnTrackerTable.ColIndex(kNamespace.Name());

static constexpr uint32_t PEER_WORKLOAD_KIND_INDEX = kConnTrackerTable.ColIndex(kPeerWorkloadKind.Name());
static constexpr uint32_t PEER_WORKLOAD_NAME_INDEX = kConnTrackerTable.ColIndex(kPeerWorkloadName.Name());
static constexpr uint32_t PEER_NAMESPACE_INDEX = kConnTrackerTable.ColIndex(kPeerNamespace.Name());

NetworkObserverManager::NetworkObserverManager(std::shared_ptr<ProcessCacheManager>& baseMgr,
                                               std::shared_ptr<SourceManager> sourceManager,
                                               moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                                               PluginMetricManagerPtr mgr)
    : AbstractManager(baseMgr, sourceManager, queue, mgr),
      mAppAggregator(
          10240,
          [this](std::unique_ptr<AppMetricData>& base, const std::shared_ptr<AbstractRecord>& o) {
              auto* other = static_cast<AbstractAppRecord*>(o.get());
              int statusCode = other->GetStatusCode();
              base->m2xxCount += statusCode / 100 == 2;
              base->m3xxCount += statusCode / 100 == 3;
              base->m4xxCount += statusCode / 100 == 4;
              base->m5xxCount += statusCode / 100 == 5;
              base->mCount++;
              base->mErrCount += other->IsError();
              base->mSlowCount += other->IsSlow();
              base->mSum += other->GetLatencyMs();
          },
          [this](const std::shared_ptr<AbstractRecord>& i,
                 std::shared_ptr<SourceBuffer>& sourceBuffer) -> std::unique_ptr<AppMetricData> {
              auto* in = static_cast<AbstractAppRecord*>(i.get());
              auto spanName = sourceBuffer->CopyString(in->GetSpanName());
              auto data
                  = std::make_unique<AppMetricData>(in->GetConnection(), StringView(spanName.data, spanName.size));
              auto connection = in->GetConnection();
              if (!connection) {
                  LOG_WARNING(sLogger, ("connection is null", ""));
                  return nullptr;
              }

              auto& ctAttrs = connection->GetConnTrackerAttrs();
              auto appId = sourceBuffer->CopyString(ctAttrs.Get<APP_ID_INDEX>());
              data->mTags.SetNoCopy<kAppId>(StringView(appId.data, appId.size));

              auto appName = sourceBuffer->CopyString(ctAttrs.Get<APP_NAME_INDEX>());
              data->mTags.SetNoCopy<kAppName>(StringView(appName.data, appName.size));

              auto host = sourceBuffer->CopyString(ctAttrs.Get<HOST_NAME_INDEX>());
              data->mTags.SetNoCopy<kHostName>(StringView(host.data, host.size));

              auto ip = sourceBuffer->CopyString(ctAttrs.Get<kPodIp>());
              data->mTags.SetNoCopy<kIp>(StringView(ip.data, ip.size));

              auto workloadKind = sourceBuffer->CopyString(ctAttrs.Get<kWorkloadKind>());
              data->mTags.SetNoCopy<kWorkloadKind>(StringView(workloadKind.data, workloadKind.size));

              auto workloadName = sourceBuffer->CopyString(ctAttrs.Get<kWorkloadName>());
              data->mTags.SetNoCopy<kWorkloadName>(StringView(workloadName.data, workloadName.size));

              auto mRpcType = sourceBuffer->CopyString(ctAttrs.Get<kRpcType>());
              data->mTags.SetNoCopy<kRpcType>(StringView(mRpcType.data, mRpcType.size));

              auto mCallType = sourceBuffer->CopyString(ctAttrs.Get<kCallType>());
              data->mTags.SetNoCopy<kCallType>(StringView(mCallType.data, mCallType.size));

              auto mCallKind = sourceBuffer->CopyString(ctAttrs.Get<kCallKind>());
              data->mTags.SetNoCopy<kCallKind>(StringView(mCallKind.data, mCallKind.size));

              auto mDestId = sourceBuffer->CopyString(ctAttrs.Get<kDestId>());
              data->mTags.SetNoCopy<kDestId>(StringView(mDestId.data, mDestId.size));

              auto endpoint = sourceBuffer->CopyString(ctAttrs.Get<kEndpoint>());
              data->mTags.SetNoCopy<kEndpoint>(StringView(endpoint.data, endpoint.size));

              auto ns = sourceBuffer->CopyString(ctAttrs.Get<kNamespace>());
              data->mTags.SetNoCopy<kNamespace>(StringView(ns.data, ns.size));
              return data;
          }),
      mNetAggregator(
          10240,
          [this](std::unique_ptr<NetMetricData>& base, const std::shared_ptr<AbstractRecord>& o) {
              auto* other = static_cast<ConnStatsRecord*>(o.get());
              base->mDropCount += other->mDropCount;
              base->mRetransCount += other->mRetransCount;
              base->mRecvBytes += other->mRecvBytes;
              base->mSendBytes += other->mSendBytes;
              base->mRecvPkts += other->mRecvPackets;
              base->mSendPkts += other->mSendPackets;
              base->mRtt += other->mRtt;
              base->mRttCount++;
              if (other->mState > 1 && other->mState < LC_TCP_MAX_STATES) {
                  base->mStateCounts[other->mState]++;
              } else {
                  base->mStateCounts[0]++;
              }
          },
          [this](const std::shared_ptr<AbstractRecord>& i, std::shared_ptr<SourceBuffer>& sourceBuffer) {
              auto* in = static_cast<ConnStatsRecord*>(i.get());
              auto connection = in->GetConnection();
              auto data = std::make_unique<NetMetricData>(in->GetConnection());
              auto& ctAttrs = connection->GetConnTrackerAttrs();

              auto appId = sourceBuffer->CopyString(ctAttrs.Get<APP_ID_INDEX>());
              data->mTags.SetNoCopy<kAppId>(StringView(appId.data, appId.size));

              auto appName = sourceBuffer->CopyString(ctAttrs.Get<APP_NAME_INDEX>());
              data->mTags.SetNoCopy<kAppName>(StringView(appName.data, appName.size));

              auto host = sourceBuffer->CopyString(ctAttrs.Get<HOST_NAME_INDEX>());
              data->mTags.SetNoCopy<kHostName>(StringView(host.data, host.size));

              auto ip = sourceBuffer->CopyString(ctAttrs.Get<kPodIp>());
              data->mTags.SetNoCopy<kIp>(StringView(ip.data, ip.size));

              auto wk = sourceBuffer->CopyString(ctAttrs.Get<kWorkloadKind>());
              data->mTags.SetNoCopy<kWorkloadKind>(StringView(wk.data, wk.size));

              auto wn = sourceBuffer->CopyString(ctAttrs.Get<kWorkloadName>());
              data->mTags.SetNoCopy<kWorkloadName>(StringView(wn.data, wn.size));

              auto ns = sourceBuffer->CopyString(ctAttrs.Get<kNamespace>());
              data->mTags.SetNoCopy<kNamespace>(StringView(ns.data, ns.size));

              auto pn = sourceBuffer->CopyString(ctAttrs.Get<kPodName>());
              data->mTags.SetNoCopy<kPodName>(StringView(pn.data, pn.size));

              auto pwk = sourceBuffer->CopyString(ctAttrs.Get<kPeerWorkloadKind>());
              data->mTags.SetNoCopy<kPeerWorkloadKind>(StringView(pwk.data, pwk.size));

              auto pwn = sourceBuffer->CopyString(ctAttrs.Get<kPeerWorkloadName>());
              data->mTags.SetNoCopy<kPeerWorkloadName>(StringView(pwn.data, pwn.size));

              auto pns = sourceBuffer->CopyString(ctAttrs.Get<kPeerNamespace>());
              data->mTags.SetNoCopy<kPeerNamespace>(StringView(pns.data, pns.size));

              auto ppn = sourceBuffer->CopyString(ctAttrs.Get<kPeerPodName>());
              data->mTags.SetNoCopy<kPeerPodName>(StringView(ppn.data, ppn.size));
              return data;
          }),
      mSpanAggregator(
          1024, // 1024 span per second
          [this](std::unique_ptr<AppSpanGroup>& base, const std::shared_ptr<AbstractRecord>& other) {
              base->mRecords.push_back(other);
          },
          [this](const std::shared_ptr<AbstractRecord>& in, std::shared_ptr<SourceBuffer>& sourceBuffer) {
              return std::make_unique<AppSpanGroup>();
          }),
      mLogAggregator(
          1024, // 1024 log per second
          [this](std::unique_ptr<AppLogGroup>& base, const std::shared_ptr<AbstractRecord>& other) {
              base->mRecords.push_back(other);
          },
          [this](const std::shared_ptr<AbstractRecord>& in, std::shared_ptr<SourceBuffer>& sourceBuffer) {
              return std::make_unique<AppLogGroup>();
          }) {
}

std::array<size_t, 2>
NetworkObserverManager::GenerateAggKeyForNetMetric(const std::shared_ptr<AbstractRecord>& abstractRecord) {
    auto* record = static_cast<ConnStatsRecord*>(abstractRecord.get());
    // calculate agg key
    std::array<size_t, 2> result;
    result.fill(0UL);
    std::hash<std::string_view> hasher;
    auto connection = record->GetConnection();
    if (!connection) {
        LOG_WARNING(sLogger, ("connection is null", ""));
        return {};
    }

    auto& connTrackerAttrs = connection->GetConnTrackerAttrs();

    // level0: hostname hostip appId appName, if it's not arms app, we need set default appname ...
    // kConnTrackerTable.ColIndex();
    // level1: namespace workloadkind workloadname peerNamespace peerWorkloadKind peerWorkloadName
    static constexpr auto idxes0 = {APP_ID_INDEX, APP_NAME_INDEX, HOST_NAME_INDEX, HOST_IP_INDEX};
    static constexpr auto idxes1 = {WORKLOAD_KIND_INDEX,
                                    WORKLOAD_NAME_INDEX,
                                    NAMESPACE_INDEX,
                                    PEER_WORKLOAD_KIND_INDEX,
                                    PEER_WORKLOAD_NAME_INDEX,
                                    PEER_NAMESPACE_INDEX};

    for (auto& x : idxes0) {
        std::string_view attr(connTrackerAttrs[x].data(), connTrackerAttrs[x].size());
        AttrHashCombine(result[0], hasher(attr));
    }
    for (auto& x : idxes1) {
        std::string_view attr(connTrackerAttrs[x].data(), connTrackerAttrs[x].size());
        AttrHashCombine(result[1], hasher(attr));
    }
    return result;
}

std::array<size_t, 2>
NetworkObserverManager::GenerateAggKeyForAppMetric(const std::shared_ptr<AbstractRecord>& abstractRecord) {
    auto* record = static_cast<AbstractAppRecord*>(abstractRecord.get());
    // calculate agg key
    std::array<size_t, 2> result;
    result.fill(0UL);
    std::hash<std::string_view> hasher;
    auto connection = record->GetConnection();
    if (!connection) {
        LOG_WARNING(sLogger, ("connection is null", ""));
        return {};
    }

    static constexpr std::array<uint32_t, 4> idxes0 = {APP_ID_INDEX, APP_NAME_INDEX, HOST_NAME_INDEX, HOST_IP_INDEX};
    static constexpr std::array<uint32_t, 9> idxes1 = {WORKLOAD_KIND_INDEX,
                                                       WORKLOAD_NAME_INDEX,
                                                       kConnTrackerTable.ColIndex(kProtocol.Name()),
                                                       kConnTrackerTable.ColIndex(kDestId.Name()),
                                                       kConnTrackerTable.ColIndex(kEndpoint.Name()),
                                                       kConnTrackerTable.ColIndex(kCallType.Name()),
                                                       kConnTrackerTable.ColIndex(kRpcType.Name()),
                                                       kConnTrackerTable.ColIndex(kCallKind.Name())};

    auto& ctAttrs = connection->GetConnTrackerAttrs();
    for (auto x : idxes0) {
        std::string_view attr(ctAttrs[x].data(), ctAttrs[x].size());
        AttrHashCombine(result[0], hasher(attr));
    }
    for (auto x : idxes1) {
        std::string_view attr(ctAttrs[x].data(), ctAttrs[x].size());
        AttrHashCombine(result[1], hasher(attr));
    }
    std::string_view rpc(record->GetSpanName());
    AttrHashCombine(result[1], hasher(rpc));

    return result;
}

std::array<size_t, 1>
NetworkObserverManager::GenerateAggKeyForSpan(const std::shared_ptr<AbstractRecord>& abstractRecord) {
    auto* record = static_cast<AbstractAppRecord*>(abstractRecord.get());
    // calculate agg key
    // just appid
    std::array<size_t, 1> result;
    result.fill(0UL);
    std::hash<std::string_view> hasher;
    auto connection = record->GetConnection();
    if (!connection) {
        LOG_WARNING(sLogger, ("connection is null", ""));
        return {};
    }
    auto& ctAttrs = connection->GetConnTrackerAttrs();
    static constexpr auto idxes = {APP_ID_INDEX, APP_NAME_INDEX, HOST_NAME_INDEX, HOST_IP_INDEX};
    for (auto& x : idxes) {
        std::string_view attr(ctAttrs[x].data(), ctAttrs[x].size());
        AttrHashCombine(result[0], hasher(attr));
    }

    return result;
}

std::array<size_t, 1>
NetworkObserverManager::GenerateAggKeyForLog(const std::shared_ptr<AbstractRecord>& abstractRecord) {
    auto* record = static_cast<AbstractAppRecord*>(abstractRecord.get());
    // just appid
    std::array<size_t, 1> result;
    result.fill(0UL);
    std::hash<uint64_t> hasher;
    auto connection = record->GetConnection();
    if (!connection) {
        LOG_WARNING(sLogger, ("connection is null", ""));
        return {};
    }

    auto connId = connection->GetConnId();

    AttrHashCombine(result[0], hasher(connId.fd));
    AttrHashCombine(result[0], hasher(connId.tgid));
    AttrHashCombine(result[0], hasher(connId.start));

    return result;
}

bool NetworkObserverManager::UpdateParsers(const std::vector<std::string>& protocols,
                                           const std::vector<std::string>& prevProtocols) {
    std::unordered_set<std::string> currentSet(protocols.begin(), protocols.end());
    std::unordered_set<std::string> prevSet(prevProtocols.begin(), prevProtocols.end());

    for (const auto& protocol : protocols) {
        if (prevSet.find(protocol) == prevSet.end()) {
            ProtocolParserManager::GetInstance().AddParser(protocol);
        }
    }

    for (const auto& protocol : prevProtocols) {
        if (currentSet.find(protocol) == currentSet.end()) {
            ProtocolParserManager::GetInstance().RemoveParser(protocol);
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
    SIZETAggTree<AppLogGroup, std::shared_ptr<AbstractRecord>> aggTree = this->mLogAggregator.GetAndReset();
    lk.unlock();

    auto nodes = aggTree.GetNodesWithAggDepth(1);
    LOG_DEBUG(sLogger, ("enter aggregator ...", nodes.size())("node size", aggTree.NodeCount()));
    if (nodes.empty()) {
        LOG_DEBUG(sLogger, ("empty nodes...", "")("node size", aggTree.NodeCount()));
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
            for (const auto& abstractRecord : group->mRecords) {
                auto* record = static_cast<AbstractAppRecord*>(abstractRecord.get());
                if (!init) {
                    const auto& ct = record->GetConnection();
                    auto& ctAttrs = ct->GetConnTrackerAttrs();
                    if (ct == nullptr) {
                        LOG_DEBUG(sLogger, ("ct is null, skip, spanname ", record->GetSpanName()));
                        continue;
                    }
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
                HttpRecord* httpRecord = static_cast<HttpRecord*>(record);
                auto ts = httpRecord->GetStartTimeStamp();
                logEvent->SetTimestamp(ts + mTimeDiff.count());
                logEvent->SetContent(kLatencyNS.LogKey(), std::to_string(httpRecord->GetLatencyNs()));
                logEvent->SetContent(kHTTPMethod.LogKey(), httpRecord->GetMethod());
                logEvent->SetContent(kHTTPPath.LogKey(), httpRecord->GetPath());
                logEvent->SetContent(kHTTPVersion.LogKey(), httpRecord->GetProtocolVersion());
                logEvent->SetContent(kStatusCode.LogKey(), std::to_string(httpRecord->GetStatusCode()));
                logEvent->SetContent(kHTTPReqBody.LogKey(), httpRecord->GetReqBody());
                logEvent->SetContent(kHTTPRespBody.LogKey(), httpRecord->GetRespBody());

                LOG_DEBUG(sLogger, ("add one log, log timestamp", ts));
                needPush = true;
            }
        });
#ifdef APSARA_UNIT_TEST_MAIN
        auto eventSize = eventGroup.GetEvents().size();
        ADD_COUNTER(mPushLogsTotal, eventSize);
        ADD_COUNTER(mPushLogGroupTotal, 1);
        mLogEventGroups.emplace_back(std::move(eventGroup));
#else
        if (init && needPush) {
            std::lock_guard lk(mContextMutex);
            if (this->mPipelineCtx == nullptr) {
                return true;
            }
            auto eventSize = eventGroup.GetEvents().size();
            ADD_COUNTER(mPushLogsTotal, eventSize);
            ADD_COUNTER(mPushLogGroupTotal, 1);
            LOG_DEBUG(sLogger, ("event group size", eventGroup.GetEvents().size()));
            std::unique_ptr<ProcessQueueItem> item
                = std::make_unique<ProcessQueueItem>(std::move(eventGroup), this->mPluginIndex);
            for (size_t times = 0; times < 5; times++) {
                auto result = ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item));
                if (QueueStatus::OK != result) {
                    LOG_WARNING(sLogger,
                                ("configName", mPipelineCtx->GetConfigName())("pluginIdx", this->mPluginIndex)(
                                    "[NetworkObserver] push log to queue failed!", magic_enum::enum_name(result)));
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                } else {
                    LOG_DEBUG(sLogger, ("NetworkObserver push log successful, events:", eventSize));
                    break;
                }
            }

        } else {
            LOG_DEBUG(sLogger, ("NetworkObserver skip push log ", ""));
        }
#endif
    }

    return true;
}

// apm
const static std::string METRIC_NAME_TAG = "arms_tag_entity";
const static std::string METRIC_NAME_REQ_TOTAL = "arms_rpc_requests_count";
const static std::string METRIC_NAME_REQ_DURATION_SUM = "arms_rpc_requests_seconds";
const static std::string METRIC_NAME_REQ_ERR_TOTAL = "arms_rpc_requests_error_count";
const static std::string METRIC_NAME_REQ_SLOW_TOTAL = "arms_rpc_requests_slow_count";
const static std::string METRIC_NAME_REQ_BY_STATUS_TOTAL = "arms_rpc_requests_by_status_count";

static const std::string status_2xx_key = "2xx";
static const std::string status_3xx_key = "3xx";
static const std::string status_4xx_key = "4xx";
static const std::string status_5xx_key = "5xx";

// npm
const static std::string METRIC_NAME_TCP_DROP_TOTAL = "arms_npm_tcp_drop_count";
const static std::string METRIC_NAME_TCP_RETRANS_TOTAL = "arms_npm_tcp_retrans_total";
const static std::string METRIC_NAME_TCP_RTT_AVG = "arms_npm_tcp_rtt_avg";
const static std::string METRIC_NAME_TCP_CONN_TOTAL = "arms_npm_tcp_count_by_state";
const static std::string METRIC_NAME_TCP_RECV_PKTS_TOTAL = "arms_npm_recv_packets_total";
const static std::string METRIC_NAME_TCP_RECV_BYTES_TOTAL = "arms_npm_recv_bytes_total";
const static std::string METRIC_NAME_TCP_SENT_PKTS_TOTAL = "arms_npm_sent_packets_total";
const static std::string METRIC_NAME_TCP_SENT_BTES_TOTAL = "arms_npm_sent_bytes_total";

const static StringView EBPF_VALUE = "ebpf";
const static StringView METRIC_VALUE = "metric";
const static StringView TRACE_VALUE = "trace";
const static StringView LOG_VALUE = "log";

const static StringView SPAN_TAG_KEY_APP = "app";

const static StringView TAG_AGENT_VERSION_KEY = "agentVersion";
const static StringView TAG_APP_KEY = "app";
const static StringView TAG_V1_VALUE = "v1";
const static StringView TAG_RESOURCE_ID_KEY = "resourceid";
const static StringView TAG_VERSION_KEY = "version";
const static StringView TAG_CLUSTER_ID_KEY = "clusterId";
const static StringView TAG_WORKLOAD_NAME_KEY = "workloadName";
const static StringView TAG_WORKLOAD_KIND_KEY = "workloadKind";
const static StringView TAG_NAMESPACE_KEY = "namespace";
const static StringView TAG_HOST_KEY = "host";
const static StringView TAG_HOSTNAME_KEY = "hostname";
const static StringView TAG_APPLICATION_VALUE = "APPLICATION";
const static StringView TAG_RESOURCE_TYPE_KEY = "resourcetype";

enum {
    TCP_ESTABLISHED = 1,
    TCP_SYN_SENT = 2,
    TCP_SYN_RECV = 3,
    TCP_FIN_WAIT1 = 4,
    TCP_FIN_WAIT2 = 5,
    TCP_TIME_WAIT = 6,
    TCP_CLOSE = 7,
    TCP_CLOSE_WAIT = 8,
    TCP_LAST_ACK = 9,
    TCP_LISTEN = 10,
    TCP_CLOSING = 11,
    TCP_NEW_SYN_RECV = 12,
    TCP_MAX_STATES = 13,
};

static constexpr std::array sNetStateStrings = {StringView("UNKNOWN"),
                                                StringView("TCP_ESTABLISHED"),
                                                StringView("TCP_SYN_SENT"),
                                                StringView("TCP_SYN_RECV"),
                                                StringView("TCP_FIN_WAIT1"),
                                                StringView("TCP_FIN_WAIT2"),
                                                StringView("TCP_TIME_WAIT"),
                                                StringView("TCP_CLOSE"),
                                                StringView("TCP_CLOSE_WAIT"),
                                                StringView("TCP_LAST_ACK"),
                                                StringView("TCP_LISTEN"),
                                                StringView("TCP_CLOSING"),
                                                StringView("TCP_NEW_SYN_RECV"),
                                                StringView("TCP_MAX_STATES")};

static constexpr StringView DEFAULT_NET_APP_NAME = "__default_app_name__";
static constexpr StringView DEFAULT_NET_APP_ID = "__default_app_id__";

bool NetworkObserverManager::ConsumeNetMetricAggregateTree(
    const std::chrono::steady_clock::time_point& execTime) { // handler
    if (!this->mFlag || this->mSuspendFlag) {
        return false;
    }

    WriteLock lk(mLogAggLock);
    SIZETAggTreeWithSourceBuffer<NetMetricData, std::shared_ptr<AbstractRecord>> aggTree
        = this->mNetAggregator.GetAndReset();
    lk.unlock();

    auto nodes = aggTree.GetNodesWithAggDepth(1);
    LOG_DEBUG(sLogger, ("enter net aggregator ...", nodes.size())("node size", aggTree.NodeCount()));
    if (nodes.empty()) {
        LOG_DEBUG(sLogger, ("empty nodes...", "")("node size", aggTree.NodeCount()));
        return true;
    }

    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

    for (auto& node : nodes) {
        LOG_DEBUG(sLogger, ("node child size", node->mChild.size()));
        // convert to a item and push to process queue
        // every node represent an instance of an arms app ...

        // auto sourceBuffer = std::make_shared<SourceBuffer>();
        std::shared_ptr<SourceBuffer> sourceBuffer = node->mSourceBuffer;
        PipelineEventGroup eventGroup(sourceBuffer); // per node represent an APP ...
        eventGroup.SetTagNoCopy(kAppType.MetricKey(), EBPF_VALUE);
        eventGroup.SetTagNoCopy(kDataType.MetricKey(), METRIC_VALUE);
        eventGroup.SetTag(TAG_CLUSTER_ID_KEY, mClusterId);

        bool init = false;
        aggTree.ForEach(node, [&](const NetMetricData* group) {
            LOG_DEBUG(sLogger,
                      ("dump group attrs", group->ToString())("ct attrs", group->mConnection->DumpConnection()));
            // auto appId = DEFAULT_NET_APP_ID;
            // if (group->mAppId.size()) {
            //     auto sb = sourceBuffer->CopyString(group->mAppId);
            //     appId = StringView(sb.data, sb.size);
            // }
            // auto appName = DEFAULT_NET_APP_NAME;
            // if (group->mAppName.size()) {
            //     auto sb = sourceBuffer->CopyString(group->mAppName);
            //     appName = StringView(sb.data, sb.size);
            // }
            // auto hostName = sourceBuffer->CopyString(group->mHost);
            // auto ip = sourceBuffer->CopyString(group->mIp);
            if (!init) {
                // set app attrs ...
                // eventGroup.SetTagNoCopy(kAppId.MetricKey(), appId);
                // eventGroup.SetTagNoCopy(kAppName.MetricKey(), appName);
                // eventGroup.SetTagNoCopy(kIp.MetricKey(), StringView(ip.data, ip.size)); // pod ip
                // eventGroup.SetTagNoCopy(kHostName.MetricKey(), StringView(hostName.data, hostName.size)); // pod name

                eventGroup.SetTagNoCopy(kAppId.MetricKey(), group->mTags.Get<kAppId>());
                eventGroup.SetTagNoCopy(kAppName.MetricKey(), group->mTags.Get<kAppName>());
                eventGroup.SetTagNoCopy(kIp.MetricKey(), group->mTags.Get<kIp>()); // pod ip
                eventGroup.SetTagNoCopy(kHostName.MetricKey(), group->mTags.Get<kHostName>()); // pod name
                init = true;
            }

            std::vector<MetricEvent*> metrics;
            if (group->mDropCount > 0) {
                auto* tcpDropMetric = eventGroup.AddMetricEvent();
                tcpDropMetric->SetName(METRIC_NAME_TCP_DROP_TOTAL);
                tcpDropMetric->SetValue(UntypedSingleValue{double(group->mDropCount)});
                metrics.push_back(tcpDropMetric);
            }

            if (group->mRetransCount > 0) {
                auto* tcpRetxMetric = eventGroup.AddMetricEvent();
                tcpRetxMetric->SetName(METRIC_NAME_TCP_RETRANS_TOTAL);
                tcpRetxMetric->SetValue(UntypedSingleValue{double(group->mRetransCount)});
                metrics.push_back(tcpRetxMetric);
            }

            if (group->mRttCount > 0) {
                auto* tcpRttAvg = eventGroup.AddMetricEvent();
                tcpRttAvg->SetName(METRIC_NAME_TCP_RTT_AVG);
                tcpRttAvg->SetValue(UntypedSingleValue{(group->mRtt * 1.0) / group->mRttCount});
                metrics.push_back(tcpRttAvg);
            }

            if (group->mRecvBytes > 0) {
                auto* tcpRxBytes = eventGroup.AddMetricEvent();
                tcpRxBytes->SetName(METRIC_NAME_TCP_RECV_BYTES_TOTAL);
                tcpRxBytes->SetValue(UntypedSingleValue{double(group->mRecvBytes)});
                metrics.push_back(tcpRxBytes);
            }

            if (group->mRecvPkts > 0) {
                auto* tcpRxPkts = eventGroup.AddMetricEvent();
                tcpRxPkts->SetName(METRIC_NAME_TCP_RECV_PKTS_TOTAL);
                tcpRxPkts->SetValue(UntypedSingleValue{double(group->mRecvPkts)});
                metrics.push_back(tcpRxPkts);
            }

            if (group->mSendBytes > 0) {
                auto* tcpTxBytes = eventGroup.AddMetricEvent();
                tcpTxBytes->SetName(METRIC_NAME_TCP_SENT_BTES_TOTAL);
                tcpTxBytes->SetValue(UntypedSingleValue{double(group->mSendBytes)});
                metrics.push_back(tcpTxBytes);
            }

            if (group->mSendPkts > 0) {
                auto* tcpTxPkts = eventGroup.AddMetricEvent();
                tcpTxPkts->SetName(METRIC_NAME_TCP_SENT_PKTS_TOTAL);
                tcpTxPkts->SetValue(UntypedSingleValue{double(group->mSendPkts)});
                metrics.push_back(tcpTxPkts);
            }

            for (size_t zz = 0; zz < LC_TCP_MAX_STATES; zz++) {
                if (group->mStateCounts[zz] > 0) {
                    auto* tcpCount = eventGroup.AddMetricEvent();
                    tcpCount->SetName(METRIC_NAME_TCP_CONN_TOTAL);
                    tcpCount->SetValue(UntypedSingleValue{double(group->mStateCounts[zz])});
                    tcpCount->SetTagNoCopy(kState.MetricKey(), sNetStateStrings[zz]);
                    metrics.push_back(tcpCount);
                }
            }

            // auto workloadKind = sourceBuffer->CopyString(group->mWorkloadKind);
            // auto workloadName = sourceBuffer->CopyString(group->mWorkloadName);
            // auto k8sNamespace = sourceBuffer->CopyString(group->mNamespace);
            // auto peerWorkloadKind = sourceBuffer->CopyString(group->mPeerWorkloadKind);
            // auto peerWorkloadName = sourceBuffer->CopyString(group->mPeerWorkloadName);
            // auto peerNamepace = sourceBuffer->CopyString(group->mPeerNamespace);
            for (auto* metricsEvent : metrics) {
                // set tags
                metricsEvent->SetTimestamp(seconds, 0);
                metricsEvent->SetTagNoCopy(kPodIp.MetricKey(), group->mTags.Get<kIp>());
                metricsEvent->SetTagNoCopy(kPodName.MetricKey(), group->mTags.Get<kPodName>());
                metricsEvent->SetTagNoCopy(kNamespace.MetricKey(), group->mTags.Get<kNamespace>());
                metricsEvent->SetTagNoCopy(kWorkloadKind.MetricKey(), group->mTags.Get<kWorkloadKind>());
                metricsEvent->SetTagNoCopy(kWorkloadName.MetricKey(), group->mTags.Get<kWorkloadName>());
                metricsEvent->SetTagNoCopy(kPeerPodName.MetricKey(), group->mTags.Get<kPeerPodName>());
                metricsEvent->SetTagNoCopy(kPeerNamespace.MetricKey(), group->mTags.Get<kPeerNamespace>());
                metricsEvent->SetTagNoCopy(kPeerWorkloadKind.MetricKey(), group->mTags.Get<kPeerWorkloadKind>());
                metricsEvent->SetTagNoCopy(kPeerWorkloadName.MetricKey(), group->mTags.Get<kPeerWorkloadName>());

                // metricsEvent->SetTagNoCopy(kPodIp.MetricKey(), StringView(ip.data, ip.size));
                // metricsEvent->SetTagNoCopy(kWorkloadName.MetricKey(), StringView(workloadName.data,
                // workloadName.size)); metricsEvent->SetTagNoCopy(kWorkloadKind.MetricKey(),
                // StringView(workloadKind.data, workloadKind.size)); metricsEvent->SetTagNoCopy(kNamespace.MetricKey(),
                // StringView(k8sNamespace.data, k8sNamespace.size));
                // metricsEvent->SetTagNoCopy(kPeerWorkloadName.MetricKey(),
                //                            StringView(peerWorkloadName.data, peerWorkloadName.size));
                // metricsEvent->SetTagNoCopy(kPeerWorkloadKind.MetricKey(),
                //                            StringView(peerWorkloadKind.data, peerWorkloadKind.size));
                // metricsEvent->SetTagNoCopy(kPeerNamespace.MetricKey(),
                //                            StringView(peerNamepace.data, peerNamepace.size));
            }
        });
#ifdef APSARA_UNIT_TEST_MAIN
        auto eventSize = eventGroup.GetEvents().size();
        ADD_COUNTER(mPushMetricsTotal, eventSize);
        ADD_COUNTER(mPushMetricGroupTotal, 1);
        mMetricEventGroups.emplace_back(std::move(eventGroup));
#else
        std::lock_guard lk(mContextMutex);
        if (this->mPipelineCtx == nullptr) {
            return true;
        }
        auto eventSize = eventGroup.GetEvents().size();
        ADD_COUNTER(mPushMetricsTotal, eventSize);
        ADD_COUNTER(mPushMetricGroupTotal, 1);
        LOG_DEBUG(sLogger, ("net event group size", eventGroup.GetEvents().size()));
        std::unique_ptr<ProcessQueueItem> item
            = std::make_unique<ProcessQueueItem>(std::move(eventGroup), this->mPluginIndex);

        for (size_t times = 0; times < 5; times++) {
            auto result = ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item));
            if (QueueStatus::OK != result) {
                LOG_WARNING(sLogger,
                            ("configName", mPipelineCtx->GetConfigName())("pluginIdx", this->mPluginIndex)(
                                "[NetworkObserver] push net metric queue failed!", magic_enum::enum_name(result)));
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } else {
                LOG_DEBUG(sLogger, ("NetworkObserver push net metric successful, events:", eventSize));
                break;
            }
        }
#endif
    }
    return true;
}

bool NetworkObserverManager::ConsumeMetricAggregateTree(
    const std::chrono::steady_clock::time_point& execTime) { // handler
    if (!this->mFlag || this->mSuspendFlag) {
        return false;
    }

    LOG_DEBUG(sLogger, ("enter aggregator ...", mAppAggregator.NodeCount()));

    WriteLock lk(this->mAppAggLock);
    SIZETAggTreeWithSourceBuffer<AppMetricData, std::shared_ptr<AbstractRecord>> aggTree
        = this->mAppAggregator.GetAndReset();
    lk.unlock();

    auto nodes = aggTree.GetNodesWithAggDepth(1);
    LOG_DEBUG(sLogger, ("enter aggregator ...", nodes.size())("node size", aggTree.NodeCount()));
    if (nodes.empty()) {
        LOG_DEBUG(sLogger, ("empty nodes...", ""));
        return true;
    }

    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

    for (auto& node : nodes) {
        LOG_DEBUG(sLogger, ("node child size", node->mChild.size()));
        // convert to a item and push to process queue
        // every node represent an instance of an arms app ...
        // auto sourceBuffer = std::make_shared<SourceBuffer>();
        std::shared_ptr<SourceBuffer> sourceBuffer = node->mSourceBuffer;
        PipelineEventGroup eventGroup(sourceBuffer); // per node represent an APP ...
        eventGroup.SetTag(kAppType.MetricKey(), EBPF_VALUE);
        eventGroup.SetTag(kDataType.MetricKey(), METRIC_VALUE);

        bool needPush = false;

        bool init = false;
        aggTree.ForEach(node, [&](const AppMetricData* group) {
            LOG_DEBUG(sLogger,
                      ("dump group attrs", group->ToString())("ct attrs", group->mConnection->DumpConnection()));
            // instance dim
            if (group->mTags.Get<kAppId>().size()) {
                needPush = true;
            }
            // if (group->mAppId.size()) {
            //     needPush = true;
            // }

            // auto workloadKind = sourceBuffer->CopyString(group->mWorkloadKind);
            // auto workloadName = sourceBuffer->CopyString(group->mWorkloadName);
            // auto k8sNamespace = sourceBuffer->CopyString(group->mNamespace);
            // auto rpcType = sourceBuffer->CopyString(group->mRpcType);
            // auto callType = sourceBuffer->CopyString(group->mCallType);
            // auto callKind = sourceBuffer->CopyString(group->mCallKind);

            if (!init) {
                eventGroup.SetTagNoCopy(kAppId.MetricKey(), group->mTags.Get<kAppId>());
                eventGroup.SetTagNoCopy(kAppName.MetricKey(), group->mTags.Get<kAppName>());
                eventGroup.SetTagNoCopy(kIp.MetricKey(), group->mTags.Get<kIp>()); // pod ip
                eventGroup.SetTagNoCopy(kHostName.MetricKey(), group->mTags.Get<kHostName>()); // pod name

                // set app attrs ...
                // eventGroup.SetTag(std::string(kAppId.MetricKey()), group->mAppId); // app id
                // eventGroup.SetTag(std::string(kIp.MetricKey()), group->mIp); // pod ip
                // eventGroup.SetTag(std::string(kAppName.MetricKey()), group->mAppName); // app name
                // eventGroup.SetTag(std::string(kHostName.MetricKey()), group->mHost); // pod name

                auto* tagMetric = eventGroup.AddMetricEvent();
                tagMetric->SetName(METRIC_NAME_TAG);
                tagMetric->SetValue(UntypedSingleValue{1.0});
                tagMetric->SetTimestamp(seconds, 0);
                tagMetric->SetTagNoCopy(TAG_AGENT_VERSION_KEY, TAG_V1_VALUE);
                tagMetric->SetTagNoCopy(TAG_APP_KEY, group->mTags.Get<kAppName>()); // app ===> appname
                tagMetric->SetTagNoCopy(TAG_RESOURCE_ID_KEY, group->mTags.Get<kAppId>()); // resourceid -==> pid
                tagMetric->SetTagNoCopy(TAG_RESOURCE_TYPE_KEY, TAG_APPLICATION_VALUE); // resourcetype ===> APPLICATION
                tagMetric->SetTagNoCopy(TAG_VERSION_KEY, TAG_V1_VALUE); // version ===> v1
                tagMetric->SetTagNoCopy(TAG_CLUSTER_ID_KEY,
                                        mClusterId); // clusterId ===> TODO read from env _cluster_id_
                tagMetric->SetTagNoCopy(TAG_HOST_KEY, group->mTags.Get<kIp>()); // host ===>
                tagMetric->SetTagNoCopy(TAG_HOSTNAME_KEY, group->mTags.Get<kHostName>()); // hostName ===>
                tagMetric->SetTagNoCopy(TAG_NAMESPACE_KEY, group->mTags.Get<kNamespace>()); // namespace ===>
                tagMetric->SetTagNoCopy(TAG_WORKLOAD_KIND_KEY, group->mTags.Get<kWorkloadKind>()); // workloadKind ===>
                tagMetric->SetTagNoCopy(TAG_WORKLOAD_NAME_KEY, group->mTags.Get<kWorkloadName>()); // workloadName ===>
                init = true;
            }

            LOG_DEBUG(sLogger,
                      ("node app", group->mTags.Get<kAppName>())("group span", group->mTags.Get<kRpc>())(
                          "node size", nodes.size())("rpcType", group->mTags.Get<kRpcType>())(
                          "callType", group->mTags.Get<kCallType>())("callKind", group->mTags.Get<kCallKind>())(
                          "appName", group->mTags.Get<kAppName>())("appId", group->mTags.Get<kAppId>())(
                          "host", group->mTags.Get<kHostName>())("ip", group->mTags.Get<kIp>())(
                          "namespace", group->mTags.Get<kNamespace>())("wk", group->mTags.Get<kWorkloadKind>())(
                          "wn", group->mTags.Get<kWorkloadName>())("reqCnt", group->mCount)("latencySum", group->mSum)(
                          "errCnt", group->mErrCount)("slowCnt", group->mSlowCount));

            std::vector<MetricEvent*> metrics;
            if (group->mCount) {
                auto* requestsMetric = eventGroup.AddMetricEvent();
                requestsMetric->SetName(METRIC_NAME_REQ_TOTAL);
                requestsMetric->SetValue(UntypedSingleValue{double(group->mCount)});
                metrics.push_back(requestsMetric);

                auto* latencyMetric = eventGroup.AddMetricEvent();
                latencyMetric->SetName(METRIC_NAME_REQ_DURATION_SUM);
                latencyMetric->SetValue(UntypedSingleValue{double(group->mSum)});
                metrics.push_back(latencyMetric);
            }
            if (group->mErrCount) {
                auto* errorMetric = eventGroup.AddMetricEvent();
                errorMetric->SetName(METRIC_NAME_REQ_ERR_TOTAL);
                errorMetric->SetValue(UntypedSingleValue{double(group->mErrCount)});
                metrics.push_back(errorMetric);
            }
            if (group->mSlowCount) {
                auto* slowMetric = eventGroup.AddMetricEvent();
                slowMetric->SetName(METRIC_NAME_REQ_SLOW_TOTAL);
                slowMetric->SetValue(UntypedSingleValue{double(group->mSlowCount)});
                metrics.push_back(slowMetric);
            }

            if (group->m2xxCount) {
                auto* statusMetric = eventGroup.AddMetricEvent();
                statusMetric->SetValue(UntypedSingleValue{double(group->m2xxCount)});
                statusMetric->SetName(METRIC_NAME_REQ_BY_STATUS_TOTAL);
                statusMetric->SetTag(kStatusCode.MetricKey(), status_2xx_key);
                metrics.push_back(statusMetric);
            }
            if (group->m3xxCount) {
                auto* statusMetric = eventGroup.AddMetricEvent();
                statusMetric->SetValue(UntypedSingleValue{double(group->m3xxCount)});
                statusMetric->SetName(METRIC_NAME_REQ_BY_STATUS_TOTAL);
                statusMetric->SetTag(kStatusCode.MetricKey(), status_3xx_key);
                metrics.push_back(statusMetric);
            }
            if (group->m4xxCount) {
                auto* statusMetric = eventGroup.AddMetricEvent();
                statusMetric->SetValue(UntypedSingleValue{double(group->m4xxCount)});
                statusMetric->SetName(METRIC_NAME_REQ_BY_STATUS_TOTAL);
                statusMetric->SetTag(kStatusCode.MetricKey(), status_4xx_key);
                metrics.push_back(statusMetric);
            }
            if (group->m5xxCount) {
                auto* statusMetric = eventGroup.AddMetricEvent();
                statusMetric->SetValue(UntypedSingleValue{double(group->m5xxCount)});
                statusMetric->SetName(METRIC_NAME_REQ_BY_STATUS_TOTAL);
                statusMetric->SetTag(kStatusCode.MetricKey(), status_5xx_key);
                metrics.push_back(statusMetric);
            }

            // auto rpc = sourceBuffer->CopyString(group->mSpanName);

            // auto destId = sourceBuffer->CopyString(group->mDestId);
            // auto endpoint = sourceBuffer->CopyString(group->mEndpoint);
            for (auto* metricsEvent : metrics) {
                // set tags
                metricsEvent->SetTimestamp(seconds, 0);

                metricsEvent->SetTagNoCopy(kWorkloadName.MetricKey(), group->mTags.Get<kWorkloadName>());
                metricsEvent->SetTagNoCopy(kWorkloadKind.MetricKey(), group->mTags.Get<kWorkloadKind>());
                metricsEvent->SetTagNoCopy(kNamespace.MetricKey(), group->mTags.Get<kNamespace>());
                metricsEvent->SetTagNoCopy(kRpc.MetricKey(), group->mTags.Get<kRpc>());
                metricsEvent->SetTagNoCopy(kRpcType.MetricKey(), group->mTags.Get<kRpcType>());
                metricsEvent->SetTagNoCopy(kCallType.MetricKey(), group->mTags.Get<kCallType>());
                metricsEvent->SetTagNoCopy(kCallKind.MetricKey(), group->mTags.Get<kCallKind>());
                metricsEvent->SetTagNoCopy(kEndpoint.MetricKey(), group->mTags.Get<kEndpoint>());
                metricsEvent->SetTagNoCopy(kDestId.MetricKey(), group->mTags.Get<kDestId>());

                // metricsEvent->SetTagNoCopy(kWorkloadName.MetricKey(), StringView(workloadName.data,
                // workloadName.size)); metricsEvent->SetTagNoCopy(kWorkloadKind.MetricKey(),
                // StringView(workloadKind.data, workloadKind.size)); metricsEvent->SetTagNoCopy(kNamespace.MetricKey(),
                // StringView(k8sNamespace.data, k8sNamespace.size)); metricsEvent->SetTagNoCopy(kRpc.MetricKey(),
                // StringView(rpc.data, rpc.size)); metricsEvent->SetTagNoCopy(kRpcType.MetricKey(),
                // StringView(rpcType.data, rpcType.size)); metricsEvent->SetTagNoCopy(kCallType.MetricKey(),
                // StringView(callType.data, callType.size)); metricsEvent->SetTagNoCopy(kCallKind.MetricKey(),
                // StringView(callKind.data, callKind.size)); metricsEvent->SetTagNoCopy(kEndpoint.MetricKey(),
                // StringView(endpoint.data, endpoint.size)); metricsEvent->SetTagNoCopy(kDestId.MetricKey(),
                // StringView(destId.data, destId.size));
            }
        });
#ifdef APSARA_UNIT_TEST_MAIN
        auto eventSize = eventGroup.GetEvents().size();
        ADD_COUNTER(mPushMetricsTotal, eventSize);
        ADD_COUNTER(mPushMetricGroupTotal, 1);
        mMetricEventGroups.emplace_back(std::move(eventGroup));
#else
        if (needPush) {
            std::lock_guard lk(mContextMutex);
            if (this->mPipelineCtx == nullptr) {
                return true;
            }
            auto eventSize = eventGroup.GetEvents().size();
            ADD_COUNTER(mPushMetricsTotal, eventSize);
            ADD_COUNTER(mPushMetricGroupTotal, 1);
            LOG_DEBUG(sLogger, ("event group size", eventGroup.GetEvents().size()));
            std::unique_ptr<ProcessQueueItem> item
                = std::make_unique<ProcessQueueItem>(std::move(eventGroup), this->mPluginIndex);

            for (size_t times = 0; times < 5; times++) {
                auto result = ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item));
                if (QueueStatus::OK != result) {
                    LOG_WARNING(sLogger,
                                ("configName", mPipelineCtx->GetConfigName())("pluginIdx", this->mPluginIndex)(
                                    "[NetworkObserver] push app metric queue failed!", magic_enum::enum_name(result)));
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                } else {
                    LOG_DEBUG(sLogger, ("NetworkObserver push app metric successful, events:", eventSize));
                    break;
                }
            }
        } else {
            LOG_DEBUG(sLogger, ("appid is empty, no need to push", ""));
        }
#endif
    }
    return true;
}

bool NetworkObserverManager::ConsumeSpanAggregateTree(
    const std::chrono::steady_clock::time_point& execTime) { // handler
    if (!this->mFlag || this->mSuspendFlag) {
        return false;
    }

    WriteLock lk(mSpanAggLock);
    SIZETAggTree<AppSpanGroup, std::shared_ptr<AbstractRecord>> aggTree = this->mSpanAggregator.GetAndReset();
    lk.unlock();

    auto nodes = aggTree.GetNodesWithAggDepth(1);
    LOG_DEBUG(sLogger, ("enter aggregator ...", nodes.size())("node size", aggTree.NodeCount()));
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
            for (const auto& abstractRecord : group->mRecords) {
                auto* record = static_cast<AbstractAppRecord*>(abstractRecord.get());
                const auto& ct = record->GetConnection();
                auto& ctAttrs = ct->GetConnTrackerAttrs();

                if (ctAttrs.Get<APP_NAME_INDEX>().empty() || ct == nullptr) {
                    LOG_DEBUG(sLogger,
                              ("no app name or ct null, skip, spanname ", record->GetSpanName())(
                                  "appname", ctAttrs.Get<APP_NAME_INDEX>())("ct null", ct == nullptr));
                    continue;
                }

                if (!init) {
                    // set app attrs ...
                    auto appName = sourceBuffer->CopyString(ctAttrs.Get<APP_NAME_INDEX>());
                    eventGroup.SetTagNoCopy(kAppName.SpanKey(), StringView(appName.data, appName.size)); // app name
                    auto appId = sourceBuffer->CopyString(ctAttrs.Get<APP_ID_INDEX>());
                    eventGroup.SetTagNoCopy(kAppId.SpanKey(), StringView(appId.data, appId.size)); // app id
                    auto podIp = sourceBuffer->CopyString(ctAttrs.Get<kPodIp>());
                    eventGroup.SetTagNoCopy(kHostIp.SpanKey(), StringView(podIp.data, podIp.size)); // pod ip
                    auto hostName = sourceBuffer->CopyString(ctAttrs.Get<HOST_NAME_INDEX>());
                    eventGroup.SetTagNoCopy(kHostName.SpanKey(), StringView(hostName.data, hostName.size)); // pod name

                    eventGroup.SetTagNoCopy(kAppType.SpanKey(), EBPF_VALUE); //
                    eventGroup.SetTagNoCopy(kDataType.SpanKey(), TRACE_VALUE);
                    for (auto tag = eventGroup.GetTags().begin(); tag != eventGroup.GetTags().end(); tag++) {
                        LOG_DEBUG(sLogger, ("record span tags", "")(std::string(tag->first), std::string(tag->second)));
                    }
                    init = true;
                }
                auto* spanEvent = eventGroup.AddSpanEvent();
                auto workloadName = sourceBuffer->CopyString(ctAttrs.Get<kWorkloadName>());
                // TODO @qianlu.kk
                spanEvent->SetTagNoCopy(SPAN_TAG_KEY_APP, StringView(workloadName.data, workloadName.size));
                auto hostName = sourceBuffer->CopyString(ctAttrs.Get<HOST_NAME_INDEX>());
                spanEvent->SetTag(kHostName.Name(), StringView(hostName.data, hostName.size));

                for (size_t i = 0; i < kConnTrackerElementsTableSize; i++) {
                    auto sb = sourceBuffer->CopyString(ctAttrs[i]);
                    spanEvent->SetTagNoCopy(kConnTrackerTable.ColSpanKey(i), StringView(sb.data, sb.size));
                    LOG_DEBUG(sLogger, ("record span tags", "")(std::string(kConnTrackerTable.ColSpanKey(i)), sb.data));
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
                spanEvent->SetTag(kHTTPReqBody.SpanKey(), record->GetReqBody());
                spanEvent->SetTag(kHTTPRespBody.SpanKey(), record->GetRespBody());
                spanEvent->SetTag(kHTTPReqBodySize.SpanKey(), std::to_string(record->GetReqBodySize()));
                spanEvent->SetTag(kHTTPRespBodySize.SpanKey(), std::to_string(record->GetRespBodySize()));
                spanEvent->SetTag(kHTTPVersion.SpanKey(), record->GetProtocolVersion());

                auto startTime = record->GetStartTimeStamp() + this->mTimeDiff.count();
                spanEvent->SetStartTimeNs(startTime);
                auto endTime = record->GetEndTimeStamp() + this->mTimeDiff.count();
                spanEvent->SetEndTimeNs(endTime);
                spanEvent->SetTimestamp(seconds);
                LOG_DEBUG(sLogger, ("add one span, startTs", startTime)("entTs", endTime));
                needPush = true;
            }
        });
#ifdef APSARA_UNIT_TEST_MAIN
        auto eventSize = eventGroup.GetEvents().size();
        ADD_COUNTER(mPushMetricsTotal, eventSize);
        ADD_COUNTER(mPushMetricGroupTotal, 1);
        mSpanEventGroups.emplace_back(std::move(eventGroup));
#else
        if (init && needPush) {
            std::lock_guard lk(mContextMutex);
            if (this->mPipelineCtx == nullptr) {
                return true;
            }
            auto eventSize = eventGroup.GetEvents().size();
            ADD_COUNTER(mPushSpansTotal, eventSize);
            ADD_COUNTER(mPushSpanGroupTotal, 1);
            LOG_DEBUG(sLogger, ("event group size", eventGroup.GetEvents().size()));
            std::unique_ptr<ProcessQueueItem> item
                = std::make_unique<ProcessQueueItem>(std::move(eventGroup), this->mPluginIndex);

            for (size_t times = 0; times < 5; times++) {
                auto result = ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item));
                if (QueueStatus::OK != result) {
                    LOG_WARNING(sLogger,
                                ("configName", mPipelineCtx->GetConfigName())("pluginIdx", this->mPluginIndex)(
                                    "[NetworkObserver] push span queue failed!", magic_enum::enum_name(result)));
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                } else {
                    LOG_DEBUG(sLogger, ("NetworkObserver push span successful, events:", eventSize));
                    break;
                }
            }
        } else {
            LOG_DEBUG(sLogger, ("NetworkObserver skip push span ", ""));
        }
#endif
    }

    return true;
}

std::string getLastPathSegment(const std::string& path) {
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return path; // No '/' found, return the entire string
    } else {
        return path.substr(pos + 1); // Return the substring after the last '/'
    }
}

int GuessContainerIdOffset() {
    const std::string cgroupFilePath = "/proc/self/cgroup";
    std::ifstream cgroupFile(cgroupFilePath);
    const std::regex regex("[a-f0-9]{64}");
    std::smatch match;

    std::string line;
    std::string lastSegment;
    while (std::getline(cgroupFile, line)) {
        // cgroup file format：<hierarchy-id>:<subsystem>:/<cgroup-path>
        LOG_DEBUG(sLogger, ("cgroup line", line));
        size_t lastColonPosition = line.find_last_of(':');
        if (lastColonPosition != std::string::npos) {
            std::string cgroupPath = line.substr(lastColonPosition + 1);
            lastSegment = getLastPathSegment(cgroupPath);
            LOG_DEBUG(sLogger, ("The last segment in the cgroup path", lastSegment));
            if (std::regex_search(lastSegment, match, regex)) {
                auto cid = match.str(0);
                LOG_DEBUG(sLogger, ("lastSegment", line)("pos", match.position())("cid", cid)("size", cid.size()));
                cgroupFile.close();
                return match.position();
            } else {
                LOG_DEBUG(sLogger, ("Find next line, Current line unexpected format in cgroup line", lastSegment));
                continue;
            }
        }
    }
    cgroupFile.close();
    LOG_ERROR(sLogger, ("No valid cgroup line to parse ... ", ""));
    return -1;
}

int NetworkObserverManager::Update(
    [[maybe_unused]] const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) {
    auto* opt = std::get<ObserverNetworkOption*>(options);

    // diff opt
    if (mPreviousOpt) {
        CompareAndUpdate("EnableLog", mPreviousOpt->mEnableLog, opt->mEnableLog, [this](bool oldValue, bool newValue) {
            this->mEnableLog = newValue;
        });
        CompareAndUpdate("EnableMetric",
                         mPreviousOpt->mEnableMetric,
                         opt->mEnableMetric,
                         [this](bool oldValue, bool newValue) { this->mEnableMetric = newValue; });
        CompareAndUpdate("EnableSpan",
                         mPreviousOpt->mEnableSpan,
                         opt->mEnableSpan,
                         [this](bool oldValue, bool newValue) { this->mEnableSpan = newValue; });
        CompareAndUpdate(
            "SampleRate", mPreviousOpt->mSampleRate, opt->mSampleRate, [this](double oldValue, double newValue) {
                if (newValue < 0 || newValue > 1) {
                    LOG_WARNING(sLogger,
                                ("invalid sample rate, must between [0, 1], use default 0.01, givin", newValue));
                    newValue = 0.01;
                }
                WriteLock lk(mSamplerLock);
                mSampler = std::make_shared<HashRatioSampler>(newValue);
            });
        CompareAndUpdate("EnableProtocols",
                         mPreviousOpt->mEnableProtocols,
                         opt->mEnableProtocols,
                         [this](const std::vector<std::string>& oldValue, const std::vector<std::string>& newValue) {
                             this->UpdateParsers(newValue, oldValue);
                         });
    }

    // update previous opt
    mPreviousOpt = std::make_unique<ObserverNetworkOption>(*opt);

    return 0;
}

int NetworkObserverManager::Init(const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) {
    if (mFlag)
        return 0;
    auto opt = std::get<ObserverNetworkOption*>(options);
    if (!opt) {
        LOG_ERROR(sLogger, ("invalid options", ""));
        return -1;
    }

    UpdateParsers(opt->mEnableProtocols, {});

    mFlag = true;

    mConnectionManager = ConnectionManager::Create();
    mConnectionManager->SetConnStatsStatus(!opt->mDisableConnStats);
    mConnectionManager->RegisterConnStatsFunc(
        [this](const std::shared_ptr<AbstractRecord>& record) { ProcessRecord(record); });

    mPollKernelFreqMgr.SetPeriod(std::chrono::milliseconds(200));
    mConsumerFreqMgr.SetPeriod(std::chrono::milliseconds(300));

    mCidOffset = GuessContainerIdOffset();

    const char* value = getenv("_cluster_id_");
    if (value != NULL) {
        mClusterId = StringTo<std::string>(value);
    }

    std::shared_ptr<AbstractManager> managerPtr
        = eBPFServer::GetInstance()->GetPluginManager(PluginType::NETWORK_OBSERVE);

    // TODO @qianlu.kk init converger later ...

    std::unique_ptr<AggregateEvent> appTraceEvent = std::make_unique<AggregateEvent>(
        1,
        [managerPtr](const std::chrono::steady_clock::time_point& execTime) {
            NetworkObserverManager* networkObserverManager = static_cast<NetworkObserverManager*>(managerPtr.get());
            return networkObserverManager->ConsumeSpanAggregateTree(execTime);
        },
        [managerPtr]() { // stop checker
            if (!managerPtr->IsExists()) {
                LOG_INFO(sLogger, ("plugin not exists", "stop schedule")("ref cnt", managerPtr.use_count()));
                return true;
            }
            return false;
        });

    std::unique_ptr<AggregateEvent> appLogEvent = std::make_unique<AggregateEvent>(
        1,
        [managerPtr](const std::chrono::steady_clock::time_point& execTime) {
            NetworkObserverManager* networkObserverManager = static_cast<NetworkObserverManager*>(managerPtr.get());
            return networkObserverManager->ConsumeLogAggregateTree(execTime);
        },
        [managerPtr]() { // stop checker
            if (!managerPtr->IsExists()) {
                LOG_INFO(sLogger, ("plugin not exists", "stop schedule")("ref cnt", managerPtr.use_count()));
                return true;
            }
            return false;
        });

    std::unique_ptr<AggregateEvent> appMetricEvent = std::make_unique<AggregateEvent>(
        15,
        [managerPtr](const std::chrono::steady_clock::time_point& execTime) {
            NetworkObserverManager* networkObserverManager = static_cast<NetworkObserverManager*>(managerPtr.get());
            return networkObserverManager->ConsumeMetricAggregateTree(execTime);
        },
        [managerPtr]() { // stop checker
            if (!managerPtr->IsExists()) {
                LOG_INFO(sLogger, ("plugin not exists", "stop schedule")("ref cnt", managerPtr.use_count()));
                return true;
            }
            return false;
        });

    std::unique_ptr<AggregateEvent> netMetricEvent = std::make_unique<AggregateEvent>(
        15,
        [managerPtr](const std::chrono::steady_clock::time_point& execTime) {
            NetworkObserverManager* networkObserverManager = static_cast<NetworkObserverManager*>(managerPtr.get());
            return networkObserverManager->ConsumeNetMetricAggregateTree(execTime);
        },
        [managerPtr]() { // stop checker
            if (!managerPtr->IsExists()) {
                LOG_INFO(sLogger, ("plugin not exists", "stop schedule")("ref cnt", managerPtr.use_count()));
                return true;
            }
            return false;
        });

    Timer::GetInstance()->PushEvent(std::move(appMetricEvent));
    Timer::GetInstance()->PushEvent(std::move(netMetricEvent));
    Timer::GetInstance()->PushEvent(std::move(appTraceEvent));
    Timer::GetInstance()->PushEvent(std::move(appLogEvent));
    // init sampler
    {
        if (opt->mSampleRate < 0 || opt->mSampleRate > 1) {
            LOG_WARNING(sLogger,
                        ("invalid sample rate, must between [0, 1], use default 0.01, givin", opt->mSampleRate));
            opt->mSampleRate = 0.01;
        }
        WriteLock lk(mSamplerLock);
        LOG_INFO(sLogger, ("sample rate", opt->mSampleRate));
        mSampler = std::make_shared<HashRatioSampler>(opt->mSampleRate);
    }

    mEnableLog = opt->mEnableLog;
    mEnableSpan = opt->mEnableSpan;
    mEnableMetric = opt->mEnableMetric;

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

    // register update host K8s metadata task ...
    if (!opt->mDisableMetadata && K8sMetadata::GetInstance().Enable()) {
        config.mCidOffset = mCidOffset;
        config.mEnableCidFilter = true;

        std::unique_ptr<AggregateEvent> hostMetaUpdateTask = std::make_unique<AggregateEvent>(
            5,
            [managerPtr](const std::chrono::steady_clock::time_point& execTime) {
                NetworkObserverManager* networkObserverManager = static_cast<NetworkObserverManager*>(managerPtr.get());
                std::vector<std::string> podIpVec;
                bool res = K8sMetadata::GetInstance().GetByLocalHostFromServer(podIpVec);
                if (res) {
                    networkObserverManager->HandleHostMetadataUpdate(podIpVec);
                } else {
                    LOG_DEBUG(sLogger, ("failed to request host metada", ""));
                }
                return true;
            },
            [managerPtr]() { // stop checker
                if (!managerPtr->IsExists()) {
                    LOG_INFO(sLogger, ("plugin not exists", "stop schedule")("ref cnt", managerPtr.use_count()));
                    return true;
                }
                return false;
            });
        Timer::GetInstance()->PushEvent(std::move(hostMetaUpdateTask));
    }

    config.mDataHandler = [](void* custom_data, struct conn_data_event_t* event) {
        auto mgr = static_cast<NetworkObserverManager*>(custom_data);
        if (mgr == nullptr) {
            LOG_ERROR(sLogger, ("assert network observer handler failed", ""));
            return;
        }

        if (event->request_len == 0 || event->response_len == 0) {
            return;
        }

        mgr->AcceptDataEvent(event);
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

    mRollbackQueue = moodycamel::BlockingConcurrentQueue<std::shared_ptr<AbstractRecord>>(4096);

    LOG_INFO(sLogger, ("begin to start ebpf ... ", ""));
    this->mFlag = true;
    this->RunInThread();
    return 0;
}

void NetworkObserverManager::HandleHostMetadataUpdate(const std::vector<std::string>& podCidVec) {
    std::vector<std::string> newContainerIds;
    std::vector<std::string> expiredContainerIds;
    std::unordered_set<std::string> currentCids;

    for (const auto& cid : podCidVec) {
        auto podInfo = K8sMetadata::GetInstance().GetInfoByContainerIdFromCache(cid);
        if (!podInfo || podInfo->appId == "") {
            // filter appid ...
            LOG_DEBUG(sLogger, (cid, "cannot fetch pod metadata or doesn't have arms label"));
            continue;
        }

        currentCids.insert(cid);
        if (!mEnabledCids.count(cid)) {
            // if cid doesn't exist in last cid set
            newContainerIds.push_back(cid);
        }
        LOG_DEBUG(sLogger,
                  ("appId", podInfo->appId)("appName", podInfo->appName)("podIp", podInfo->podIp)(
                      "podName", podInfo->podName)("containerId", cid));
    }

    for (const auto& cid : mEnabledCids) {
        if (!currentCids.count(cid)) {
            expiredContainerIds.push_back(cid);
        }
    }

    mEnabledCids = std::move(currentCids);
    UpdateWhitelists(std::move(newContainerIds), std::move(expiredContainerIds));
}

void NetworkObserverManager::RunInThread() {
    // periodically poll perf buffer ...
    LOG_INFO(sLogger, ("enter core thread ", ""));
    // start a new thread to poll perf buffer ...
    mCoreThread = std::thread(&NetworkObserverManager::PollBufferWrapper, this);
    mRecordConsume = std::thread(&NetworkObserverManager::ConsumeRecords, this);

    LOG_INFO(sLogger, ("network observer plugin installed.", ""));
}

void NetworkObserverManager::ProcessRecordAsLog(const std::shared_ptr<AbstractRecord>& record) {
    WriteLock lk(mLogAggLock);
    auto res = mLogAggregator.Aggregate(record, GenerateAggKeyForLog(record));
    LOG_DEBUG(sLogger, ("agg res", res)("node count", mLogAggregator.NodeCount()));
}

void NetworkObserverManager::ProcessRecordAsSpan(const std::shared_ptr<AbstractRecord>& record) {
    WriteLock lk(mSpanAggLock);
    auto res = mSpanAggregator.Aggregate(record, GenerateAggKeyForSpan(record));
    LOG_DEBUG(sLogger, ("agg res", res)("node count", mSpanAggregator.NodeCount()));
}

void NetworkObserverManager::ProcessRecordAsMetric(const std::shared_ptr<AbstractRecord>& record) {
    WriteLock lk(mAppAggLock);
    auto res = mAppAggregator.Aggregate(record, GenerateAggKeyForAppMetric(record));
    LOG_DEBUG(sLogger, ("agg res", res)("node count", mAppAggregator.NodeCount()));
}

std::atomic_int HttpRecord::sConstructCount = 0;
std::atomic_int HttpRecord::sDestructCount = 0;

void NetworkObserverManager::ProcessRecord(const std::shared_ptr<AbstractRecord>& record) {
    switch (record->GetRecordType()) {
        case RecordType::APP_RECORD: {
            auto* appRecord = static_cast<AbstractAppRecord*>(record.get());
            if (!appRecord || !appRecord->GetConnection()) {
                // should not happen
                return;
            }
            if (!appRecord->GetConnection()->MetaAttachReadyForApp()) {
                // rollback
                auto times = record->Rollback();
#ifdef APSARA_UNIT_TEST_MAIN
                if (times == 1) {
                    mRollbackRecordTotal++;
                }
#endif
                if (times > 5) {
#ifdef APSARA_UNIT_TEST_MAIN
                    mDropRecordTotal++;
#endif
                    LOG_WARNING(sLogger, ("app meta not ready, drop app record, times", times)
                                // ("connection", appRecord->GetConnection()->DumpConnection())
                    );
                } else {
                    LOG_WARNING(sLogger,
                                ("app meta not ready, rollback app record, times",
                                 times)("connection", appRecord->GetConnection()->DumpConnection()));
                    mRollbackQueue.try_enqueue(std::move(record));
                }
                return;
            } else {
                LOG_DEBUG(sLogger,
                          ("app meta ready, times",
                           record->RollbackCount())("connection", appRecord->GetConnection()->DumpConnection()));
            }

            if (mEnableLog && record->ShouldSample()) {
                ProcessRecordAsLog(record);
            }
            if (mEnableMetric) {
                // TODO: add converge ...
                // aggregate ...
                ProcessRecordAsMetric(record);
            }
            if (mEnableSpan && record->ShouldSample()) {
                ProcessRecordAsSpan(record);
            }
            break;
        }
        case RecordType::CONN_STATS_RECORD: {
            auto* connStatsRecord = static_cast<ConnStatsRecord*>(record.get());
            if (!connStatsRecord || !connStatsRecord->GetConnection()) {
                // should not happen
                return;
            }
            if (!connStatsRecord->GetConnection()->MetaAttachReadyForNet()) {
                // rollback
                auto times = record->Rollback();
                if (times > 5) {
                    LOG_WARNING(sLogger, ("net meta not ready, drop net record, times", times)
                                // ("connection", connStatsRecord->GetConnection()->DumpConnection())
                    );
                } else {
                    LOG_WARNING(sLogger,
                                ("net meta not ready, rollback net record, times",
                                 times)("connection", connStatsRecord->GetConnection()->DumpConnection()));
                    mRollbackQueue.enqueue(record);
                }
                return;
            }
            // do aggregate
            {
                WriteLock lk(mNetAggLock);
                auto res = mNetAggregator.Aggregate(record, GenerateAggKeyForNetMetric(record));
                LOG_DEBUG(sLogger, ("agg res", res)("node count", mNetAggregator.NodeCount()));
            }

            break;
        }
        default:
            break;
    }
}

void NetworkObserverManager::HandleRollbackRecords(const std::chrono::steady_clock::time_point& now) {
    if (mRollbackRecords.size() < 1024 && !mConsumerFreqMgr.Expired(now)) {
        return;
    }

    // periodically consume freq mgr ...
    mConsumerFreqMgr.Reset(now);

    size_t size = mRollbackRecords.size();
    for (size_t i = 0; i < size; i++) {
        auto& record = mRollbackRecords.front();
        ProcessRecord(record);
    }
}

void NetworkObserverManager::ConsumeRecords() {
    std::vector<std::shared_ptr<AbstractRecord>> items(4096);
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
        size_t count = mRollbackQueue.wait_dequeue_bulk_timed(items.data(), 4096, std::chrono::milliseconds(200));
        LOG_DEBUG(sLogger, ("get records:", count));
        // handle ....
        if (count == 0) {
            continue;
        }

        for (size_t i = 0; i < count; i++) {
            ProcessRecord(items[i]);
        }

        items.clear();
        items.resize(4096);
        LOG_DEBUG(sLogger,
                  ("construct records", HttpRecord::sConstructCount)("destruct records", HttpRecord::sDestructCount));
    }
}

void NetworkObserverManager::PollBufferWrapper() {
    LOG_DEBUG(sLogger, ("enter poll perf buffer", ""));
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
        int ret = mSourceManager->PollPerfBuffers(
            PluginType::NETWORK_OBSERVE, kNetObserverMaxBatchConsumeSize, &flag, kNetObserverMaxWaitTimeMS);
        if (ret < 0) {
            LOG_WARNING(sLogger, ("poll event err, ret", ret));
        }

        mConnectionManager->Iterations(cnt++);

        // Consume Rollback Queue
        // HandleRollbackRecords(now);

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
    ADD_COUNTER(mLossKernelEventsTotal, lost_count);
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

void NetworkObserverManager::AcceptDataEvent(struct conn_data_event_t* event) {
    ADD_COUNTER(mRecvKernelEventsTotal, 1);
    const auto conn = mConnectionManager->AcceptNetDataEvent(event);
    mRecvHttpDataEventsTotal.fetch_add(1);

    LOG_DEBUG(sLogger, ("begin to handle data event", ""));

    // get protocol
    auto protocol = event->protocol;
    if (support_proto_e::ProtoUnknown == protocol) {
        LOG_DEBUG(sLogger, ("protocol is unknown, skip parse", ""));
        return;
    }

    LOG_DEBUG(sLogger, ("begin parse, protocol is", std::string(magic_enum::enum_name(event->protocol))));

    ReadLock lk(mSamplerLock);
    std::vector<std::shared_ptr<AbstractRecord>> records
        = ProtocolParserManager::GetInstance().Parse(protocol, conn, event, mSampler);
    lk.unlock();

    // add records to span/event generate queue
    for (auto& record : records) {
        ProcessRecord(record);
        // mRollbackQueue.enqueue(std::move(record));
    }
}

void NetworkObserverManager::AcceptNetStatsEvent(struct conn_stats_event_t* event) {
    ADD_COUNTER(mRecvKernelEventsTotal, 1);
    LOG_DEBUG(
        sLogger,
        ("[DUMP] stats event handle, fd", event->conn_id.fd)("pid", event->conn_id.tgid)("start", event->conn_id.start)(
            "role", int(event->role))("state", int(event->conn_events))("eventTs", event->ts));
    mConnectionManager->AcceptNetStatsEvent(event);
}

void NetworkObserverManager::AcceptNetCtrlEvent(struct conn_ctrl_event_t* event) {
    ADD_COUNTER(mRecvKernelEventsTotal, 1);
    LOG_DEBUG(sLogger,
              ("[DUMP] ctrl event handle, fd", event->conn_id.fd)("pid", event->conn_id.tgid)(
                  "start", event->conn_id.start)("type", int(event->type))("eventTs", event->ts));
    mConnectionManager->AcceptNetCtrlEvent(event);
}

int NetworkObserverManager::Destroy() {
    if (!mFlag)
        return 0;
    LOG_INFO(sLogger, ("prepare to destroy", ""));
    mSourceManager->StopPlugin(PluginType::NETWORK_OBSERVE);
    LOG_INFO(sLogger, ("destroy stage", "shutdown ebpf prog"));
    this->mFlag = false;

    if (this->mCoreThread.joinable()) {
        this->mCoreThread.join();
    }
    LOG_INFO(sLogger, ("destroy stage", "release core thread"));

    if (this->mRecordConsume.joinable()) {
        this->mRecordConsume.join();
    }
#ifdef APSARA_UNIT_TEST_MAIN
    return 0;
#endif
    LOG_INFO(sLogger, ("destroy stage", "destroy connection manager"));
    mConnectionManager.reset(nullptr);
    LOG_INFO(sLogger, ("destroy stage", "destroy sampler"));
    {
        WriteLock lk(mSamplerLock);
        mSampler.reset();
    }

    mEnabledCids.clear();
    mPreviousOpt.reset(nullptr);

    LOG_INFO(sLogger, ("destroy stage", "clear statistics"));

    mDataEventsDropTotal = 0;
    mConntrackerNum = 0;
    mRecvConnStatEventsTotal = 0;
    mRecvCtrlEventsTotal = 0;
    mRecvHttpDataEventsTotal = 0;
    mLostConnStatEventsTotal = 0;
    mLostCtrlEventsTotal = 0;
    mLostDataEventsTotal = 0;

    LOG_INFO(sLogger, ("destroy stage", "clear agg tree"));
    {
        WriteLock lk(mAppAggLock);
        mAppAggregator.Reset();
    }
    {
        WriteLock lk(mNetAggLock);
        mNetAggregator.Reset();
    }
    {
        WriteLock lk(mSpanAggLock);
        mSpanAggregator.Reset();
    }
    {
        WriteLock lk(mLogAggLock);
        mLogAggregator.Reset();
    }

    LOG_INFO(sLogger, ("destroy stage", "release consumer thread"));
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

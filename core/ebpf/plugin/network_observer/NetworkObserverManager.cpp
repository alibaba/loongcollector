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

#include "ebpf/plugin/network_observer/NetworkObserverManager.h"

#include "collection_pipeline/queue/ProcessQueueItem.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/HashUtil.h"
#include "common/StringTools.h"
#include "common/StringView.h"
#include "common/TimeUtil.h"
#include "common/http/AsynCurlRunner.h"
#include "common/magic_enum.hpp"
#include "ebpf/Config.h"
#include "ebpf/EBPFServer.h"
#include "ebpf/include/export.h"
#include "ebpf/protocol/ProtocolParser.h"
#include "ebpf/type/AggregateEvent.h"
#include "ebpf/util/TraceId.h"
#include "logger/Logger.h"
#include "metadata/K8sMetadata.h"

extern "C" {
#include <coolbpf/net.h>
}

namespace logtail::ebpf {

class EBPFServer;

inline constexpr int kNetObserverMaxBatchConsumeSize = 4096;
inline constexpr int kNetObserverMaxWaitTimeMS = 0;

static constexpr uint32_t kAppIdIndex = kConnTrackerTable.ColIndex(kAppId.Name());
static constexpr uint32_t kAppNameIndex = kConnTrackerTable.ColIndex(kAppName.Name());
static constexpr uint32_t kHostNameIndex = kConnTrackerTable.ColIndex(kHostName.Name());
static constexpr uint32_t kHostIpIndex = kConnTrackerTable.ColIndex(kIp.Name());

static constexpr uint32_t kWorkloadKindIndex = kConnTrackerTable.ColIndex(kWorkloadKind.Name());
static constexpr uint32_t kWorkloadNameIndex = kConnTrackerTable.ColIndex(kWorkloadName.Name());
static constexpr uint32_t kNamespaceIndex = kConnTrackerTable.ColIndex(kNamespace.Name());

static constexpr uint32_t kPeerWorkloadKindIndex = kConnTrackerTable.ColIndex(kPeerWorkloadKind.Name());
static constexpr uint32_t kPeerWorkloadNameIndex = kConnTrackerTable.ColIndex(kPeerWorkloadName.Name());
static constexpr uint32_t kPeerNamespaceIndex = kConnTrackerTable.ColIndex(kPeerNamespace.Name());

// apm
const static std::string kMetricNameTag = "arms_tag_entity";
const static std::string kMetricNameRequestTotal = "arms_rpc_requests_count";
const static std::string kMetricNameRequestDurationSum = "arms_rpc_requests_seconds";
const static std::string kMetricNameRequestErrorTotal = "arms_rpc_requests_error_count";
const static std::string kMetricNameRequestSlowTotal = "arms_rpc_requests_slow_count";
const static std::string kMetricNameRequestByStatusTotal = "arms_rpc_requests_by_status_count";

static const StringView kStatus2xxKey = "2xx";
static const StringView kStatus3xxKey = "3xx";
static const StringView kStatus4xxKey = "4xx";
static const StringView kStatus5xxKey = "5xx";

// npm
const static std::string kMetricNameTcpDropTotal = "arms_npm_tcp_drop_count";
const static std::string kMetricNameTcpRetransTotal = "arms_npm_tcp_retrans_total";
const static std::string kMetricNameTcpRttAvg = "arms_npm_tcp_rtt_avg";
const static std::string kMetricNameTcpConnTotal = "arms_npm_tcp_count_by_state";
const static std::string kMetricNameTcpRecvPktsTotal = "arms_npm_recv_packets_total";
const static std::string kMetricNameTcpRecvBytesTotal = "arms_npm_recv_bytes_total";
const static std::string kMetricNameTcpSentPktsTotal = "arms_npm_sent_packets_total";
const static std::string kMetricNameTcpSentBytesTotal = "arms_npm_sent_bytes_total";

const static StringView kEBPFValue = "ebpf";
const static StringView kMetricValue = "metric";
const static StringView kTraceValue = "trace";
const static StringView kLogValue = "log";

const static StringView kSpanTagKeyApp = "app";

const static StringView kTagAgentVersionKey = "agentVersion";
const static StringView kTagAppKey = "app";
const static StringView kTagV1Value = "v1";
const static StringView kTagResourceIdKey = "resourceid";
const static StringView kTagVersionKey = "version";
const static StringView kTagClusterIdKey = "clusterId";
const static StringView kTagWorkloadNameKey = "workloadName";
const static StringView kTagWorkloadKindKey = "workloadKind";
const static StringView kTagNamespaceKey = "namespace";
const static StringView kTagHostKey = "host";
const static StringView kTagHostnameKey = "hostname";
const static StringView kTagApplicationValue = "APPLICATION";
const static StringView kTagResourceTypeKey = "resourcetype";

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

NetworkObserverManager::NetworkObserverManager(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
                                               const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                                               moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                                               const PluginMetricManagerPtr& metricManager)
    : AbstractManager(processCacheManager, eBPFAdapter, queue, metricManager),
      mAppAggregator(
          10240,
          [](std::unique_ptr<AppMetricData>& base, const std::shared_ptr<AbstractRecord>& o) {
              auto* other = static_cast<AbstractAppRecord*>(o.get());
              int statusCode = other->GetStatusCode();
              if (statusCode >= 500) {
                  base->m5xxCount += 1;
              } else if (statusCode >= 400) {
                  base->m4xxCount += 1;
              } else if (statusCode >= 300) {
                  base->m3xxCount += 1;
              } else {
                  base->m2xxCount += 1;
              }
              base->mCount++;
              base->mErrCount += other->IsError();
              base->mSlowCount += other->IsSlow();
              base->mSum += other->GetLatencySeconds();
          },
          [](const std::shared_ptr<AbstractRecord>& i,
             std::shared_ptr<SourceBuffer>& sourceBuffer) -> std::unique_ptr<AppMetricData> {
              auto* in = static_cast<AbstractAppRecord*>(i.get());
              auto spanName = sourceBuffer->CopyString(in->GetSpanName());
              auto connection = in->GetConnection();
              if (!connection) {
                  LOG_WARNING(sLogger, ("connection is null", ""));
                  return nullptr;
              }
              auto data
                  = std::make_unique<AppMetricData>(connection, sourceBuffer, StringView(spanName.data, spanName.size));

              const auto& ctAttrs = connection->GetConnTrackerAttrs();
              {
                  auto appId = sourceBuffer->CopyString(ctAttrs.Get<kAppIdIndex>());
                  data->mTags.SetNoCopy<kAppId>(StringView(appId.data, appId.size));

                  auto appName = sourceBuffer->CopyString(ctAttrs.Get<kAppNameIndex>());
                  data->mTags.SetNoCopy<kAppName>(StringView(appName.data, appName.size));

                  auto host = sourceBuffer->CopyString(ctAttrs.Get<kHostNameIndex>());
                  data->mTags.SetNoCopy<kHostName>(StringView(host.data, host.size));

                  auto ip = sourceBuffer->CopyString(ctAttrs.Get<kIp>());
                  data->mTags.SetNoCopy<kIp>(StringView(ip.data, ip.size));
              }

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
          [](std::unique_ptr<NetMetricData>& base, const std::shared_ptr<AbstractRecord>& o) {
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
          [](const std::shared_ptr<AbstractRecord>& i, std::shared_ptr<SourceBuffer>& sourceBuffer) {
              auto* in = static_cast<ConnStatsRecord*>(i.get());
              auto connection = in->GetConnection();
              auto data = std::make_unique<NetMetricData>(connection, sourceBuffer);
              const auto& ctAttrs = connection->GetConnTrackerAttrs();

              {
                  auto appId = sourceBuffer->CopyString(ctAttrs.Get<kAppIdIndex>());
                  data->mTags.SetNoCopy<kAppId>(StringView(appId.data, appId.size));

                  auto appName = sourceBuffer->CopyString(ctAttrs.Get<kAppNameIndex>());
                  data->mTags.SetNoCopy<kAppName>(StringView(appName.data, appName.size));

                  auto host = sourceBuffer->CopyString(ctAttrs.Get<kHostNameIndex>());
                  data->mTags.SetNoCopy<kHostName>(StringView(host.data, host.size));

                  auto ip = sourceBuffer->CopyString(ctAttrs.Get<kIp>());
                  data->mTags.SetNoCopy<kIp>(StringView(ip.data, ip.size));
              }

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
          [](std::unique_ptr<AppSpanGroup>& base, const std::shared_ptr<AbstractRecord>& other) {
              base->mRecords.push_back(other);
          },
          [](const std::shared_ptr<AbstractRecord>&, std::shared_ptr<SourceBuffer>&) {
              return std::make_unique<AppSpanGroup>();
          }),
      mLogAggregator(
          1024, // 1024 log per second
          [](std::unique_ptr<AppLogGroup>& base, const std::shared_ptr<AbstractRecord>& other) {
              base->mRecords.push_back(other);
          },
          [](const std::shared_ptr<AbstractRecord>&, std::shared_ptr<SourceBuffer>&) {
              return std::make_unique<AppLogGroup>();
          }) {
    if (mMetricMgr) {
        // init metrics
        MetricLabels connectionNumLabels = {{METRIC_LABEL_KEY_EVENT_SOURCE, METRIC_LABEL_VALUE_EVENT_SOURCE_EBPF}};
        auto ref = mMetricMgr->GetOrCreateReentrantMetricsRecordRef(connectionNumLabels);
        mRefAndLabels.emplace_back(connectionNumLabels);
        mConnectionNum = ref->GetIntGauge(METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_CONNECTION_NUM);

        MetricLabels appLabels = {{METRIC_LABEL_KEY_RECORD_TYPE, METRIC_LABEL_VALUE_RECORD_TYPE_APP}};
        ref = mMetricMgr->GetOrCreateReentrantMetricsRecordRef(appLabels);
        mRefAndLabels.emplace_back(appLabels);
        mAppMetaAttachSuccessTotal = ref->GetCounter(METRIC_PLUGIN_EBPF_META_ATTACH_SUCCESS_TOTAL);
        mAppMetaAttachFailedTotal = ref->GetCounter(METRIC_PLUGIN_EBPF_META_ATTACH_FAILED_TOTAL);
        mAppMetaAttachRollbackTotal = ref->GetCounter(METRIC_PLUGIN_EBPF_META_ATTACH_ROLLBACK_TOTAL);

        MetricLabels netLabels = {{METRIC_LABEL_KEY_RECORD_TYPE, METRIC_LABEL_VALUE_RECORD_TYPE_NET}};
        ref = mMetricMgr->GetOrCreateReentrantMetricsRecordRef(netLabels);
        mRefAndLabels.emplace_back(netLabels);
        mNetMetaAttachSuccessTotal = ref->GetCounter(METRIC_PLUGIN_EBPF_META_ATTACH_SUCCESS_TOTAL);
        mNetMetaAttachFailedTotal = ref->GetCounter(METRIC_PLUGIN_EBPF_META_ATTACH_FAILED_TOTAL);
        mNetMetaAttachRollbackTotal = ref->GetCounter(METRIC_PLUGIN_EBPF_META_ATTACH_ROLLBACK_TOTAL);

        MetricLabels eventTypeLabels = {{METRIC_LABEL_KEY_EVENT_TYPE, METRIC_LABEL_VALUE_EVENT_TYPE_METRIC}};
        ref = mMetricMgr->GetOrCreateReentrantMetricsRecordRef(eventTypeLabels);
        mRefAndLabels.emplace_back(eventTypeLabels);
        mPushMetricsTotal = ref->GetCounter(METRIC_PLUGIN_OUT_EVENTS_TOTAL);
        mPushMetricGroupTotal = ref->GetCounter(METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL);

        eventTypeLabels = {{METRIC_LABEL_KEY_EVENT_TYPE, METRIC_LABEL_VALUE_EVENT_TYPE_TRACE}};
        mRefAndLabels.emplace_back(eventTypeLabels);
        ref = mMetricMgr->GetOrCreateReentrantMetricsRecordRef(eventTypeLabels);
        mPushSpansTotal = ref->GetCounter(METRIC_PLUGIN_OUT_EVENTS_TOTAL);
        mPushSpanGroupTotal = ref->GetCounter(METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL);
    }
}

std::array<size_t, 2>
NetworkObserverManager::GenerateAggKeyForNetMetric(const std::shared_ptr<AbstractRecord>& abstractRecord) {
    auto* record = static_cast<ConnStatsRecord*>(abstractRecord.get());
    // calculate agg key
    std::array<size_t, 2> result{};
    result.fill(0UL);
    std::hash<std::string_view> hasher;
    auto connection = record->GetConnection();
    if (!connection) {
        LOG_WARNING(sLogger, ("connection is null", ""));
        return {};
    }

    const auto& connTrackerAttrs = connection->GetConnTrackerAttrs();

    // level0: hostname hostip appId appName, if it's not arms app, we need set default appname ...
    // kConnTrackerTable.ColIndex();
    // level1: namespace workloadkind workloadname peerNamespace peerWorkloadKind peerWorkloadName
    static constexpr auto kIdxes0 = {kAppIdIndex, kAppNameIndex, kHostNameIndex, kHostIpIndex};
    static constexpr auto kIdxes1 = {kWorkloadKindIndex,
                                     kWorkloadNameIndex,
                                     kNamespaceIndex,
                                     kPeerWorkloadKindIndex,
                                     kPeerWorkloadNameIndex,
                                     kPeerNamespaceIndex};

    for (const auto& x : kIdxes0) {
        std::string_view attr(connTrackerAttrs[x].data(), connTrackerAttrs[x].size());
        AttrHashCombine(result[0], hasher(attr));
    }
    for (const auto& x : kIdxes1) {
        std::string_view attr(connTrackerAttrs[x].data(), connTrackerAttrs[x].size());
        AttrHashCombine(result[1], hasher(attr));
    }
    return result;
}

std::array<size_t, 2>
NetworkObserverManager::GenerateAggKeyForAppMetric(const std::shared_ptr<AbstractRecord>& abstractRecord) {
    auto* record = static_cast<AbstractAppRecord*>(abstractRecord.get());
    // calculate agg key
    std::array<size_t, 2> result{};
    result.fill(0UL);
    std::hash<std::string_view> hasher;
    auto connection = record->GetConnection();
    if (!connection) {
        LOG_WARNING(sLogger, ("connection is null", ""));
        return {};
    }

    static constexpr std::array<uint32_t, 4> kIdxes0 = {kAppIdIndex, kAppNameIndex, kHostNameIndex, kHostIpIndex};
    static constexpr std::array<uint32_t, 9> kIdxes1 = {kWorkloadKindIndex,
                                                        kWorkloadNameIndex,
                                                        kConnTrackerTable.ColIndex(kProtocol.Name()),
                                                        kConnTrackerTable.ColIndex(kDestId.Name()),
                                                        kConnTrackerTable.ColIndex(kEndpoint.Name()),
                                                        kConnTrackerTable.ColIndex(kCallType.Name()),
                                                        kConnTrackerTable.ColIndex(kRpcType.Name()),
                                                        kConnTrackerTable.ColIndex(kCallKind.Name())};

    const auto& ctAttrs = connection->GetConnTrackerAttrs();
    for (const auto x : kIdxes0) {
        std::string_view attr(ctAttrs[x].data(), ctAttrs[x].size());
        AttrHashCombine(result[0], hasher(attr));
    }
    for (const auto x : kIdxes1) {
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
    std::array<size_t, 1> result{};
    result.fill(0UL);
    std::hash<std::string_view> hasher;
    auto connection = record->GetConnection();
    if (!connection) {
        LOG_WARNING(sLogger, ("connection is null", ""));
        return {};
    }
    const auto& ctAttrs = connection->GetConnTrackerAttrs();
    static constexpr auto kIdxes = {kAppIdIndex, kAppNameIndex, kHostNameIndex, kHostIpIndex};
    for (const auto& x : kIdxes) {
        std::string_view attr(ctAttrs[x].data(), ctAttrs[x].size());
        AttrHashCombine(result[0], hasher(attr));
    }

    return result;
}

std::array<size_t, 1>
NetworkObserverManager::GenerateAggKeyForLog(const std::shared_ptr<AbstractRecord>& abstractRecord) {
    auto* record = static_cast<AbstractAppRecord*>(abstractRecord.get());
    // just appid
    std::array<size_t, 1> result{};
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

bool NetworkObserverManager::updateParsers(const std::vector<std::string>& protocols,
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

bool NetworkObserverManager::ConsumeLogAggregateTree(const std::chrono::steady_clock::time_point&) { // handler
    if (!this->mFlag || this->mSuspendFlag) {
        return false;
    }
#ifdef APSARA_UNIT_TEST_MAIN
    mExecTimes++;
#endif

    WriteLock lk(mLogAggLock);
    SIZETAggTree<AppLogGroup, std::shared_ptr<AbstractRecord>> aggTree = this->mLogAggregator.GetAndReset();
    lk.unlock();

    auto nodes = aggTree.GetNodesWithAggDepth(1);
    LOG_DEBUG(sLogger, ("enter log aggregator ...", nodes.size())("node size", aggTree.NodeCount()));
    if (nodes.empty()) {
        LOG_DEBUG(sLogger, ("empty nodes...", "")("node size", aggTree.NodeCount()));
        return true;
    }

    for (auto& node : nodes) {
        // convert to a item and push to process queue
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer); // per node represent an APP ...
        eventGroup.SetTagNoCopy(kDataType.LogKey(), kLogValue);
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
                    const auto& ctAttrs = ct->GetConnTrackerAttrs();
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
                auto* httpRecord = static_cast<HttpRecord*>(record);
                auto timeSpec = ConvertKernelTimeToUnixTime(httpRecord->GetStartTimeStamp());
                logEvent->SetTimestamp(timeSpec.tv_sec, timeSpec.tv_nsec);
                logEvent->SetContent(kLatencyNS.LogKey(), std::to_string(httpRecord->GetLatencyNs()));
                logEvent->SetContent(kHTTPMethod.LogKey(), httpRecord->GetMethod());
                logEvent->SetContent(kHTTPPath.LogKey(),
                                     httpRecord->GetRealPath().size() ? httpRecord->GetRealPath()
                                                                      : httpRecord->GetPath());
                logEvent->SetContent(kHTTPVersion.LogKey(), httpRecord->GetProtocolVersion());
                logEvent->SetContent(kStatusCode.LogKey(), std::to_string(httpRecord->GetStatusCode()));
                logEvent->SetContent(kHTTPReqBody.LogKey(), httpRecord->GetReqBody());
                logEvent->SetContent(kHTTPRespBody.LogKey(), httpRecord->GetRespBody());
                LOG_DEBUG(sLogger, ("add one log, log timestamp", timeSpec.tv_sec)("nano", timeSpec.tv_nsec));
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

static constexpr std::array kSNetStateStrings = {StringView("UNKNOWN"),
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

static constexpr StringView kDefaultNetAppName = "__default_app_name__";
static constexpr StringView kDefaultNetAppId = "__default_app_id__";

bool NetworkObserverManager::ConsumeNetMetricAggregateTree(const std::chrono::steady_clock::time_point&) { // handler
    if (!this->mFlag || this->mSuspendFlag) {
        return false;
    }
#ifdef APSARA_UNIT_TEST_MAIN
    mExecTimes++;
#endif

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
        eventGroup.SetTagNoCopy(kAppType.MetricKey(), kEBPFValue);
        eventGroup.SetTagNoCopy(kDataType.MetricKey(), kMetricValue);
        eventGroup.SetTag(kTagClusterIdKey, mClusterId);

        bool init = false;
        aggTree.ForEach(node, [&](const NetMetricData* group) {
            LOG_DEBUG(sLogger,
                      ("dump group attrs", group->ToString())("ct attrs", group->mConnection->DumpConnection()));
            if (!init) {
                eventGroup.SetTagNoCopy(kAppId.MetricKey(), group->mTags.Get<kAppId>());
                eventGroup.SetTagNoCopy(kAppName.MetricKey(), group->mTags.Get<kAppName>());
                eventGroup.SetTagNoCopy(kIp.MetricKey(), group->mTags.Get<kIp>()); // pod ip
                eventGroup.SetTagNoCopy(kHostName.MetricKey(), group->mTags.Get<kIp>()); // pod name
                init = true;
            }

            std::vector<MetricEvent*> metrics;
            if (group->mDropCount > 0) {
                auto* tcpDropMetric = eventGroup.AddMetricEvent();
                tcpDropMetric->SetName(kMetricNameTcpDropTotal);
                tcpDropMetric->SetValue(UntypedSingleValue{double(group->mDropCount)});
                metrics.push_back(tcpDropMetric);
            }

            if (group->mRetransCount > 0) {
                auto* tcpRetxMetric = eventGroup.AddMetricEvent();
                tcpRetxMetric->SetName(kMetricNameTcpRetransTotal);
                tcpRetxMetric->SetValue(UntypedSingleValue{double(group->mRetransCount)});
                metrics.push_back(tcpRetxMetric);
            }

            if (group->mRttCount > 0) {
                auto* tcpRttAvg = eventGroup.AddMetricEvent();
                tcpRttAvg->SetName(kMetricNameTcpRttAvg);
                tcpRttAvg->SetValue(UntypedSingleValue{(group->mRtt * 1.0) / group->mRttCount});
                metrics.push_back(tcpRttAvg);
            }

            if (group->mRecvBytes > 0) {
                auto* tcpRxBytes = eventGroup.AddMetricEvent();
                tcpRxBytes->SetName(kMetricNameTcpRecvBytesTotal);
                tcpRxBytes->SetValue(UntypedSingleValue{double(group->mRecvBytes)});
                metrics.push_back(tcpRxBytes);
            }

            if (group->mRecvPkts > 0) {
                auto* tcpRxPkts = eventGroup.AddMetricEvent();
                tcpRxPkts->SetName(kMetricNameTcpRecvPktsTotal);
                tcpRxPkts->SetValue(UntypedSingleValue{double(group->mRecvPkts)});
                metrics.push_back(tcpRxPkts);
            }

            if (group->mSendBytes > 0) {
                auto* tcpTxBytes = eventGroup.AddMetricEvent();
                tcpTxBytes->SetName(kMetricNameTcpSentBytesTotal);
                tcpTxBytes->SetValue(UntypedSingleValue{double(group->mSendBytes)});
                metrics.push_back(tcpTxBytes);
            }

            if (group->mSendPkts > 0) {
                auto* tcpTxPkts = eventGroup.AddMetricEvent();
                tcpTxPkts->SetName(kMetricNameTcpSentPktsTotal);
                tcpTxPkts->SetValue(UntypedSingleValue{double(group->mSendPkts)});
                metrics.push_back(tcpTxPkts);
            }

            for (size_t zz = 0; zz < LC_TCP_MAX_STATES; zz++) {
                if (group->mStateCounts[zz] > 0) {
                    auto* tcpCount = eventGroup.AddMetricEvent();
                    tcpCount->SetName(kMetricNameTcpConnTotal);
                    tcpCount->SetValue(UntypedSingleValue{double(group->mStateCounts[zz])});
                    tcpCount->SetTagNoCopy(kState.MetricKey(), kSNetStateStrings[zz]);
                    metrics.push_back(tcpCount);
                }
            }

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

bool NetworkObserverManager::ConsumeMetricAggregateTree(const std::chrono::steady_clock::time_point&) { // handler
    if (!this->mFlag || this->mSuspendFlag) {
        return false;
    }
#ifdef APSARA_UNIT_TEST_MAIN
    mExecTimes++;
#endif

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
        eventGroup.SetTagNoCopy(kAppType.MetricKey(), kEBPFValue);
        eventGroup.SetTagNoCopy(kDataType.MetricKey(), kMetricValue);

        bool needPush = false;

        bool init = false;
        aggTree.ForEach(node, [&](const AppMetricData* group) {
            LOG_DEBUG(sLogger,
                      ("dump group attrs", group->ToString())("ct attrs", group->mConnection->DumpConnection()));
            // instance dim
            if (group->mTags.Get<kAppId>().size() || mAppId.size()) {
                needPush = true;
            }

            if (!init) {
                if (group->mTags.Get<kAppId>().size()) {
                    eventGroup.SetTagNoCopy(kAppId.MetricKey(), group->mTags.Get<kAppId>());
                    eventGroup.SetTagNoCopy(kAppName.MetricKey(), group->mTags.Get<kAppName>());
                    eventGroup.SetTagNoCopy(kIp.MetricKey(), group->mTags.Get<kIp>()); // pod ip
                    eventGroup.SetTagNoCopy(kHostName.MetricKey(), group->mTags.Get<kIp>()); // pod ip
                } else {
                    LOG_DEBUG(sLogger, ("no app id retrieve from metadata", "use configure"));
                    eventGroup.SetTag(kAppId.MetricKey(), mAppId);
                    eventGroup.SetTag(kAppName.MetricKey(), mAppName);
                    if (mHostIp.empty()) {
                        eventGroup.SetTagNoCopy(kIp.MetricKey(), group->mTags.Get<kIp>()); // pod ip
                        eventGroup.SetTagNoCopy(kHostName.MetricKey(), group->mTags.Get<kIp>()); // pod name
                    } else {
                        eventGroup.SetTag(kIp.MetricKey(), mHostIp); // pod ip
                        eventGroup.SetTag(kHostName.MetricKey(), mHostIp); // pod name
                    }
                }

                auto* tagMetric = eventGroup.AddMetricEvent();
                tagMetric->SetName(kMetricNameTag);
                tagMetric->SetValue(UntypedSingleValue{1.0});
                tagMetric->SetTimestamp(seconds, 0);
                tagMetric->SetTagNoCopy(kTagAgentVersionKey, kTagV1Value);
                tagMetric->SetTagNoCopy(kTagAppKey, group->mTags.Get<kAppName>()); // app ===> appname
                tagMetric->SetTagNoCopy(kTagResourceIdKey, group->mTags.Get<kAppId>()); // resourceid -==> pid
                tagMetric->SetTagNoCopy(kTagResourceTypeKey, kTagApplicationValue); // resourcetype ===> APPLICATION
                tagMetric->SetTagNoCopy(kTagVersionKey, kTagV1Value); // version ===> v1
                tagMetric->SetTagNoCopy(kTagClusterIdKey,
                                        mClusterId); // clusterId ===> TODO read from env _cluster_id_
                tagMetric->SetTagNoCopy(kTagHostKey, group->mTags.Get<kIp>()); // host ===>
                tagMetric->SetTagNoCopy(kTagHostnameKey, group->mTags.Get<kHostName>()); // hostName ===>
                tagMetric->SetTagNoCopy(kTagNamespaceKey, group->mTags.Get<kNamespace>()); // namespace ===>
                tagMetric->SetTagNoCopy(kTagWorkloadKindKey, group->mTags.Get<kWorkloadKind>()); // workloadKind ===>
                tagMetric->SetTagNoCopy(kTagWorkloadNameKey, group->mTags.Get<kWorkloadName>()); // workloadName ===>
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
                requestsMetric->SetName(kMetricNameRequestTotal);
                requestsMetric->SetValue(UntypedSingleValue{double(group->mCount)});
                metrics.push_back(requestsMetric);

                auto* latencyMetric = eventGroup.AddMetricEvent();
                latencyMetric->SetName(kMetricNameRequestDurationSum);
                latencyMetric->SetValue(UntypedSingleValue{double(group->mSum)});
                metrics.push_back(latencyMetric);
            }
            if (group->mErrCount) {
                auto* errorMetric = eventGroup.AddMetricEvent();
                errorMetric->SetName(kMetricNameRequestErrorTotal);
                errorMetric->SetValue(UntypedSingleValue{double(group->mErrCount)});
                metrics.push_back(errorMetric);
            }
            if (group->mSlowCount) {
                auto* slowMetric = eventGroup.AddMetricEvent();
                slowMetric->SetName(kMetricNameRequestSlowTotal);
                slowMetric->SetValue(UntypedSingleValue{double(group->mSlowCount)});
                metrics.push_back(slowMetric);
            }

            if (group->m2xxCount) {
                auto* statusMetric = eventGroup.AddMetricEvent();
                statusMetric->SetValue(UntypedSingleValue{double(group->m2xxCount)});
                statusMetric->SetName(kMetricNameRequestByStatusTotal);
                statusMetric->SetTagNoCopy(kStatusCode.MetricKey(), kStatus2xxKey);
                metrics.push_back(statusMetric);
            }
            if (group->m3xxCount) {
                auto* statusMetric = eventGroup.AddMetricEvent();
                statusMetric->SetValue(UntypedSingleValue{double(group->m3xxCount)});
                statusMetric->SetName(kMetricNameRequestByStatusTotal);
                statusMetric->SetTagNoCopy(kStatusCode.MetricKey(), kStatus3xxKey);
                metrics.push_back(statusMetric);
            }
            if (group->m4xxCount) {
                auto* statusMetric = eventGroup.AddMetricEvent();
                statusMetric->SetValue(UntypedSingleValue{double(group->m4xxCount)});
                statusMetric->SetName(kMetricNameRequestByStatusTotal);
                statusMetric->SetTagNoCopy(kStatusCode.MetricKey(), kStatus4xxKey);
                metrics.push_back(statusMetric);
            }
            if (group->m5xxCount) {
                auto* statusMetric = eventGroup.AddMetricEvent();
                statusMetric->SetValue(UntypedSingleValue{double(group->m5xxCount)});
                statusMetric->SetName(kMetricNameRequestByStatusTotal);
                statusMetric->SetTagNoCopy(kStatusCode.MetricKey(), kStatus5xxKey);
                metrics.push_back(statusMetric);
            }

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

static constexpr StringView kSpanHostAttrKey = "host";

bool NetworkObserverManager::ConsumeSpanAggregateTree(const std::chrono::steady_clock::time_point&) { // handler
    if (!this->mFlag || this->mSuspendFlag) {
        return false;
    }
#ifdef APSARA_UNIT_TEST_MAIN
    mExecTimes++;
#endif

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
                const auto& ctAttrs = ct->GetConnTrackerAttrs();

                if ((mAppName.empty() && ctAttrs.Get<kAppNameIndex>().empty()) || !ct) {
                    LOG_DEBUG(sLogger,
                              ("no app name or ct null, skip, spanname ", record->GetSpanName())(
                                  "appname", ctAttrs.Get<kAppNameIndex>())("ct null", ct == nullptr));
                    continue;
                }

                if (!init) {
                    if (ctAttrs.Get<kAppNameIndex>().size()) {
                        // set app attrs ...
                        auto appName = sourceBuffer->CopyString(ctAttrs.Get<kAppNameIndex>());
                        eventGroup.SetTagNoCopy(kAppName.SpanKey(), StringView(appName.data, appName.size)); // app name
                        auto appId = sourceBuffer->CopyString(ctAttrs.Get<kAppIdIndex>());
                        eventGroup.SetTagNoCopy(kAppId.SpanKey(), StringView(appId.data, appId.size)); // app id
                        auto podIp = sourceBuffer->CopyString(ctAttrs.Get<kIp>());
                        eventGroup.SetTagNoCopy(kHostIp.SpanKey(), StringView(podIp.data, podIp.size)); // pod ip
                        eventGroup.SetTagNoCopy(kHostName.SpanKey(), StringView(podIp.data, podIp.size)); // pod name
                    } else {
                        LOG_DEBUG(sLogger, ("no app id retrieve from metadata", "use configure"));
                        eventGroup.SetTag(kAppId.SpanKey(), mAppId);
                        eventGroup.SetTag(kAppName.SpanKey(), mAppName);
                        if (mHostIp.empty()) {
                            eventGroup.SetTagNoCopy(kIp.SpanKey(), ctAttrs.Get<kIp>()); // pod ip
                            eventGroup.SetTagNoCopy(kHostName.SpanKey(), ctAttrs.Get<kIp>()); // pod ip
                        } else {
                            eventGroup.SetTag(kIp.SpanKey(), mHostIp); // pod ip
                            eventGroup.SetTag(kHostName.SpanKey(), mHostIp); // pod name
                        }
                    }
                    eventGroup.SetTagNoCopy(kAppType.SpanKey(), kEBPFValue);
                    eventGroup.SetTagNoCopy(kDataType.SpanKey(), kTraceValue);
                    for (auto tag = eventGroup.GetTags().begin(); tag != eventGroup.GetTags().end(); tag++) {
                        LOG_DEBUG(sLogger, ("record span tags", "")(std::string(tag->first), std::string(tag->second)));
                    }
                    init = true;
                }
                auto* spanEvent = eventGroup.AddSpanEvent();
                auto workloadName = sourceBuffer->CopyString(ctAttrs.Get<kWorkloadName>());
                // TODO @qianlu.kk
                spanEvent->SetTagNoCopy(kSpanTagKeyApp, StringView(workloadName.data, workloadName.size));

                // attr.host, adjust to old logic ...
                auto host = sourceBuffer->CopyString(ctAttrs.Get<kIp>());
                spanEvent->SetTag(kSpanHostAttrKey, StringView(host.data, host.size));

                for (size_t i = 0; i < kConnTrackerElementsTableSize; i++) {
                    auto sb = sourceBuffer->CopyString(ctAttrs[i]);
                    spanEvent->SetTagNoCopy(kConnTrackerTable.ColSpanKey(i), StringView(sb.data, sb.size));
                    LOG_DEBUG(sLogger, ("record span tags", "")(std::string(kConnTrackerTable.ColSpanKey(i)), sb.data));
                }

                spanEvent->SetTraceId(TraceIDToString(record->mTraceId));
                spanEvent->SetSpanId(SpanIDToString(record->mSpanId));
                spanEvent->SetStatus(record->IsError() ? SpanEvent::StatusCode::Error : SpanEvent::StatusCode::Ok);
                auto role = ct->GetRole();
                if (role == support_role_e::IsClient) {
                    spanEvent->SetKind(SpanEvent::Kind::Client);
                } else if (role == support_role_e::IsServer) {
                    spanEvent->SetKind(SpanEvent::Kind::Server);
                } else {
                    spanEvent->SetKind(SpanEvent::Kind::Unspecified);
                }

                spanEvent->SetName(record->GetSpanName());
                spanEvent->SetTag(kHTTPReqBody.SpanKey(), record->GetReqBody());
                spanEvent->SetTag(kHTTPRespBody.SpanKey(), record->GetRespBody());
                spanEvent->SetTag(kHTTPReqBodySize.SpanKey(), std::to_string(record->GetReqBodySize()));
                spanEvent->SetTag(kHTTPRespBodySize.SpanKey(), std::to_string(record->GetRespBodySize()));
                spanEvent->SetTag(kHTTPVersion.SpanKey(), record->GetProtocolVersion());

                struct timespec startTime = ConvertKernelTimeToUnixTime(record->GetStartTimeStamp());
                struct timespec endTime = ConvertKernelTimeToUnixTime(record->GetEndTimeStamp());
                spanEvent->SetStartTimeNs(startTime.tv_sec * 1000000000 + startTime.tv_nsec);
                spanEvent->SetEndTimeNs(endTime.tv_sec * 1000000000 + endTime.tv_nsec);
                spanEvent->SetTimestamp(startTime.tv_sec, startTime.tv_nsec);
                LOG_DEBUG(sLogger,
                          ("add one span, startTs", startTime.tv_sec * 1000000000 + startTime.tv_nsec)(
                              "entTs", endTime.tv_sec * 1000000000 + endTime.tv_nsec));
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

std::string GetLastPathSegment(const std::string& path) {
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return path; // No '/' found, return the entire string
    }
    return path.substr(pos + 1); // Return the substring after the last '/'
}

int GuessContainerIdOffset() {
    static const std::string kCgroupFilePath = "/proc/self/cgroup";
    std::string containerId;
    return ProcParser::GetContainerId(kCgroupFilePath, containerId);
}

int NetworkObserverManager::Update(
    [[maybe_unused]] const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) {
    auto* opt = std::get<ObserverNetworkOption*>(options);

    // diff opt
    if (mPreviousOpt) {
        compareAndUpdate("EnableLog", mPreviousOpt->mEnableLog, opt->mEnableLog, [this](bool oldValue, bool newValue) {
            this->mEnableLog = newValue;
        });
        compareAndUpdate("EnableMetric", mPreviousOpt->mEnableMetric, opt->mEnableMetric, [this](bool, bool newValue) {
            this->mEnableMetric = newValue;
        });
        compareAndUpdate("EnableSpan", mPreviousOpt->mEnableSpan, opt->mEnableSpan, [this](bool, bool newValue) {
            this->mEnableSpan = newValue;
        });
        compareAndUpdate("SampleRate", mPreviousOpt->mSampleRate, opt->mSampleRate, [this](double, double newValue) {
            if (newValue < 0) {
                LOG_WARNING(sLogger, ("invalid sample rate, must between [0, 1], use default 0.01, given", newValue));
                newValue = 0;
            } else if (newValue >= 1) {
                newValue = 1.0;
            }
            WriteLock lk(mSamplerLock);
            mSampler = std::make_shared<HashRatioSampler>(newValue);
        });
        compareAndUpdate("EnableProtocols",
                         mPreviousOpt->mEnableProtocols,
                         opt->mEnableProtocols,
                         [this](const std::vector<std::string>& oldValue, const std::vector<std::string>& newValue) {
                             this->updateParsers(newValue, oldValue);
                         });
        compareAndUpdate("MaxConnections",
                         mPreviousOpt->mMaxConnections,
                         opt->mMaxConnections,
                         [this](const int&, const int& newValue) {
                             this->mConnectionManager->UpdateMaxConnectionThreshold(newValue);
                         });
    }

    // update previous opt
    mPreviousOpt = std::make_unique<ObserverNetworkOption>(*opt);

    return 0;
}

int NetworkObserverManager::Init(const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) {
    if (mFlag) {
        return 0;
    }
    auto* opt = std::get<ObserverNetworkOption*>(options);
    if (!opt) {
        LOG_ERROR(sLogger, ("invalid options", ""));
        return -1;
    }

    updateParsers(opt->mEnableProtocols, {});

    mFlag = true;

    mConnectionManager = ConnectionManager::Create();
    mConnectionManager->SetConnStatsStatus(!opt->mDisableConnStats);
    mConnectionManager->UpdateMaxConnectionThreshold(opt->mMaxConnections);
    mConnectionManager->RegisterConnStatsFunc(
        [this](std::shared_ptr<AbstractRecord>& record) { processRecord(record); });

    mAppId = opt->mAppId;
    mAppName = opt->mAppName;
    mHostName = opt->mHostName;
    mHostIp = opt->mHostIp;

    mPollKernelFreqMgr.SetPeriod(std::chrono::milliseconds(200));
    mConsumerFreqMgr.SetPeriod(std::chrono::milliseconds(300));

    mCidOffset = GuessContainerIdOffset();

    const char* value = getenv("_cluster_id_");
    if (value != nullptr) {
        mClusterId = value;
    }

    auto now = std::chrono::steady_clock::now();
    std::shared_ptr<ScheduleConfig> metricConfig
        = std::make_shared<NetworkObserverScheduleConfig>(std::chrono::seconds(15), JobType::METRIC_AGG);
    std::shared_ptr<ScheduleConfig> spanConfig
        = std::make_shared<NetworkObserverScheduleConfig>(std::chrono::seconds(2), JobType::SPAN_AGG);
    std::shared_ptr<ScheduleConfig> logConfig
        = std::make_shared<NetworkObserverScheduleConfig>(std::chrono::seconds(2), JobType::LOG_AGG);
    ScheduleNext(now, metricConfig);
    ScheduleNext(now, spanConfig);
    ScheduleNext(now, logConfig);

    // init sampler
    {
        if (opt->mSampleRate < 0) {
            LOG_WARNING(sLogger,
                        ("invalid sample rate, must between [0, 1], use default 0.01, given", opt->mSampleRate));
            opt->mSampleRate = 0;
        } else if (opt->mSampleRate >= 1) {
            opt->mSampleRate = 1.0;
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
    config.mStatsHandler = [](void* customData, struct conn_stats_event_t* event) {
        if (!event) {
            LOG_ERROR(sLogger, ("event is null", ""));
            return;
        }
        auto* mgr = static_cast<NetworkObserverManager*>(customData);
        if (mgr) {
            mgr->mRecvConnStatEventsTotal.fetch_add(1);
            mgr->AcceptNetStatsEvent(event);
        }
    };

    config.mDataHandler = [](void* customData, struct conn_data_event_t* event) {
        if (!event) {
            LOG_ERROR(sLogger, ("event is null", ""));
            return;
        }
        auto* mgr = static_cast<NetworkObserverManager*>(customData);
        if (mgr == nullptr) {
            LOG_ERROR(sLogger, ("assert network observer handler failed", ""));
            return;
        }

        if (event->request_len == 0 || event->response_len == 0) {
            LOG_ERROR(
                sLogger,
                ("request len or response len is zero, req len", event->request_len)("resp len", event->response_len));
            return;
        }

        mgr->AcceptDataEvent(event);
    };

    config.mCtrlHandler = [](void* customData, struct conn_ctrl_event_t* event) {
        if (!event) {
            LOG_ERROR(sLogger, ("event is null", ""));
            return;
        }
        auto* mgr = static_cast<NetworkObserverManager*>(customData);
        if (!mgr) {
            LOG_ERROR(sLogger, ("assert network observer handler failed", ""));
        }

        mgr->mRecvCtrlEventsTotal.fetch_add(1);
        mgr->AcceptNetCtrlEvent(event);
    };

    config.mLostHandler = [](void* customData, enum callback_type_e type, uint64_t lostCount) {
        LOG_DEBUG(sLogger, ("========= [DUMP] net event lost, type", int(type))("count", lostCount));
        auto* mgr = static_cast<NetworkObserverManager*>(customData);
        if (!mgr) {
            LOG_ERROR(sLogger, ("assert network observer handler failed", ""));
            return;
        }
        mgr->RecordEventLost(type, lostCount);
    };

    if (K8sMetadata::GetInstance().Enable()) {
        config.mCidOffset = mCidOffset;
        config.mEnableCidFilter = true;
    }

    pc->mConfig = config;
    auto ret = mEBPFAdapter->StartPlugin(PluginType::NETWORK_OBSERVE, std::move(pc));
    if (!ret) {
        return -1;
    }

    mRollbackQueue = moodycamel::BlockingConcurrentQueue<std::shared_ptr<AbstractRecord>>(4096);

    LOG_INFO(sLogger, ("begin to start ebpf ... ", ""));
    this->mFlag = true;
    this->runInThread();

    // register update host K8s metadata task ...
    if (K8sMetadata::GetInstance().Enable()) {
        std::shared_ptr<ScheduleConfig> config
            = std::make_shared<NetworkObserverScheduleConfig>(std::chrono::seconds(5), JobType::HOST_META_UPDATE);
        ScheduleNext(now, config);
    }

    return 0;
}

bool NetworkObserverManager::UploadHostMetadataUpdateTask() {
    std::vector<std::string> keys;
    auto request = K8sMetadata::GetInstance().BuildAsyncRequest(
        keys,
        PodInfoType::HostInfo,
        []() {
            auto managerPtr = EBPFServer::GetInstance()->GetPluginManager(PluginType::NETWORK_OBSERVE);
            return managerPtr && managerPtr->IsExists();
        },
        [](const std::vector<std::string>& podIpVec) {
            auto managerPtr = EBPFServer::GetInstance()->GetPluginManager(PluginType::NETWORK_OBSERVE);
            if (managerPtr == nullptr) {
                return;
            }
            auto* networkObserverManager = static_cast<NetworkObserverManager*>(managerPtr.get());
            if (networkObserverManager) {
                networkObserverManager->HandleHostMetadataUpdate(podIpVec);
            }
        });
    AsynCurlRunner::GetInstance()->AddRequest(std::move(request));
    return true;
}

void NetworkObserverManager::HandleHostMetadataUpdate(const std::vector<std::string>& podCidVec) {
    std::vector<std::string> newContainerIds;
    std::vector<std::string> expiredContainerIds;
    std::unordered_set<std::string> currentCids;

    for (const auto& cid : podCidVec) {
        auto podInfo = K8sMetadata::GetInstance().GetInfoByContainerIdFromCache(cid);
        if (!podInfo || podInfo->mAppId == "") {
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
                  ("appId", podInfo->mAppId)("appName", podInfo->mAppName)("podIp", podInfo->mPodIp)(
                      "podName", podInfo->mPodName)("containerId", cid));
    }

    for (const auto& cid : mEnabledCids) {
        if (!currentCids.count(cid)) {
            expiredContainerIds.push_back(cid);
        }
    }

    mEnabledCids = std::move(currentCids);
    UpdateWhitelists(std::move(newContainerIds), std::move(expiredContainerIds));
}

bool NetworkObserverManager::ScheduleNext(const std::chrono::steady_clock::time_point& execTime,
                                          const std::shared_ptr<ScheduleConfig>& config) {
    auto* noConfig = static_cast<NetworkObserverScheduleConfig*>(config.get());
    if (noConfig == nullptr) {
        LOG_WARNING(sLogger, ("config is null", ""));
        return false;
    }

    std::chrono::steady_clock::time_point nextTime = execTime + config->mInterval;
    Timer::GetInstance()->PushEvent(std::make_unique<AggregateEvent>(nextTime, config));

    LOG_DEBUG(sLogger, ("exec schedule task", magic_enum::enum_name(noConfig->mJobType)));

    switch (noConfig->mJobType) {
        case JobType::METRIC_AGG: {
            ConsumeNetMetricAggregateTree(execTime);
            ConsumeMetricAggregateTree(execTime);
            break;
        }
        case JobType::SPAN_AGG: {
            ConsumeSpanAggregateTree(execTime);
            break;
        }
        case JobType::LOG_AGG: {
            ConsumeLogAggregateTree(execTime);
            break;
        }
        case JobType::HOST_META_UPDATE: {
            UploadHostMetadataUpdateTask();
            break;
        }
        default: {
            LOG_ERROR(sLogger, ("skip schedule, unknown job type", magic_enum::enum_name(noConfig->mJobType)));
            return false;
        }
    }

    return true;
}

void NetworkObserverManager::runInThread() {
    // periodically poll perf buffer ...
    LOG_INFO(sLogger, ("enter core thread ", ""));
    // start a new thread to poll perf buffer ...
    mCoreThread = std::thread(&NetworkObserverManager::PollBufferWrapper, this);
    mRecordConsume = std::thread(&NetworkObserverManager::ConsumeRecords, this);

    LOG_INFO(sLogger, ("network observer plugin installed.", ""));
}

void NetworkObserverManager::processRecordAsLog(const std::shared_ptr<AbstractRecord>& record) {
    WriteLock lk(mLogAggLock);
    auto res = mLogAggregator.Aggregate(record, GenerateAggKeyForLog(record));
    LOG_DEBUG(sLogger, ("agg res", res)("node count", mLogAggregator.NodeCount()));
}

void NetworkObserverManager::processRecordAsSpan(const std::shared_ptr<AbstractRecord>& record) {
    WriteLock lk(mSpanAggLock);
    auto res = mSpanAggregator.Aggregate(record, GenerateAggKeyForSpan(record));
    LOG_DEBUG(sLogger, ("agg res", res)("node count", mSpanAggregator.NodeCount()));
}

void NetworkObserverManager::processRecordAsMetric(const std::shared_ptr<AbstractRecord>& record) {
    WriteLock lk(mAppAggLock);
    auto res = mAppAggregator.Aggregate(record, GenerateAggKeyForAppMetric(record));
    LOG_DEBUG(sLogger, ("agg res", res)("node count", mAppAggregator.NodeCount()));
}

void NetworkObserverManager::handleRollback(const std::shared_ptr<AbstractRecord>& record, bool& drop) {
    int times = record->Rollback();
#ifdef APSARA_UNIT_TEST_MAIN
    if (times == 1) {
        mRollbackRecordTotal++;
    }
#endif
    if (times > 5) {
#ifdef APSARA_UNIT_TEST_MAIN
        mDropRecordTotal++;
#endif
        drop = true;
        LOG_WARNING(sLogger,
                    ("meta not ready, drop record, times", times)("record type",
                                                                  magic_enum::enum_name(record->GetRecordType())));
    } else {
        LOG_DEBUG(sLogger,
                  ("meta not ready, rollback record, times", times)("record type",
                                                                    magic_enum::enum_name(record->GetRecordType())));
        mRollbackQueue.try_enqueue(record);
        drop = false;
    }
}

void NetworkObserverManager::processRecord(const std::shared_ptr<AbstractRecord>& record) {
    if (!record) {
        return;
    }
    bool isDrop = false;
    switch (record->GetRecordType()) {
        case RecordType::APP_RECORD: {
            auto* appRecord = static_cast<AbstractAppRecord*>(record.get());
            if (!appRecord || !appRecord->GetConnection()) {
                // should not happen
                return;
            }

            if (!appRecord->GetConnection()->IsMetaAttachReadyForAppRecord()
                && appRecord->GetConnection()->IsConnDeleted()) {
                // try attach again, for sake of connection is released in connection manager ...
                appRecord->GetConnection()->TryAttachPeerMeta();
                appRecord->GetConnection()->TryAttachSelfMeta();
            }

            if (!appRecord->GetConnection()->IsMetaAttachReadyForAppRecord()) {
                // rollback
                handleRollback(record, isDrop);
                if (isDrop) {
                    ADD_COUNTER(mAppMetaAttachFailedTotal, 1);
                } else {
                    ADD_COUNTER(mAppMetaAttachRollbackTotal, 1);
                }
                return;
            }

            ADD_COUNTER(mAppMetaAttachSuccessTotal, 1);

            // handle record
            if (mEnableLog && record->ShouldSample()) {
                processRecordAsLog(record);
            }
            if (mEnableMetric) {
                // TODO(qianlu): add converge ...
                // aggregate ...
                processRecordAsMetric(record);
            }
            if (mEnableSpan && record->ShouldSample()) {
                processRecordAsSpan(record);
            }
            break;
        }
        case RecordType::CONN_STATS_RECORD: {
            auto* connStatsRecord = static_cast<ConnStatsRecord*>(record.get());
            if (!connStatsRecord || !connStatsRecord->GetConnection()) {
                // should not happen
                return;
            }
            if (!connStatsRecord->GetConnection()->IsMetaAttachReadyForNetRecord()) {
                // rollback
                handleRollback(record, isDrop);
                if (isDrop) {
                    ADD_COUNTER(mNetMetaAttachFailedTotal, 1);
                } else {
                    ADD_COUNTER(mNetMetaAttachRollbackTotal, 1);
                }
                return;
            }
            ADD_COUNTER(mNetMetaAttachSuccessTotal, 1);

            // handle record
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

void NetworkObserverManager::ConsumeRecords() {
    std::array<std::shared_ptr<AbstractRecord>, 4096> items;
    while (mFlag) {
        // poll event from
        auto now = std::chrono::steady_clock::now();
        auto nextWindow = mConsumerFreqMgr.Next();
        if (!mConsumerFreqMgr.Expired(now)) {
            std::this_thread::sleep_until(nextWindow);
            mConsumerFreqMgr.Reset(nextWindow);
        } else {
            mConsumerFreqMgr.Reset(now);
        }
        size_t count
            = mRollbackQueue.wait_dequeue_bulk_timed(items.data(), items.size(), std::chrono::milliseconds(200));
        LOG_DEBUG(sLogger, ("get records:", count));
        // handle ....
        if (count == 0) {
            continue;
        }

        for (size_t i = 0; i < count; i++) {
            auto& event = items[i];
            if (!event) {
                LOG_ERROR(sLogger, ("Encountered null event in RollbackQueue at index", i));
                continue;
            }
            processRecord(event);
        }

        // clear
        for (size_t i = 0; i < count; i++) {
            items[i].reset();
        }
    }
}

void NetworkObserverManager::PollBufferWrapper() {
    LOG_DEBUG(sLogger, ("enter poll perf buffer", ""));
    int32_t flag = 0;
    while (this->mFlag) {
        // poll event from
        auto now = std::chrono::steady_clock::now();
        auto nextWindow = mPollKernelFreqMgr.Next();
        if (!mPollKernelFreqMgr.Expired(now)) {
            std::this_thread::sleep_until(nextWindow);
            mPollKernelFreqMgr.Reset(nextWindow);
        } else {
            mPollKernelFreqMgr.Reset(now);
        }

        // poll stats -> ctrl -> info
        int ret = mEBPFAdapter->PollPerfBuffers(
            PluginType::NETWORK_OBSERVE, kNetObserverMaxBatchConsumeSize, &flag, kNetObserverMaxWaitTimeMS);
        if (ret < 0) {
            LOG_WARNING(sLogger, ("poll event err, ret", ret));
        }

        mConnectionManager->Iterations();
        SET_GAUGE(mConnectionNum, mConnectionManager->ConnectionTotal());

        LOG_DEBUG(
            sLogger,
            ("===== statistic =====>> total data events:",
             mRecvHttpDataEventsTotal.load())(" total conn stats events:", mRecvConnStatEventsTotal.load())(
                " total ctrl events:", mRecvCtrlEventsTotal.load())(" lost data events:", mLostDataEventsTotal.load())(
                " lost stats events:", mLostConnStatEventsTotal.load())(" lost ctrl events:",
                                                                        mLostCtrlEventsTotal.load()));
    }
}

void NetworkObserverManager::RecordEventLost(enum callback_type_e type, uint64_t lostCount) {
    ADD_COUNTER(mLossKernelEventsTotal, lostCount);
    switch (type) {
        case STAT_HAND:
            mLostConnStatEventsTotal.fetch_add(lostCount);
            return;
        case INFO_HANDLE:
            mLostDataEventsTotal.fetch_add(lostCount);
            return;
        case CTRL_HAND:
            mLostCtrlEventsTotal.fetch_add(lostCount);
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
    // atomic shared_ptr
    std::vector<std::shared_ptr<AbstractRecord>> records
        = ProtocolParserManager::GetInstance().Parse(protocol, conn, event, mSampler);
    lk.unlock();

    if (records.empty()) {
        return;
    }

    // add records to span/event generate queue
    for (auto& record : records) {
        processRecord(record);
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
    if (!mFlag) {
        return 0;
    }
    LOG_INFO(sLogger, ("prepare to destroy", ""));
    mEBPFAdapter->StopPlugin(PluginType::NETWORK_OBSERVE);
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
#ifdef APSARA_UNIT_TEST_MAIN
    mEnableCids = enableCids;
    mDisableCids = disableCids;
    return;
#endif
    for (auto& cid : enableCids) {
        LOG_INFO(sLogger, ("UpdateWhitelists cid", cid));
        mEBPFAdapter->SetNetworkObserverCidFilter(cid, true);
    }

    for (auto& cid : disableCids) {
        LOG_INFO(sLogger, ("UpdateBlacklists cid", cid));
        mEBPFAdapter->SetNetworkObserverCidFilter(cid, false);
    }
}

} // namespace logtail::ebpf

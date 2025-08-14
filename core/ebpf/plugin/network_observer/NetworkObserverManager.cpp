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

#include <cstdint>

#include "collection_pipeline/queue/ProcessQueueItem.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/HashUtil.h"
#include "common/MachineInfoUtil.h"
#include "common/StringTools.h"
#include "common/StringView.h"
#include "common/TimeKeeper.h"
#include "common/TimeUtil.h"
#include "common/http/AsynCurlRunner.h"
#include "common/magic_enum.hpp"
#include "common/version.h"
#include "ebpf/Config.h"
#include "ebpf/EBPFServer.h"
#include "ebpf/include/export.h"
#include "ebpf/protocol/ProtocolParser.h"
#include "ebpf/util/TraceId.h"
#include "logger/Logger.h"
#include "metadata/K8sMetadata.h"
#include "plugin/network_observer/HttpRetryableEvent.h"
#include "type/NetworkObserverEvent.h"
#include "type/table/BaseElements.h"

extern "C" {
#include <coolbpf/net.h>

#include <utility>
}

DEFINE_FLAG_INT32(ebpf_networkobserver_max_connections, "maximum connections", 5000);
DEFINE_FLAG_STRING(ebpf_networkobserver_enable_protocols, "enable application protocols, split by comma", "HTTP");
DEFINE_FLAG_DOUBLE(ebpf_networkobserver_default_sample_rate, "ebpf network observer default sample rate", 1.0);

namespace logtail::ebpf {

#define COPY_AND_SET_TAG(eventGroup, sourceBuffer, tagKey, value) \
    do { \
        auto _var = (sourceBuffer)->CopyString(value); \
        (eventGroup).SetTagNoCopy((tagKey), StringView{_var.data, _var.size}); \
    } while (0)


inline constexpr int kNetObserverMaxBatchConsumeSize = 4096;
inline constexpr int kNetObserverMaxWaitTimeMS = 0;
inline constexpr size_t kGlobalWorkloadKey = 0;
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
const static StringView kAPMValue = "apm";
const static StringView kMetricValue = "metric";
const static StringView kTraceValue = "trace";
const static StringView kLogValue = "log";
const static StringView kAgentInfoValue = "agent_info";

const static StringView kSpanTagKeyApp = "app";

const static StringView kTagAgentVersionKey = "agentVersion";
const static StringView kTagAppKey = "app";
const static StringView kTagV1Value = "v1";
const static StringView kTagResourceIdKey = "resourceid";
const static StringView kTagVersionKey = "version";
const static StringView kTagClusterIdKey = "clusterId";
const static StringView kTagTechnology = "technology";
const static StringView kTagWorkloadNameKey = "workloadName";
const static StringView kTagWorkloadKindKey = "workloadKind";
const static StringView kTagNamespaceKey = "namespace";
const static StringView kTagHostKey = "host";
const static StringView kTagHostnameKey = "hostname";
const static StringView kTagApplicationValue = "APPLICATION";
const static StringView kTagResourceTypeKey = "resourcetype";
const static StringView kTagCmsWorkspaceKey = "acsCmsWorkspace";
const static StringView kTagArmsServiceIdKey = "acsArmsServiceId";

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

std::shared_ptr<AppDetail>
GetAppDetail(const std::unordered_map<size_t, std::shared_ptr<AppDetail>>& currentContainerConfigs, size_t key) {
    const auto& it = currentContainerConfigs.find(key);
    if (it != currentContainerConfigs.end()) {
        return it->second;
    }

    const auto it2 = currentContainerConfigs.find(kGlobalWorkloadKey); // for global config
    if (it2 != currentContainerConfigs.end()) {
        LOG_DEBUG(sLogger, ("use cluster default config, origin containerIdKey", key));
        return it2->second;
    }
    return nullptr;
}

NetworkObserverManager::NetworkObserverManager(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
                                               const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                                               moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue)
    : AbstractManager(processCacheManager, eBPFAdapter, queue),
      mAppAggregator(
          10240,
          [](std::unique_ptr<AppMetricData>& base, L7Record* other) {
              if (base == nullptr) {
                  return;
              }
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
          [this](L7Record* in, std::shared_ptr<SourceBuffer>& sourceBuffer) -> std::unique_ptr<AppMetricData> {
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
                  auto appConfig = getAppConfigFromReplica(connection); // build func is called by poller thread ...
                  if (appConfig == nullptr) {
                      return nullptr;
                  }
                  auto host = sourceBuffer->CopyString(ctAttrs.Get<kHostNameIndex>());
                  data->mTags.SetNoCopy<kHostName>(StringView(host.data, host.size));

                  auto ip = sourceBuffer->CopyString(ctAttrs.Get<kIp>());
                  data->mTags.SetNoCopy<kIp>(StringView(ip.data, ip.size));

                  auto appId = sourceBuffer->CopyString(appConfig->mAppId);
                  data->mTags.SetNoCopy<kAppId>(StringView(appId.data, appId.size));

                  auto appName = sourceBuffer->CopyString(appConfig->mAppName);
                  data->mTags.SetNoCopy<kAppName>(StringView(appName.data, appName.size));

                  auto workspace = sourceBuffer->CopyString(appConfig->mWorkspace);
                  data->mTags.SetNoCopy<kAppName>(StringView(workspace.data, workspace.size));

                  auto serviceId = sourceBuffer->CopyString(appConfig->mServiceId);
                  data->mTags.SetNoCopy<kArmsServiceId>(StringView(serviceId.data, serviceId.size));
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
          [](std::unique_ptr<NetMetricData>& base, ConnStatsRecord* other) {
              if (base == nullptr) {
                  return;
              }
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
          [this](ConnStatsRecord* in, std::shared_ptr<SourceBuffer>& sourceBuffer) -> std::unique_ptr<NetMetricData> {
              auto connection = in->GetConnection();
              if (!connection) {
                  LOG_WARNING(sLogger, ("connection is null", ""));
                  return nullptr;
              }
              auto appConfig = getAppConfigFromReplica(connection); // build func is called by poller thread ...
              if (appConfig == nullptr) {
                  LOG_WARNING(sLogger, ("appConfig is null", ""));
                  return nullptr;
              }
              auto data = std::make_unique<NetMetricData>(connection, sourceBuffer);
              const auto& ctAttrs = connection->GetConnTrackerAttrs();

              {
                  auto appId = sourceBuffer->CopyString(appConfig->mAppId);
                  data->mTags.SetNoCopy<kAppId>(StringView(appId.data, appId.size));

                  auto appName = sourceBuffer->CopyString(appConfig->mAppName);
                  data->mTags.SetNoCopy<kAppName>(StringView(appName.data, appName.size));

                  auto serviceId = sourceBuffer->CopyString(appConfig->mServiceId);
                  data->mTags.SetNoCopy<kArmsServiceId>(StringView(serviceId.data, serviceId.size));

                  auto workspace = sourceBuffer->CopyString(appConfig->mWorkspace);
                  data->mTags.SetNoCopy<kWorkspace>(StringView(workspace.data, workspace.size));

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
          4096,
          [](std::unique_ptr<AppSpanGroup>& base, const std::shared_ptr<CommonEvent>& other) {
              if (base == nullptr) {
                  return;
              }
              base->mRecords.push_back(other);
          },
          [](const std::shared_ptr<CommonEvent>&, std::shared_ptr<SourceBuffer>&) {
              return std::make_unique<AppSpanGroup>();
          }),
      mLogAggregator(
          4096,
          [](std::unique_ptr<AppLogGroup>& base, const std::shared_ptr<CommonEvent>& other) {
              if (base == nullptr) {
                  return;
              }
              base->mRecords.push_back(other);
          },
          [](const std::shared_ptr<CommonEvent>&, std::shared_ptr<SourceBuffer>&) {
              return std::make_unique<AppLogGroup>();
          }) {
}

std::array<size_t, 2>
NetworkObserverManager::generateAggKeyForNetMetric(ConnStatsRecord* record,
                                                   const std::shared_ptr<logtail::ebpf::AppDetail>& appInfo) {
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
    static constexpr auto kIdxes0 = {kHostNameIndex, kHostIpIndex};
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
    AttrHashCombine(result[0], appInfo->mAppHash); // combine app hash

    for (const auto& x : kIdxes1) {
        std::string_view attr(connTrackerAttrs[x].data(), connTrackerAttrs[x].size());
        AttrHashCombine(result[1], hasher(attr));
    }
    return result;
}

std::array<size_t, 2>
NetworkObserverManager::generateAggKeyForAppMetric(L7Record* record,
                                                   const std::shared_ptr<logtail::ebpf::AppDetail>& appInfo) {
    // auto* record = static_cast<AbstractAppRecord*>(abstractRecord.get());
    // calculate agg key
    std::array<size_t, 2> result{};
    result.fill(0UL);
    std::hash<std::string_view> hasher;
    auto connection = record->GetConnection();
    if (!connection) {
        LOG_WARNING(sLogger, ("connection is null", ""));
        return {};
    }

    static constexpr std::array<uint32_t, 4> kIdxes0 = {kHostNameIndex, kHostIpIndex};
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
    AttrHashCombine(result[0], appInfo->mAppHash);
    for (const auto x : kIdxes1) {
        std::string_view attr(ctAttrs[x].data(), ctAttrs[x].size());
        AttrHashCombine(result[1], hasher(attr));
    }
    std::string_view rpc(record->GetSpanName());
    AttrHashCombine(result[1], hasher(rpc));

    return result;
}

std::array<size_t, 1>
NetworkObserverManager::generateAggKeyForSpan(L7Record* record,
                                              const std::shared_ptr<logtail::ebpf::AppDetail>& appInfo) {
    // auto* record = static_cast<AbstractAppRecord*>(abstractRecord.get());
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
    static constexpr auto kIdxes = {kHostNameIndex, kHostIpIndex};
    for (const auto& x : kIdxes) {
        std::string_view attr(ctAttrs[x].data(), ctAttrs[x].size());
        AttrHashCombine(result[0], hasher(attr));
    }
    AttrHashCombine(result[0], appInfo->mAppHash);

    return result;
}

std::array<size_t, 1>
NetworkObserverManager::generateAggKeyForLog(L7Record* record,
                                             const std::shared_ptr<logtail::ebpf::AppDetail>& appInfo) {
    // auto* record = static_cast<AbstractAppRecord*>(abstractRecord.get());
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

bool NetworkObserverManager::ConsumeLogAggregateTree() { // handler
    if (!this->mInited || this->mSuspendFlag) {
        return false;
    }
#ifdef APSARA_UNIT_TEST_MAIN
    mExecTimes++;
#endif

    auto aggTree = mLogAggregator.GetAndReset();
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
        QueueKey queueKey = 0;
        uint32_t pluginIdx = -1;
        StringView configName;
        CounterPtr pushLogsTotal = nullptr;
        CounterPtr pushLogGroupTotal = nullptr;
        aggTree.ForEach(node, [&](const AppLogGroup* group) {
            // set process tag
            if (group->mRecords.empty()) {
                LOG_DEBUG(sLogger, ("", "no records .."));
                return;
            }
            std::array<StringView, kConnTrackerElementsTableSize> ctAttrVal;
            for (const auto& abstractRecord : group->mRecords) {
                auto* record = static_cast<L7Record*>(abstractRecord.get());
                if (!init) {
                    const auto& ct = record->GetConnection();
                    const auto& ctAttrs = ct->GetConnTrackerAttrs();
                    const auto& appInfo = getConnAppConfig(ct);
                    queueKey = appInfo->mQueueKey;
                    pluginIdx = appInfo->mPluginIndex;
                    configName = appInfo->mConfigName;
                    pushLogsTotal = appInfo->mPushLogsTotal;
                    pushLogGroupTotal = appInfo->mPushLogGroupTotal;
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
        if (needPush) {
            auto eventSize = eventGroup.GetEvents().size();
            ADD_COUNTER(pushLogsTotal, eventSize);
            ADD_COUNTER(pushLogGroupTotal, 1);
            mLogEventGroups.emplace_back(std::move(eventGroup));
        }

#else
        if (init && needPush) {
            pushEventsWithRetry(EventDataType::LOG,
                                std::move(eventGroup),
                                configName,
                                queueKey,
                                pluginIdx,
                                pushLogsTotal,
                                pushLogGroupTotal);
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

bool NetworkObserverManager::ConsumeNetMetricAggregateTree() { // handler
    if (!this->mInited || this->mSuspendFlag) {
        return false;
    }
#ifdef APSARA_UNIT_TEST_MAIN
    mExecTimes++;
#endif

    auto aggTree = mNetAggregator.GetAndReset();

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
        std::shared_ptr<SourceBuffer>& sourceBuffer = node->mSourceBuffer;
        PipelineEventGroup eventGroup(sourceBuffer); // per node represent an APP ...
        eventGroup.SetTagNoCopy(kAppType.MetricKey(), kAPMValue);
        eventGroup.SetTagNoCopy(kTagTechnology, kEBPFValue);
        eventGroup.SetTagNoCopy(kDataType.MetricKey(), kMetricValue);
        eventGroup.SetTag(kTagClusterIdKey, mClusterId);

        bool init = false;
        QueueKey queueKey = 0;
        uint32_t pluginIdx = -1;
        StringView configName;
        CounterPtr pushMetricsTotal = nullptr;
        CounterPtr pushMetricGroupTotal = nullptr;
        aggTree.ForEach(node, [&](const NetMetricData* group) {
            LOG_DEBUG(sLogger,
                      ("dump group attrs", group->ToString())("ct attrs", group->mConnection->DumpConnection()));
            if (group == nullptr || group->mConnection == nullptr) {
                return;
            }
            if (!init) {
                const auto& appInfo = getConnAppConfig(group->mConnection); // running in timer thread, need thread safe
                if (appInfo == nullptr || appInfo->mAppId.empty()) {
                    return;
                }
                queueKey = appInfo->mQueueKey;
                pluginIdx = appInfo->mPluginIndex;
                configName = appInfo->mConfigName;
                pushMetricsTotal = appInfo->mPushMetricsTotal;
                pushMetricGroupTotal = appInfo->mPushMetricGroupTotal;
                COPY_AND_SET_TAG(eventGroup, sourceBuffer, kAppId.MetricKey(), appInfo->mAppId);
                COPY_AND_SET_TAG(eventGroup, sourceBuffer, kAppName.MetricKey(), appInfo->mAppName);
                COPY_AND_SET_TAG(eventGroup, sourceBuffer, kWorkspace.MetricKey(), appInfo->mWorkspace);
                COPY_AND_SET_TAG(eventGroup, sourceBuffer, kArmsServiceId.MetricKey(), appInfo->mServiceId);
                eventGroup.SetTagNoCopy(kIp.MetricKey(), group->mTags.Get<kIp>()); // pod ip
                eventGroup.SetTagNoCopy(kHostName.MetricKey(), group->mTags.Get<kHostName>()); // pod name
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
        ADD_COUNTER(pushMetricsTotal, eventGroup.GetEvents().size());
        ADD_COUNTER(pushMetricGroupTotal, 1);
        mMetricEventGroups.emplace_back(std::move(eventGroup));
#else
        pushEventsWithRetry(EventDataType::NET_METRIC,
                            std::move(eventGroup),
                            configName,
                            queueKey,
                            pluginIdx,
                            pushMetricsTotal,
                            pushMetricGroupTotal);
#endif
    }
    return true;
}

bool NetworkObserverManager::ConsumeMetricAggregateTree() { // handler
    if (!this->mInited || this->mSuspendFlag) {
        return false;
    }
#ifdef APSARA_UNIT_TEST_MAIN
    mExecTimes++;
#endif

    LOG_DEBUG(sLogger, ("enter aggregator ...", mAppAggregator.NodeCount()));

    auto aggTree = this->mAppAggregator.GetAndReset();

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
        std::shared_ptr<SourceBuffer>& sourceBuffer = node->mSourceBuffer;
        PipelineEventGroup eventGroup(sourceBuffer); // per node represent an APP ...
        eventGroup.SetTagNoCopy(kAppType.MetricKey(), kAPMValue);
        eventGroup.SetTagNoCopy(kTagTechnology, kEBPFValue);
        eventGroup.SetTagNoCopy(kDataType.MetricKey(), kMetricValue);

        bool init = false;
        QueueKey queueKey = 0;
        uint32_t pluginIdx = -1;
        StringView configName;
        CounterPtr pushMetricsTotal = nullptr;
        CounterPtr pushMetricGroupTotal = nullptr;
        aggTree.ForEach(node, [&](const AppMetricData* group) {
            LOG_DEBUG(sLogger,
                      ("dump group attrs", group->ToString())("ct attrs", group->mConnection->DumpConnection()));
            // instance dim
            if (group->mConnection == nullptr) {
                return;
            }

            if (!init) {
                const auto& appInfo = getConnAppConfig(group->mConnection); // running in timer thread, need thread safe
                if (appInfo == nullptr || appInfo->mAppId.empty()) {
                    return;
                }
                queueKey = appInfo->mQueueKey;
                pluginIdx = appInfo->mPluginIndex;
                configName = appInfo->mConfigName;
                pushMetricsTotal = appInfo->mPushMetricsTotal;
                pushMetricGroupTotal = appInfo->mPushMetricGroupTotal;

                // set common attrs ...
                COPY_AND_SET_TAG(eventGroup, sourceBuffer, kAppId.MetricKey(), appInfo->mAppId);
                COPY_AND_SET_TAG(eventGroup, sourceBuffer, kAppName.MetricKey(), appInfo->mAppName);
                COPY_AND_SET_TAG(eventGroup, sourceBuffer, kWorkspace.MetricKey(), appInfo->mWorkspace);
                COPY_AND_SET_TAG(eventGroup, sourceBuffer, kArmsServiceId.MetricKey(), appInfo->mServiceId);
                eventGroup.SetTagNoCopy(kIp.MetricKey(), group->mTags.Get<kIp>()); // pod ip
                eventGroup.SetTagNoCopy(kHostName.MetricKey(), group->mTags.Get<kHostName>()); // pod ip

                auto* tagMetric = eventGroup.AddMetricEvent();
                tagMetric->SetName(kMetricNameTag);
                tagMetric->SetValue(UntypedSingleValue{1.0});
                tagMetric->SetTimestamp(seconds, 0);
                tagMetric->SetTagNoCopy(kTagAgentVersionKey, kTagV1Value);
                tagMetric->SetTag(kTagAppKey, appInfo->mAppName); // app ===> appname
                tagMetric->SetTag(kTagResourceIdKey, appInfo->mAppId); // resourceid -==> pid
                tagMetric->SetTag(kTagCmsWorkspaceKey, appInfo->mWorkspace); // workspace ===>
                tagMetric->SetTag(kTagArmsServiceIdKey, appInfo->mServiceId); // serviceId ===>
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
        if (init) {
            ADD_COUNTER(pushMetricsTotal, eventGroup.GetEvents().size());
            ADD_COUNTER(pushMetricGroupTotal, 1);
            mMetricEventGroups.emplace_back(std::move(eventGroup));
        }
#else
        if (init) {
            pushEventsWithRetry(EventDataType::APP_METRIC,
                                std::move(eventGroup),
                                configName,
                                queueKey,
                                pluginIdx,
                                pushMetricsTotal,
                                pushMetricGroupTotal);
        } else {
            LOG_DEBUG(sLogger, ("appid is empty, no need to push", ""));
        }
#endif
    }
    return true;
}

static constexpr StringView kSpanHostAttrKey = "host";

bool NetworkObserverManager::ConsumeSpanAggregateTree() { // handler
    if (!this->mInited || this->mSuspendFlag) {
        return false;
    }
#ifdef APSARA_UNIT_TEST_MAIN
    mExecTimes++;
#endif

    auto aggTree = mSpanAggregator.GetAndReset();

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
        QueueKey queueKey = 0;
        uint32_t pluginIdx = -1;
        StringView configName;
        CounterPtr pushSpansTotal = nullptr;
        CounterPtr pushSpanGroupTotal = nullptr;
        aggTree.ForEach(node, [&](const AppSpanGroup* group) {
            // set process tag
            if (group->mRecords.empty()) {
                LOG_DEBUG(sLogger, ("", "no records .."));
                return;
            }
            for (const auto& abstractRecord : group->mRecords) {
                auto* record = static_cast<L7Record*>(abstractRecord.get());
                if (record == nullptr || record->GetConnection() == nullptr) {
                    continue;
                }
                const auto& ct = record->GetConnection();
                const auto& ctAttrs = ct->GetConnTrackerAttrs();

                if (!init) {
                    const auto& appInfo = getConnAppConfig(ct); // thread safe, can be used in timer thread ...
                    if (appInfo == nullptr || appInfo->mAppId.empty()) {
                        return;
                    }
                    queueKey = appInfo->mQueueKey;
                    pluginIdx = appInfo->mPluginIndex;
                    configName = appInfo->mConfigName;
                    pushSpansTotal = appInfo->mPushSpansTotal;
                    pushSpanGroupTotal = appInfo->mPushSpanGroupTotal;
                    COPY_AND_SET_TAG(eventGroup, sourceBuffer, kAppId.SpanKey(), appInfo->mAppId);
                    COPY_AND_SET_TAG(eventGroup, sourceBuffer, kAppName.SpanKey(), appInfo->mAppName);
                    COPY_AND_SET_TAG(eventGroup, sourceBuffer, kWorkspace.SpanKey(), appInfo->mWorkspace);
                    COPY_AND_SET_TAG(eventGroup, sourceBuffer, kArmsServiceId.SpanKey(), appInfo->mServiceId);

                    COPY_AND_SET_TAG(eventGroup, sourceBuffer, kHostIp.SpanKey(), ctAttrs.Get<kIp>()); // pod ip
                    COPY_AND_SET_TAG(
                        eventGroup, sourceBuffer, kHostName.SpanKey(), ctAttrs.Get<kPodName>()); // pod name
                    eventGroup.SetTagNoCopy(kAppType.SpanKey(), kAPMValue);
                    eventGroup.SetTagNoCopy(kTagTechnology, kEBPFValue);
                    eventGroup.SetTagNoCopy(kDataType.SpanKey(), kTraceValue);
                    for (auto tag = eventGroup.GetTags().begin(); tag != eventGroup.GetTags().end(); tag++) {
                        LOG_DEBUG(sLogger, ("record span tags", "")(std::string(tag->first), std::string(tag->second)));
                    }
                    init = true;
                }
                auto* spanEvent = eventGroup.AddSpanEvent();
                COPY_AND_SET_TAG(eventGroup, sourceBuffer, kSpanTagKeyApp, ctAttrs.Get<kWorkloadName>());

                // attr.host, adjust to old logic ...
                auto host = sourceBuffer->CopyString(ctAttrs.Get<kIp>());
                spanEvent->SetTag(kSpanHostAttrKey, StringView(host.data, host.size));

                for (size_t i = 0; i < kConnTrackerElementsTableSize; i++) {
                    auto sb = sourceBuffer->CopyString(ctAttrs[i]);
                    spanEvent->SetTagNoCopy(kConnTrackerTable.ColSpanKey(i), StringView(sb.data, sb.size));
                    LOG_DEBUG(sLogger, ("record span tags", "")(std::string(kConnTrackerTable.ColSpanKey(i)), sb.data));
                }

                spanEvent->SetTraceId(TraceIDToString(record->GetTraceId()));
                spanEvent->SetSpanId(SpanIDToString(record->GetSpanId()));
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
                auto* httpRecord = static_cast<HttpRecord*>(record);
                spanEvent->SetTag(kHTTPReqBody.SpanKey(), httpRecord->GetReqBody());
                spanEvent->SetTag(kHTTPRespBody.SpanKey(), httpRecord->GetRespBody());
                spanEvent->SetTag(kHTTPReqBodySize.SpanKey(), std::to_string(httpRecord->GetReqBodySize()));
                spanEvent->SetTag(kHTTPRespBodySize.SpanKey(), std::to_string(httpRecord->GetRespBodySize()));
                spanEvent->SetTag(kHTTPVersion.SpanKey(), httpRecord->GetProtocolVersion());

                // spanEvent->SetTag(kHTTPReqHeader.SpanKey(), httpRecord->GetReqHeaderMap());
                // spanEvent->SetTag(kHTTPRespHeader.SpanKey(), httpRecord->GetRespHeaders());

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
        if (needPush) {
            ADD_COUNTER(pushSpansTotal, eventGroup.GetEvents().size());
            ADD_COUNTER(pushSpanGroupTotal, 1);
            mSpanEventGroups.emplace_back(std::move(eventGroup));
        }

#else
        if (init && needPush) {
            pushEventsWithRetry(EventDataType::APP_SPAN,
                                std::move(eventGroup),
                                configName,
                                queueKey,
                                pluginIdx,
                                pushSpansTotal,
                                pushSpanGroupTotal);
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

int NetworkObserverManager::Init() {
    if (mInited) {
        return 0;
    }

    static std::string sDelimComma = ",";
    auto protocols = StringSpliter(STRING_FLAG(ebpf_networkobserver_enable_protocols), sDelimComma);
    updateParsers(protocols, {});
    // updateParsers(opt->mEnableProtocols, {});

    mInited = true;

    mConnectionManager = ConnectionManager::Create();
    mConnectionManager->UpdateMaxConnectionThreshold(INT32_FLAG(ebpf_networkobserver_max_connections));

    mCidOffset = GuessContainerIdOffset();

    const char* value = getenv("_cluster_id_");
    if (value != nullptr) {
        mClusterId = value;
    }

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

    return 0;
}

std::shared_ptr<AppDetail> NetworkObserverManager::getWorkloadAppConfig(const std::string& ns,
                                                                        const std::string& workloadKind,
                                                                        const std::string& workloadName) {
    size_t key = GenerateWorkloadKey(ns, workloadKind, workloadName);
    return getWorkloadAppConfig(key);
}

std::shared_ptr<AppDetail> NetworkObserverManager::getWorkloadAppConfig(size_t workloadKey) {
    const auto& it = mWorkloadConfigs.find(workloadKey);
    if (it != mWorkloadConfigs.end()) {
        return it->second.config;
    }
    return nullptr;
}

std::shared_ptr<AppDetail> NetworkObserverManager::getContainerAppConfig(size_t key) {
    return GetAppDetail(mContainerConfigs, key);
}

std::shared_ptr<AppDetail> NetworkObserverManager::getConnAppConfig(const std::shared_ptr<Connection>& conn) {
    ReadLock lk(mAppConfigLock);
    return getContainerAppConfig(conn->GetContainerIdKey());
}

std::shared_ptr<AppDetail> NetworkObserverManager::getAppConfigFromReplica(const std::shared_ptr<Connection>& conn) {
    if (!conn) {
        return nullptr;
    }
    return GetAppDetail(mContainerConfigsReplica, conn->GetContainerIdKey());
}

int NetworkObserverManager::AddOrUpdateConfig(const CollectionPipelineContext* ctx,
                                              uint32_t index,
                                              const PluginMetricManagerPtr& metricMgr,
                                              const std::variant<SecurityOptions*, ObserverNetworkOption*>& opt) {
    if (!ctx) {
        LOG_ERROR(sLogger, ("ctx is null", ""));
        return 1;
    }

    auto* option = std::get<ObserverNetworkOption*>(opt);
    if (!option) {
        LOG_WARNING(sLogger, ("option is null, configName", ctx->GetConfigName()));
        return 1;
    }

    auto newConfig = std::make_shared<AppDetail>(option, metricMgr);
    newConfig->mQueueKey = ctx->GetProcessQueueKey();
    newConfig->mPluginIndex = index;
    newConfig->mConfigName = ctx->GetConfigName();

    WriteLock lk(mAppConfigLock);
    std::vector<std::string> expiredCids;
    const std::string& configName = ctx->GetConfigName();

    if (option->mSelectors.empty()) {
        // clean old config
        if (auto it = mConfigToWorkloads.find(configName); it != mConfigToWorkloads.end()) {
            for (auto key : it->second) {
                if (auto wIt = mWorkloadConfigs.find(key); wIt != mWorkloadConfigs.end()) {
                    expiredCids.insert(
                        expiredCids.end(), wIt->second.containerIds.begin(), wIt->second.containerIds.end());
                    mWorkloadConfigs.erase(wIt);
                }
            }
            mConfigToWorkloads.erase(it);
        }

        // setup new config
        WorkloadConfig defaultConfig;
        defaultConfig.config = newConfig;
        mWorkloadConfigs[kGlobalWorkloadKey] = defaultConfig;
        mConfigToWorkloads[configName] = {kGlobalWorkloadKey};
        mContainerConfigs.insert({kGlobalWorkloadKey, newConfig});

        updateConfigVersionAndWhitelist({}, std::move(expiredCids));
        return 0;
    }

    std::set<size_t> currentWorkloadKeys;
    // collect current workload keys
    for (const auto& selector : option->mSelectors) {
        auto key = GenerateWorkloadKey(selector.mNamespace, selector.mWorkloadKind, selector.mWorkloadName);
        currentWorkloadKeys.insert(key);
    }

    // add or update config => workloads
    auto& workloadKeys = mConfigToWorkloads[configName];
    std::set<size_t> expiredWorkloadKeys;

    // search expired workloads
    std::set_difference(workloadKeys.begin(),
                        workloadKeys.end(),
                        currentWorkloadKeys.begin(),
                        currentWorkloadKeys.end(),
                        std::inserter(expiredWorkloadKeys, expiredWorkloadKeys.begin()));

    // clean expired workloads
    for (auto key : expiredWorkloadKeys) {
        if (auto it = mWorkloadConfigs.find(key); it != mWorkloadConfigs.end()) {
            for (const auto& cid : it->second.containerIds) {
                expiredCids.push_back(cid);
                size_t cidKey = GenerateContainerKey(cid);
                mContainerConfigs.erase(cidKey);
            }
            mWorkloadConfigs.erase(it);
        }
        workloadKeys.erase(key);
    }

    // add or update workload configs
    for (auto key : currentWorkloadKeys) {
        bool configChanged = false;
        if (auto it = mWorkloadConfigs.find(key); it != mWorkloadConfigs.end()) {
            if (!(*(it->second.config) == *newConfig)) {
                it->second.config = newConfig;
                configChanged = true;
            }
        } else {
            WorkloadConfig wc;
            wc.config = newConfig;
            mWorkloadConfigs[key] = wc;
            configChanged = true;
            workloadKeys.insert(key);
        }

        // update container configs
        if (configChanged) {
            updateContainerConfigs(key, newConfig);
        }
    }

    updateConfigVersionAndWhitelist({}, std::move(expiredCids));
    Resume(opt);
    return 0;
}

int NetworkObserverManager::RemoveConfig(const std::string& configName) {
    WriteLock lk(mAppConfigLock);

    auto configIt = mConfigToWorkloads.find(configName);
    if (configIt == mConfigToWorkloads.end()) {
        LOG_DEBUG(sLogger, ("No workloads for config", configName));
        return 0;
    }

    if (configIt->second.empty() || (configIt->second.size() == 1 && configIt->second.count(kGlobalWorkloadKey))) {
        mContainerConfigs.erase(kGlobalWorkloadKey);
    }

    std::vector<std::string> expiredCids;
    // clear related workloads
    for (auto key : configIt->second) {
        if (auto wIt = mWorkloadConfigs.find(key); wIt != mWorkloadConfigs.end()) {
            for (const auto& cid : wIt->second.containerIds) {
                expiredCids.push_back(cid);
                // clean up container configs ...
                mContainerConfigs.erase(GenerateContainerKey(cid));
            }
            mWorkloadConfigs.erase(wIt);
        }
    }

    // delete config
    mConfigToWorkloads.erase(configIt);

    updateConfigVersionAndWhitelist({}, std::move(expiredCids));

    LOG_INFO(sLogger,
             ("Removed config", configName)("remaining workloads", mWorkloadConfigs.size())("container configs",
                                                                                            mContainerConfigs.size()));

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

// called by curl thread, async update configs for container ...
void NetworkObserverManager::HandleHostMetadataUpdate(const std::vector<std::string>& podCidVec) {
    std::vector<std::pair<std::string, uint64_t>> newContainerIds;
    std::map<size_t, std::shared_ptr<AppDetail>> newCidConfigs;
    std::vector<std::string> expiredContainerIds;

    WriteLock lk(mAppConfigLock);

    std::unordered_map<size_t, std::set<std::string>> currentWorkloadCids;
    for (const auto& cid : podCidVec) {
        auto podInfo = K8sMetadata::GetInstance().GetInfoByContainerIdFromCache(cid);
        if (!podInfo) {
            continue;
        }

        size_t workloadKey = GenerateWorkloadKey(podInfo->mNamespace, podInfo->mWorkloadKind, podInfo->mWorkloadName);
        auto wIt = mWorkloadConfigs.find(workloadKey);
        if (wIt == mWorkloadConfigs.end() || !wIt->second.config || wIt->second.config->mAppName.empty()) {
            continue;
        }

        currentWorkloadCids[workloadKey].insert(cid);

        // check if is new container
        size_t cidKey = GenerateContainerKey(cid);
        if (!wIt->second.containerIds.count(cid)) {
            newContainerIds.emplace_back(cid, static_cast<uint64_t>(cidKey));
        }
        newCidConfigs[cidKey] = wIt->second.config;
        // mContainerConfigs[cidKey] = wIt->second.config;

        LOG_DEBUG(sLogger,
                  ("appId", wIt->second.config->mAppId)("appName", wIt->second.config->mAppName)(
                      "podIp", podInfo->mPodIp)("podName", podInfo->mPodName)("containerId", cid));
    }

    // check expiration and update workload config
    for (auto& [workloadKey, wConfig] : mWorkloadConfigs) {
        std::set<std::string> expiredInWorkload;

        // find expired container
        for (const auto& cid : wConfig.containerIds) {
            if (!currentWorkloadCids[workloadKey].count(cid)) {
                expiredContainerIds.push_back(cid);
                expiredInWorkload.insert(cid);
            }
        }

        // erase expired container
        for (const auto& cid : expiredInWorkload) {
            wConfig.containerIds.erase(cid);
            size_t cidKey = GenerateContainerKey(cid);
            mContainerConfigs.erase(cidKey);
        }

        // add new container
        if (auto it = currentWorkloadCids.find(workloadKey); it != currentWorkloadCids.end()) {
            for (const auto& cid : it->second) {
                wConfig.containerIds.insert(cid);
            }
        }
    }

    for (const auto& it : newCidConfigs) {
        mContainerConfigs[it.first] = it.second;
    }

    updateConfigVersionAndWhitelist(std::move(newContainerIds), std::move(expiredContainerIds));
}

void NetworkObserverManager::processRecordAsLog(const std::shared_ptr<CommonEvent>& record,
                                                const std::shared_ptr<logtail::ebpf::AppDetail>& appInfo) {
    auto* l7Record = static_cast<L7Record*>(record.get());
    auto res = mLogAggregator.Aggregate(record, generateAggKeyForLog(l7Record, appInfo));
    LOG_DEBUG(sLogger, ("agg res", res)("node count", mLogAggregator.NodeCount()));
}

void NetworkObserverManager::processRecordAsSpan(const std::shared_ptr<CommonEvent>& record,
                                                 const std::shared_ptr<logtail::ebpf::AppDetail>& appInfo) {
    auto* l7Record = static_cast<L7Record*>(record.get());
    auto res = mSpanAggregator.Aggregate(record, generateAggKeyForSpan(l7Record, appInfo));
    LOG_DEBUG(sLogger, ("agg res", res)("node count", mSpanAggregator.NodeCount()));
}

void NetworkObserverManager::processRecordAsMetric(L7Record* record,
                                                   const std::shared_ptr<logtail::ebpf::AppDetail>& appInfo) {
    auto res = mAppAggregator.Aggregate(record, generateAggKeyForAppMetric(record, appInfo));
    LOG_DEBUG(sLogger, ("agg res", res)("node count", mAppAggregator.NodeCount()));
}

int NetworkObserverManager::PollPerfBuffer(int timeout) {
    auto nowMs = TimeKeeper::GetInstance()->NowMs();
    // 1. listen host pod info // every 5 seconds
    if (K8sMetadata::GetInstance().Enable() && nowMs - mLastUpdateHostMetaTimeMs >= 5000) { // 5s
        // TODO (qianlu.kk) instead of using cri interface ...
        UploadHostMetadataUpdateTask();
        mLastUpdateHostMetaTimeMs = nowMs;
    }

    // 2. do perf callback ==> update cache, generate record(if not ready, add to mRetryableEventCache, else add to
    // aggregator) poll stats -> ctrl -> info
    if (mLastConfigVersion != mConfigVersion) {
        ReadLock lk(mAppConfigLock);
        mContainerConfigsReplica = mContainerConfigs;
        mLastConfigVersion.store(mConfigVersion);
    }

    int32_t flag = 0;
    int ret = mEBPFAdapter->PollPerfBuffers(
        PluginType::NETWORK_OBSERVE, kNetObserverMaxBatchConsumeSize, &flag, timeout); // 0 means non-blocking
    if (ret < 0) {
        LOG_WARNING(sLogger, ("poll event err, ret", ret));
    }

    // 3. connection cache gc
    // Iterations() mainly do gc and do not generate conn stats records ...
    // map in map, outter key is epoc, inner key is id?
    mConnectionManager->Iterations();
    SET_GAUGE(mConnectionNum, mConnectionManager->ConnectionTotal());

    LOG_DEBUG(
        sLogger,
        ("===== statistic =====>> total data events:",
         mRecvHttpDataEventsTotal.load())(" total conn stats events:", mRecvConnStatEventsTotal.load())(
            " total ctrl events:", mRecvCtrlEventsTotal.load())(" lost data events:", mLostDataEventsTotal.load())(
            " lost stats events:", mLostConnStatEventsTotal.load())(" lost ctrl events:", mLostCtrlEventsTotal.load()));
    // 4. consume mRetryableEventCache, used for handling metadata attach failed scenario ...
    mRetryableEventCache.HandleEvents();
    return ret;
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

    // AcceptDataEvent is called in poller thread, PollPerfBuffer will copy app config before do callback ...
    const auto& appDetail = getAppConfigFromReplica(conn);
    if (appDetail == nullptr) {
        LOG_DEBUG(sLogger,
                  ("failed to find app detail for conn", conn->DumpConnection())("cidKey", conn->GetContainerIdKey()));
        return;
    }

    std::vector<std::shared_ptr<L7Record>> records
        = ProtocolParserManager::GetInstance().Parse(protocol, conn, event, appDetail);

    if (records.empty()) {
        return;
    }

    // add records to span/event generate queue
    for (const auto& record : records) {
        std::shared_ptr<RetryableEvent> retryableEvent
            = std::make_shared<HttpRetryableEvent>(5, record, mCommonEventQueue);
        if (!retryableEvent->HandleMessage()) {
            // LOG_DEBUG(sLogger, ("failed once", "enqueue retry cache")("meta flag", conn->GetMetaFlags()));
            mRetryableEventCache.AddEvent(retryableEvent);
        }
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

int NetworkObserverManager::SendEvents() {
    auto nowMs = TimeKeeper::GetInstance()->NowMs();
    // consume log agg tree -- 2000ms
    if (nowMs - mLastSendLogTimeMs >= mSendLogIntervalMs) {
        LOG_DEBUG(sLogger, ("begin consume log agg tree", "log"));
        ConsumeLogAggregateTree();
        mLastSendLogTimeMs = nowMs;
    }

    // consume span agg tree -- 2000ms
    if (nowMs - mLastSendSpanTimeMs >= mSendSpanIntervalMs) {
        LOG_DEBUG(sLogger, ("begin consume span agg tree", "span"));
        ConsumeSpanAggregateTree();
        mLastSendSpanTimeMs = nowMs;
    }

    // consume metric agg trees -- 15000ms
    if (nowMs - mLastSendMetricTimeMs >= mSendMetricIntervalMs) {
        LOG_DEBUG(sLogger, ("begin consume metric agg tree", "metric"));
        ConsumeMetricAggregateTree();
        mLastSendMetricTimeMs = nowMs;
    }

    if (nowMs - mLastSendAgentInfoTimeMs >= mSendAgentInfoIntervalMs) {
        LOG_DEBUG(sLogger, ("begin report agent info", "agentinfo"));
        ReportAgentInfo();
        mLastSendAgentInfoTimeMs = nowMs;
    }

    return 0;
}

const static std::string kAgentInfoAppIdKey = "pid";
const static std::string kAgentInfoIpKey = "ip";
const static std::string kAgentInfoHostnameKey = "hostname";
const static std::string kAgentInfoAppnameKey = "appName";
const static std::string kAgentInfoAgentVersionKey = "agentVersion";
const static std::string kAgentInfoStartTsKey = "startTimestamp";

void NetworkObserverManager::pushEventsWithRetry(EventDataType dataType,
                                                 PipelineEventGroup&& eventGroup,
                                                 const StringView& configName,
                                                 QueueKey queueKey,
                                                 uint32_t pluginIdx,
                                                 CounterPtr& eventCounter,
                                                 CounterPtr& eventGroupCounter,
                                                 size_t retryTimes) {
    size_t eventsSize = eventGroup.GetEvents().size();
    if (eventsSize > 0) {
        // push
        ADD_COUNTER(eventCounter, eventsSize);
        ADD_COUNTER(eventGroupCounter, 1);
        LOG_DEBUG(sLogger, ("agentinfo group size", eventsSize));
        std::unique_ptr<ProcessQueueItem> item = std::make_unique<ProcessQueueItem>(std::move(eventGroup), pluginIdx);
        for (size_t times = 0; times < retryTimes; times++) {
            auto result = ProcessQueueManager::GetInstance()->PushQueue(queueKey, std::move(item));
            if (QueueStatus::OK != result) {
                LOG_WARNING(
                    sLogger,
                    ("configName", configName)("pluginIdx", pluginIdx)("dataType", magic_enum::enum_name(dataType))(
                        "[NetworkObserver] push to queue failed!", magic_enum::enum_name(result)));
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } else {
                LOG_DEBUG(sLogger,
                          ("NetworkObserver push events successful, eventSize:",
                           eventsSize)("dataType", magic_enum::enum_name(dataType)));
                break;
            }
        }
    }
}

void NetworkObserverManager::ReportAgentInfo() {
    int cnt = 0;
    const time_t now = time(nullptr);
    for (const auto& configToWorkload : mConfigToWorkloads) {
        const auto& workloadKeys = configToWorkload.second;
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        for (const auto& workloadKey : workloadKeys) {
            const auto& it = mWorkloadConfigs.find(kGlobalWorkloadKey);
            if (it == mWorkloadConfigs.end()) {
                LOG_DEBUG(sLogger, ("[AgentInfo] failed to find workloadKey from mWorkloadConfigs", workloadKey));
                continue;
            }
            auto& workloadConfig = it->second;
            auto& appConfig = workloadConfig.config;
            if (appConfig == nullptr) {
                LOG_DEBUG(sLogger,
                          ("[AgentInfo] failed to find app config for workloadKey from mWorkloadConfigs", workloadKey));
                continue;
            }
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.SetTagNoCopy(kDataType.LogKey(), kAgentInfoValue);
            if (workloadKey == kGlobalWorkloadKey) {
                // instance level ...
                auto* event = eventGroup.AddLogEvent();
                event->SetContent(kAgentInfoAppIdKey, appConfig->mAppId);
                event->SetContent(kAgentInfoAppnameKey, appConfig->mAppName);
                event->SetContent(kAgentInfoAgentVersionKey, ILOGTAIL_VERSION);
                if (Connection::gSelfPodIp.empty()) {
                    event->SetContent(kAgentInfoIpKey, GetHostIp());
                } else {
                    event->SetContentNoCopy(kAgentInfoIpKey, Connection::gSelfPodIp);
                }

                if (Connection::gSelfPodName.empty()) {
                    event->SetContent(kAgentInfoHostnameKey, GetHostName());
                } else {
                    event->SetContentNoCopy(kAgentInfoHostnameKey, Connection::gSelfPodName);
                }
                event->SetTimestamp(now, 0);
                cnt++;
            } else {
                if (!K8sMetadata::GetInstance().Enable()) {
                    continue;
                }

                for (const auto& containerId : workloadConfig.containerIds) {
                    // generate for k8s ---- POD Level
                    auto podMeta = K8sMetadata::GetInstance().GetInfoByContainerIdFromCache(containerId);
                    if (podMeta == nullptr) {
                        LOG_DEBUG(sLogger, ("[AgentInfo] failed to fetch containerId", containerId));
                        continue;
                    }

                    auto* event = eventGroup.AddLogEvent();
                    event->SetContent(kAgentInfoAppIdKey, appConfig->mAppId);
                    event->SetContent(kAgentInfoIpKey, podMeta->mPodIp);
                    event->SetContent(kAgentInfoHostnameKey, podMeta->mPodName);
                    event->SetContent(kAgentInfoAppnameKey, appConfig->mAppName);
                    event->SetContent(kAgentInfoAgentVersionKey, ILOGTAIL_VERSION);
                    event->SetContent(kAgentInfoStartTsKey, ToString(podMeta->mTimestamp));
                    event->SetTimestamp(now, 0);
                    cnt++;
                }
            }

            pushEventsWithRetry(EventDataType::AGENT_INFO,
                                std::move(eventGroup),
                                appConfig->mConfigName,
                                appConfig->mQueueKey,
                                appConfig->mPluginIndex,
                                appConfig->mPushLogsTotal,
                                appConfig->mPushLogGroupTotal);
        }
    }

    LOG_DEBUG(sLogger, ("[AgentInfo] ReportAgentInfo count:", cnt));
}

int NetworkObserverManager::HandleEvent([[maybe_unused]] const std::shared_ptr<CommonEvent>& commonEvent) {
    auto* httpRecord = static_cast<HttpRecord*>(commonEvent.get());
    if (httpRecord) {
        auto appDetail = httpRecord->GetAppDetail();
        if (appDetail->mEnableLog && httpRecord->ShouldSample()) {
            processRecordAsLog(commonEvent, appDetail);
        }
        if (appDetail->mEnableSpan && httpRecord->ShouldSample()) {
            processRecordAsSpan(commonEvent, appDetail);
        }
        if (appDetail->mEnableMetric) {
            processRecordAsMetric(httpRecord, appDetail);
        }
    }
    return 0;
}

int NetworkObserverManager::Destroy() {
    if (!mInited) {
        return 0;
    }
    LOG_INFO(sLogger, ("prepare to destroy", ""));
    mEBPFAdapter->StopPlugin(PluginType::NETWORK_OBSERVE);
    LOG_INFO(sLogger, ("destroy stage", "shutdown ebpf prog"));
    this->mInited = false;

#ifdef APSARA_UNIT_TEST_MAIN
    return 0;
#endif
    LOG_INFO(sLogger, ("destroy stage", "destroy connection manager"));
    mConnectionManager.reset(nullptr);
    LOG_INFO(sLogger, ("destroy stage", "destroy sampler"));

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
    mAppAggregator.Reset();
    mNetAggregator.Reset();
    mSpanAggregator.Reset();
    mLogAggregator.Reset();

    LOG_INFO(sLogger, ("destroy stage", "release consumer thread"));
    return 0;
}

void NetworkObserverManager::UpdateWhitelists(std::vector<std::pair<std::string, uint64_t>>&& enableCids,
                                              std::vector<std::string>&& disableCids) {
#ifdef APSARA_UNIT_TEST_MAIN
    mEnableCids = enableCids;
    mDisableCids = disableCids;
    return;
#endif
    for (auto& cid : enableCids) {
        LOG_INFO(sLogger, ("UpdateWhitelists cid", cid.first)("key", cid.second));
        mEBPFAdapter->SetNetworkObserverCidFilter(cid.first, true, cid.second);
    }

    for (auto& cid : disableCids) {
        LOG_INFO(sLogger, ("UpdateBlacklists cid", cid));
        mEBPFAdapter->SetNetworkObserverCidFilter(cid, false, 0);
    }
}

} // namespace logtail::ebpf

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
#include "common/StringTools.h"
#include "common/magic_enum.hpp"
#include "ebpf/Config.h"
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

NetworkObserverManager::NetworkObserverManager(std::shared_ptr<ProcessCacheManager>& baseMgr,
                                               std::shared_ptr<SourceManager> sourceManager,
                                               moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                                               std::shared_ptr<Timer> scheduler)
    : AbstractManager(baseMgr, sourceManager, queue, scheduler),
      mAppAggregator(
          4096,
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
              base->mSum += double(other->GetLatencyMs() / 1000);
          },
          [this](const std::shared_ptr<AbstractRecord>& i) -> std::unique_ptr<AppMetricData> {
              auto* in = static_cast<AbstractAppRecord*>(i.get());
              auto data = std::make_unique<AppMetricData>(in->GetConnection(), in->GetSpanName());
              auto connection = in->GetConnection();
              if (!connection) {
                  LOG_WARNING(sLogger, ("connection is null", ""));
                  return nullptr;
              }
              auto ctAttrs = connection->GetConnTrackerAttrs();
              data->mAppId = ctAttrs[kConnTrackerTable.ColIndex(kAppId.Name())];
              data->mAppName = ctAttrs[kConnTrackerTable.ColIndex(kAppName.Name())];
              data->mHost = ctAttrs[kConnTrackerTable.ColIndex(kHostName.Name())];
              data->mIp = ctAttrs[kConnTrackerTable.ColIndex(kPodIp.Name())];

              data->mWorkloadKind = ctAttrs[kConnTrackerTable.ColIndex(kWorkloadKind.Name())];
              data->mWorkloadName = ctAttrs[kConnTrackerTable.ColIndex(kWorkloadName.Name())];

              data->mRpcType = ctAttrs[kConnTrackerTable.ColIndex(kRpcType.Name())];
              data->mCallType = ctAttrs[kConnTrackerTable.ColIndex(kCallType.Name())];
              data->mCallKind = ctAttrs[kConnTrackerTable.ColIndex(kCallKind.Name())];

              data->mDestId = ctAttrs[kConnTrackerTable.ColIndex(kDestId.Name())];
              data->mEndpoint = ctAttrs[kConnTrackerTable.ColIndex(kEndpoint.Name())];
              data->mNamespace = ctAttrs[kConnTrackerTable.ColIndex(kNamespace.Name())];

              return data;
          }),
      mNetAggregator(
          4096,
          [this](std::unique_ptr<NetMetricData>& base, const std::shared_ptr<AbstractRecord>& o) {
              auto* other = static_cast<ConnStatsRecord*>(o.get());
              base->mDropCount += other->mDropCount;
              base->mRetransCount += other->mRetransCount;
              base->mRecvBytes += other->mRecvBytes;
              base->mSendBytes += other->mSendBytes;
              base->mRecvPkts += other->mRecvPackets;
              base->mSendPkts += other->mSendPackets;
          },
          [this](const std::shared_ptr<AbstractRecord>& i) {
              auto* in = static_cast<ConnStatsRecord*>(i.get());
              return std::make_unique<NetMetricData>(in->GetConnection());
          }),
      mSpanAggregator(
          4096,
          [this](std::unique_ptr<AppSpanGroup>& base, const std::shared_ptr<AbstractRecord>& other) {
              base->mRecords.push_back(other);
          },
          [this](const std::shared_ptr<AbstractRecord>& in) { return std::make_unique<AppSpanGroup>(); }),
      mLogAggregator(
          4096,
          [this](std::unique_ptr<AppLogGroup>& base, const std::shared_ptr<AbstractRecord>& other) {
              base->mRecords.push_back(other);
          },
          [this](const std::shared_ptr<AbstractRecord>& in) { return std::make_unique<AppLogGroup>(); }) {
}

// done
std::array<size_t, 2>
NetworkObserverManager::GenerateAggKeyForAppMetric(const std::shared_ptr<AbstractRecord>& abstractRecord) {
    auto* record = static_cast<AbstractAppRecord*>(abstractRecord.get());
    // calculate agg key
    std::array<size_t, 2> hash_result;
    hash_result.fill(0UL);
    std::hash<std::string> hasher;
    auto connection = record->GetConnection();
    if (!connection) {
        LOG_WARNING(sLogger, ("connection is null", ""));
        return {};
    }

    auto connTrackerAttrs = connection->GetConnTrackerAttrs();

    for (size_t j = 0; j < kAppMetricsTable.Elements().size(); j++) {
        int agg_level = static_cast<int>(kAppMetricsTable.Elements()[j].AggType());
        if (agg_level >= kMinAggregationLevel && agg_level <= kMaxAggregationLevel) {
            bool ok = kConnTrackerTable.HasCol(kAppMetricsTable.Elements()[j].Name());
            std::string attr;
            if (ok) {
                attr = connTrackerAttrs[kConnTrackerTable.ColIndex(kAppMetricsTable.Elements()[j].Name())];
            } else if (kAppMetricsTable.Elements()[j].Name() == kRpc.Name()) {
                // record dedicated ...
                attr = record->GetSpanName();
            } else {
                LOG_WARNING(sLogger, ("unknown elements", kAppMetricsTable.Elements()[j].Name()));
                continue;
            }

            int hash_result_index = agg_level - kMinAggregationLevel;
            hash_result[hash_result_index] ^= hasher(attr) + 0x9e3779b9 + (hash_result[hash_result_index] << 6)
                + (hash_result[hash_result_index] >> 2);
        }
    }

    return hash_result;
}

std::array<size_t, 1>
NetworkObserverManager::GenerateAggKeyForSpan(const std::shared_ptr<AbstractRecord>& abstractRecord) {
    auto* record = static_cast<AbstractAppRecord*>(abstractRecord.get());
    // calculate agg key
    // just appid
    std::array<size_t, 1> hash_result;
    hash_result.fill(0UL);
    std::hash<std::string> hasher;
    auto connection = record->GetConnection();
    if (!connection) {
        LOG_WARNING(sLogger, ("connection is null", ""));
        return {};
    }
    auto connTrackerAttrs = connection->GetConnTrackerAttrs();
    auto elements = {kAppId, kIp, kHostName};
    for (auto& x : elements) {
        auto attr = connTrackerAttrs[kConnTrackerTable.ColIndex(x.Name())];
        hash_result[0] ^= hasher(attr) + 0x9e3779b9 + (hash_result[0] << 6) + (hash_result[0] >> 2);
    }

    return hash_result;
}

std::array<size_t, 1>
NetworkObserverManager::GenerateAggKeyForLog(const std::shared_ptr<AbstractRecord>& abstractRecord) {
    auto* record = static_cast<AbstractAppRecord*>(abstractRecord.get());
    // just appid
    std::array<size_t, 1> hash_result;
    hash_result.fill(0UL);
    std::hash<uint64_t> hasher;
    auto connection = record->GetConnection();
    if (!connection) {
        LOG_WARNING(sLogger, ("connection is null", ""));
        return {};
    }
    auto connId = connection->GetConnId();
    // auto connId = record->GetConnId();

    std::array<uint64_t, 3> elements = {uint64_t(connId.fd), uint64_t(connId.tgid), connId.start};
    for (auto& x : elements) {
        hash_result[0] ^= hasher(x) + 0x9e3779b9 + (hash_result[0] << 6) + (hash_result[0] >> 2);
    }

    return hash_result;
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
                    auto ctAttrs = ct->GetConnTrackerAttrs();
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
        mLogEventGroups.emplace_back(std::move(eventGroup));
#else
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
const static std::string METRIC_NAME_TCP_CONN_TOTAL = "arms_npm_tcp_count_by_state";
const static std::string METRIC_NAME_TCP_RECV_PKTS_TOTAL = "arms_npm_recv_packets_total";
const static std::string METRIC_NAME_TCP_RECV_BYTES_TOTAL = "arms_npm_recv_bytes_total";
const static std::string METRIC_NAME_TCP_SENT_PKTS_TOTAL = "arms_npm_sent_packets_total";
const static std::string METRIC_NAME_TCP_SENT_BTES_TOTAL = "arms_npm_sent_bytes_total";

const static std::string EBPF_VALUE = "ebpf";
const static std::string METRIC_VALUE = "metric";
const static std::string TRACE_VALUE = "trace";
const static std::string LOG_VALUE = "log";

const static std::string TAG_AGENT_VERSION_KEY = "agentVersion";
const static std::string TAG_APP_KEY = "app";
const static std::string TAG_V1_VALUE = "v1";
const static std::string TAG_RESOURCE_ID_KEY = "resourceid";
const static std::string TAG_VERSION_KEY = "version";
const static std::string TAG_CLUSTER_ID_KEY = "clusterId";
const static std::string TAG_WORKLOAD_NAME_KEY = "workloadName";
const static std::string TAG_WORKLOAD_KIND_KEY = "workloadKind";
const static std::string TAG_NAMESPACE_KEY = "namespace";
const static std::string TAG_HOST_KEY = "host";
const static std::string TAG_HOSTNAME_KEY = "hostname";
const static std::string TAG_APPLICATION_VALUE = "APPLICATION";
const static std::string TAG_RESOURCE_TYPE_KEY = "resourcetype";


bool NetworkObserverManager::ConsumeMetricAggregateTree(
    const std::chrono::steady_clock::time_point& execTime) { // handler
    if (!this->mFlag || this->mSuspendFlag) {
        return false;
    }

    LOG_DEBUG(sLogger, ("enter aggregator ...", mAppAggregator.NodeCount()));

    WriteLock lk(this->mAppAggLock);
    SIZETAggTree<AppMetricData, std::shared_ptr<AbstractRecord>> aggTree = this->mAppAggregator.GetAndReset();
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
        LOG_DEBUG(sLogger, ("node child size", node->child.size()));
        // convert to a item and push to process queue
        // every node represent an instance of an arms app ...
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer); // per node represent an APP ...
        eventGroup.SetTag(kAppType.MetricKey(), EBPF_VALUE);
        eventGroup.SetTag(kDataType.MetricKey(), METRIC_VALUE);

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
                eventGroup.SetTag(std::string(kAppId.MetricKey()), group->mAppId); // app id
                eventGroup.SetTag(std::string(kIp.MetricKey()), group->mIp); // pod ip
                eventGroup.SetTag(std::string(kAppName.MetricKey()), group->mAppName); // app name
                eventGroup.SetTag(std::string(kHostName.MetricKey()), group->mHost); // pod name

                auto* tagMetric = eventGroup.AddMetricEvent();
                tagMetric->SetName(METRIC_NAME_TAG);
                tagMetric->SetValue(UntypedSingleValue{1.0});
                tagMetric->SetTimestamp(seconds, 0);
                tagMetric->SetTag(TAG_AGENT_VERSION_KEY, TAG_V1_VALUE);
                tagMetric->SetTag(TAG_APP_KEY, group->mAppName); // app ===> appname
                tagMetric->SetTag(TAG_RESOURCE_ID_KEY, group->mAppId); // resourceid -==> pid
                tagMetric->SetTag(TAG_RESOURCE_TYPE_KEY, TAG_APPLICATION_VALUE); // resourcetype ===> APPLICATION
                tagMetric->SetTag(TAG_VERSION_KEY, std::string("v1")); // version ===> v1
                tagMetric->SetTag(TAG_CLUSTER_ID_KEY, mClusterId); // clusterId ===> TODO read from env _cluster_id_
                tagMetric->SetTag(TAG_HOST_KEY, group->mIp); // host ===>
                tagMetric->SetTag(TAG_HOSTNAME_KEY, group->mHost); // hostName ===>
                tagMetric->SetTag(TAG_NAMESPACE_KEY, group->mNamespace); // namespace ===>
                tagMetric->SetTag(TAG_WORKLOAD_KIND_KEY, group->mWorkloadKind); // workloadKind ===>
                tagMetric->SetTag(TAG_WORKLOAD_NAME_KEY, group->mWorkloadName); // workloadName ===>
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

            auto rpc = sourceBuffer->CopyString(group->mSpanName);

            auto destId = sourceBuffer->CopyString(group->mDestId);
            auto endpoint = sourceBuffer->CopyString(group->mEndpoint);
            for (auto* metricsEvent : metrics) {
                // set tags
                metricsEvent->SetTimestamp(seconds, 0);
                metricsEvent->SetTagNoCopy(kWorkloadName.MetricKey(), StringView(workloadName.data, workloadName.size));
                metricsEvent->SetTagNoCopy(kWorkloadKind.MetricKey(), StringView(workloadKind.data, workloadKind.size));
                metricsEvent->SetTagNoCopy(kRpc.MetricKey(), StringView(rpc.data, rpc.size));
                metricsEvent->SetTagNoCopy(kRpcType.MetricKey(), StringView(rpcType.data, rpcType.size));
                metricsEvent->SetTagNoCopy(kCallType.MetricKey(), StringView(callType.data, callType.size));
                metricsEvent->SetTagNoCopy(kCallKind.MetricKey(), StringView(callKind.data, callKind.size));
                metricsEvent->SetTagNoCopy(kEndpoint.MetricKey(), StringView(endpoint.data, endpoint.size));
                metricsEvent->SetTagNoCopy(kDestId.MetricKey(), StringView(destId.data, destId.size));
            }
        });
#ifdef APSARA_UNIT_TEST_MAIN
        mMetricEventGroups.emplace_back(std::move(eventGroup));
#else
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
                auto ctAttrs = ct->GetConnTrackerAttrs();
                auto appname = ctAttrs[kConnTrackerTable.ColIndex(kAppName.Name())];
                if (appname.empty() || ct == nullptr) {
                    LOG_DEBUG(sLogger,
                              ("no app name or ct null, skip, spanname ",
                               record->GetSpanName())("appname", appname)("ct null", ct == nullptr));
                    continue;
                }
                if (!init) {
                    // set app attrs ...
                    eventGroup.SetTag(kAppName.SpanKey(),
                                      ctAttrs[kConnTrackerTable.ColIndex(kAppName.Name())]); // app name
                    eventGroup.SetTag(kAppId.SpanKey(),
                                      ctAttrs[kConnTrackerTable.ColIndex(kAppId.Name())]); // app id
                    eventGroup.SetTag(kHostIp.SpanKey(),
                                      ctAttrs[kConnTrackerTable.ColIndex(kPodIp.Name())]); // pod ip
                    eventGroup.SetTag(kHostName.SpanKey(),
                                      ctAttrs[kConnTrackerTable.ColIndex(kPodName.Name())]); // pod name
                    eventGroup.SetTag(kAppType.SpanKey(), EBPF_VALUE); //
                    eventGroup.SetTag(kDataType.SpanKey(), TRACE_VALUE);
                    for (auto tag = eventGroup.GetTags().begin(); tag != eventGroup.GetTags().end(); tag++) {
                        LOG_DEBUG(sLogger, ("record span tags", "")(std::string(tag->first), std::string(tag->second)));
                    }
                    init = true;
                }
                auto* spanEvent = eventGroup.AddSpanEvent();
                spanEvent->SetTag("app", ctAttrs[kConnTrackerTable.ColIndex(kWorkloadName.Name())]);
                spanEvent->SetTag(kHostName.Name(), ctAttrs[kConnTrackerTable.ColIndex(kHostName.Name())]);
                for (auto element : kAppTraceTable.Elements()) {
                    auto sb = sourceBuffer->CopyString(ctAttrs[kConnTrackerTable.ColIndex(element.Name())]);
                    spanEvent->SetTagNoCopy(element.SpanKey(), StringView(sb.data, sb.size));
                    LOG_DEBUG(sLogger, ("record span tags", "")(std::string(element.SpanKey()), sb.data));
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
        mSpanEventGroups.emplace_back(std::move(eventGroup));
#else
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
    [[maybe_unused]] const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*>& options) {
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
    auto opt = std::get<ObserverNetworkOption*>(options);
    if (!opt) {
        LOG_ERROR(sLogger, ("invalid options", ""));
        return -1;
    }

    UpdateParsers(opt->mEnableProtocols, {});

    mFlag = true;
    mStartUid++;

    mConnectionManager = ConnectionManager::Create();

    mPollKernelFreqMgr.SetPeriod(std::chrono::milliseconds(200));
    mConsumerFreqMgr.SetPeriod(std::chrono::milliseconds(500));

    mCidOffset = GuessContainerIdOffset();

    const char* value = getenv("_cluster_id_");
    if (value != NULL) {
        mClusterId = StringTo<std::string>(value);
    }
    // if purage container mode

    // TODO @qianlu.kk init converger later ...

    std::unique_ptr<AggregateEvent> appTraceEvent = std::make_unique<AggregateEvent>(
        5,
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
        5,
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
    {
        if (opt->mSampleRate < 0 || opt->mSampleRate > 1) {
            LOG_WARNING(sLogger,
                        ("invalid sample rate, must between [0, 1], use default 0.01, givin", opt->mSampleRate));
            opt->mSampleRate = 0.01;
        }
        WriteLock lk(mSamplerLock);
        LOG_DEBUG(sLogger, ("sample rate", opt->mSampleRate));
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
    if (K8sMetadata::GetInstance().Enable()) {
        config.mCidOffset = mCidOffset;
        config.mEnableCidFilter = true;
        std::unique_ptr<AggregateEvent> hostMetaUpdateTask = std::make_unique<AggregateEvent>(
            5,
            [this](const std::chrono::steady_clock::time_point& execTime) {
                std::vector<std::string> podIpVec;
                bool res = K8sMetadata::GetInstance().GetByLocalHostFromServer(podIpVec);
                if (res) {
                    this->HandleHostMetadataUpdate(podIpVec);
                } else {
                    LOG_DEBUG(sLogger, ("failed to request host metada", ""));
                }
                return true;
            },
            [this](int currentUid) { // validator
                auto isStop = !this->mFlag.load() || currentUid != this->mStartUid;
                if (isStop) {
                    LOG_INFO(
                        sLogger,
                        ("stop schedule, mflag", this->mFlag)("currentUid", currentUid)("pluginUid", this->mStartUid));
                }
                return isStop;
            },
            mStartUid);
        mScheduler->PushEvent(std::move(hostMetaUpdateTask));
    }

    config.mDataHandler = [](void* custom_data, struct conn_data_event_t* event) {
        // LOG_DEBUG(sLogger,
        //           ("[DUMP] stats event handle, fd", event->conn_id.fd)("pid", event->conn_id.tgid)(
        //               "start", event->conn_id.start)("role", int(event->role))("startTs", event->start_ts)(
        //               "endTs", event->end_ts)("protocol", int(event->protocol))("req_len", event->request_len)(
        //               "resp_len", event->response_len)("data", event->msg));
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

    mRecordQueue = moodycamel::BlockingConcurrentQueue<std::shared_ptr<AbstractRecord>>(4096);

    LOG_INFO(sLogger, ("begin to start ebpf ... ", ""));
    this->mFlag = true;
    this->RunInThread();
    return 0;
}

void NetworkObserverManager::HandleHostMetadataUpdate(const std::vector<std::string>& podIpVec) {
    std::vector<std::string> newContainerIds;
    std::vector<std::string> expiredContainerIds;
    std::unordered_set<std::string> currentCids;

    for (const auto& ip : podIpVec) {
        auto podInfo = K8sMetadata::GetInstance().GetInfoByIpFromCache(ip);
        if (!podInfo || podInfo->appId == "") {
            // filter appid ...
            LOG_DEBUG(sLogger, (ip, "cannot fetch pod metadata or doesn't have arms label"));
            continue;
        }

        std::string cids;
        for (auto& cid : podInfo->containerIds) {
            cids += cid + ",";
            currentCids.insert(cid);
            if (!mEnabledCids.count(cid)) {
                // if cid doesn't exist in last cid set
                newContainerIds.push_back(cid);
            }
        }
        LOG_DEBUG(sLogger,
                  ("appId", podInfo->appId)("appName", podInfo->appName)("podIp", podInfo->podIp)(
                      "podName", podInfo->podName)("containerId", cids));
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
                    LOG_DEBUG(sLogger,
                              ("app meta not ready, rollback app record, times",
                               times)("connection", appRecord->GetConnection()->DumpConnection()));
                    mRecordQueue.enqueue(std::move(record));
                }
                return;
            } else {
                LOG_DEBUG(sLogger,
                          ("app meta ready, times",
                           record->RollbackCount())("connection", appRecord->GetConnection()->DumpConnection()));
            }

            if (mEnableLog) {
                ProcessRecordAsLog(record);
            }
            if (mEnableMetric) {
                // do converge ...
                // aggregate ...
                ProcessRecordAsMetric(record);
            }
            if (mEnableSpan) {
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
                    LOG_DEBUG(sLogger,
                              ("net meta not ready, rollback net record, times",
                               times)("connection", connStatsRecord->GetConnection()->DumpConnection()));
                    mRecordQueue.enqueue(std::move(record));
                }
                return;
            }
            // do aggregate
            break;
        }
        default:
            break;
    }
}

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

        for (size_t i = 0; i < count; i++) {
            ProcessRecord(items[i]);
        }

        items.clear();
        items.resize(1024);
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
        int ret = mSourceManager->PollPerfBuffers(PluginType::NETWORK_OBSERVE, 4096, &flag, 0);
        if (ret < 0) {
            LOG_WARNING(sLogger, ("poll event err, ret", ret));
        }

        mConnectionManager->Iterations(cnt++);

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

void NetworkObserverManager::AcceptDataEvent(struct conn_data_event_t* event) {
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
    std::vector<std::unique_ptr<AbstractRecord>> records
        = ProtocolParserManager::GetInstance().Parse(protocol, conn, event, mSampler);
    lk.unlock();

    // add records to span/event generate queue
    for (auto& record : records) {
        mRecordQueue.enqueue(std::move(record));
    }
}

void NetworkObserverManager::AcceptNetStatsEvent(struct conn_stats_event_t* event) {
    LOG_DEBUG(
        sLogger,
        ("[DUMP] stats event handle, fd", event->conn_id.fd)("pid", event->conn_id.tgid)("start", event->conn_id.start)(
            "role", int(event->role))("state", int(event->conn_events))("eventTs", event->ts));
    mConnectionManager->AcceptNetStatsEvent(event);
}

void NetworkObserverManager::AcceptNetCtrlEvent(struct conn_ctrl_event_t* event) {
    LOG_DEBUG(sLogger,
              ("[DUMP] ctrl event handle, fd", event->conn_id.fd)("pid", event->conn_id.tgid)(
                  "start", event->conn_id.start)("type", int(event->type))("eventTs", event->ts));
    mConnectionManager->AcceptNetCtrlEvent(event);
}


void NetworkObserverManager::Stop() {
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
        // TODO?? black or delete ??
        LOG_INFO(sLogger, ("UpdateBlacklists cid", cid));
        mSourceManager->SetNetworkObserverCidFilter(cid, false);
    }
}

} // namespace ebpf
} // namespace logtail

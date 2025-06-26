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

#include "NetworkSecurityManager.h"

#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/queue/ProcessQueueItem.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/HashUtil.h"
#include "common/NetworkUtil.h"
#include "common/TimeKeeper.h"
#include "common/TimeUtil.h"
#include "common/magic_enum.hpp"
#include "ebpf/type/AggregateEvent.h"
#include "ebpf/type/table/BaseElements.h"
#include "logger/Logger.h"
#include "models/PipelineEventGroup.h"

namespace logtail::ebpf {

class EBPFServer;

const std::string NetworkSecurityManager::kTcpSendMsgValue = "tcp_sendmsg";
const std::string NetworkSecurityManager::kTcpCloseValue = "tcp_close";
const std::string NetworkSecurityManager::kTcpConnectValue = "tcp_connect";

void HandleNetworkKernelEvent(void* ctx, int, void* data, __u32) {
    if (!ctx) {
        LOG_ERROR(sLogger, ("ctx is null", ""));
        return;
    }
    auto* ss = static_cast<NetworkSecurityManager*>(ctx);
    if (ss == nullptr) {
        return;
    }
    auto* event = static_cast<tcp_data_t*>(data);
    ss->RecordNetworkEvent(event);
}

void HandleNetworkKernelEventLoss(void* ctx, int cpu, __u64 num) {
    if (!ctx) {
        LOG_ERROR(sLogger, ("ctx is null", "")("lost network kernel events num", num));
        return;
    }
    auto* ss = static_cast<NetworkSecurityManager*>(ctx);
    if (ss == nullptr) {
        return;
    }
    ss->UpdateLossKernelEventsTotal(num);
}

void NetworkSecurityManager::UpdateLossKernelEventsTotal(uint64_t cnt) {
    ADD_COUNTER(mLossKernelEventsTotal, cnt);
}

void NetworkSecurityManager::RecordNetworkEvent(tcp_data_t* event) {
    ADD_COUNTER(mRecvKernelEventsTotal, 1);
    KernelEventType type = KernelEventType::TCP_SENDMSG_EVENT;
    switch (event->func) {
        case TRACEPOINT_FUNC_TCP_SENDMSG:
            type = KernelEventType::TCP_SENDMSG_EVENT;
            break;
        case TRACEPOINT_FUNC_TCP_CONNECT:
            type = KernelEventType::TCP_CONNECT_EVENT;
            break;
        case TRACEPOINT_FUNC_TCP_CLOSE:
            type = KernelEventType::TCP_CLOSE_EVENT;
            break;
        default:
            return;
    }
    auto evt = std::make_shared<NetworkEvent>(event->key.pid,
                                              event->key.ktime,
                                              type,
                                              event->timestamp,
                                              event->protocol,
                                              event->family,
                                              event->saddr,
                                              event->daddr,
                                              event->sport,
                                              event->dport,
                                              event->net_ns);
    if (!mCommonEventQueue.try_enqueue(evt)) {
        // don't use move as it will set mProcessEvent to nullptr even if enqueue
        // failed, this is unexpected but don't know why
        LOG_WARNING(sLogger,
                    ("[lost_network_event] try_enqueue failed pid", event->key.pid)("ktime", event->key.ktime)(
                        "saddr", event->saddr)("daddr", event->daddr)("sport", event->sport)("dport", event->dport));
    } else {
        LOG_DEBUG(sLogger,
                  ("[record_network_event] pid", event->key.pid)("ktime", event->key.ktime)("saddr", event->saddr)(
                      "daddr", event->daddr)("sport", event->sport)("dport", event->dport));
    }
}


NetworkSecurityManager::NetworkSecurityManager(const std::shared_ptr<ProcessCacheManager>& base,
                                               const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                                               moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                                               const PluginMetricManagerPtr& metricManager)
    : AbstractManager(base, eBPFAdapter, queue, metricManager),
      mAggregateTree(
          4096,
          [](std::unique_ptr<NetworkEventGroup>& base, const std::shared_ptr<CommonEvent>& other) {
              base->mInnerEvents.emplace_back(other);
          },
          [](const std::shared_ptr<CommonEvent>& ce, std::shared_ptr<SourceBuffer>&) {
              auto* in = static_cast<NetworkEvent*>(ce.get());
              return std::make_unique<NetworkEventGroup>(in->mPid,
                                                         in->mKtime,
                                                         in->mProtocol,
                                                         in->mFamily,
                                                         in->mSaddr,
                                                         in->mDaddr,
                                                         in->mSport,
                                                         in->mDport,
                                                         in->mNetns);
          }) {
}

int NetworkSecurityManager::SendEvents() {
    if (!IsRunning()) {
        return 0;
    }
    auto nowMs = TimeKeeper::GetInstance()->NowMs();
    if (nowMs - mLastSendTimeMs < mSendIntervalMs) {
        return 0;
    }

    WriteLock lk(this->mLock);
    SIZETAggTree<NetworkEventGroup, std::shared_ptr<CommonEvent>> aggTree(this->mAggregateTree.GetAndReset());
    lk.unlock();

    auto nodes = aggTree.GetNodesWithAggDepth(1);
    LOG_DEBUG(sLogger, ("enter aggregator ...", nodes.size()));
    if (nodes.empty()) {
        LOG_DEBUG(sLogger, ("empty nodes...", ""));
        return 0;
    }
    // do we need to aggregate all the events into a eventgroup??
    // use source buffer to hold the memory
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup sharedEventGroup(sourceBuffer);
    PipelineEventGroup eventGroup(sourceBuffer);
    for (auto& node : nodes) {
        // convert to a item and push to process queue
        LOG_DEBUG(sLogger, ("child num", node->mChild.size()));
        auto processCacheMgr = GetProcessCacheManager();
        if (processCacheMgr == nullptr) {
            LOG_WARNING(sLogger, ("ProcessCacheManager is null", ""));
            return 0;
        }
        aggTree.ForEach(node, [&](const NetworkEventGroup* group) {
            auto sharedEvent = sharedEventGroup.CreateLogEvent();
            bool hit = processCacheMgr->FinalizeProcessTags(group->mPid, group->mKtime, *sharedEvent);
            if (!hit) {
                LOG_ERROR(sLogger, ("failed to finalize process tags for pid ", group->mPid)("ktime", group->mKtime));
                return;
            }

            auto protocolSb = sourceBuffer->CopyString(GetProtocolString(group->mProtocol));
            auto familySb = sourceBuffer->CopyString(GetFamilyString(group->mFamily));
            auto saddrSb = sourceBuffer->CopyString(GetAddrString(group->mSaddr));
            auto daddrSb = sourceBuffer->CopyString(GetAddrString(group->mDaddr));
            auto sportSb = sourceBuffer->CopyString(std::to_string(group->mSport));
            auto dportSb = sourceBuffer->CopyString(std::to_string(group->mDport));
            auto netnsSb = sourceBuffer->CopyString(std::to_string(group->mNetns));

            for (const auto& innerEvent : group->mInnerEvents) {
                auto* logEvent = eventGroup.AddLogEvent();
                for (const auto& it : *sharedEvent) {
                    logEvent->SetContentNoCopy(it.first, it.second);
                }
                logEvent->SetContentNoCopy(kL4Protocol.LogKey(), StringView(protocolSb.data, protocolSb.size));
                logEvent->SetContentNoCopy(kFamily.LogKey(), StringView(familySb.data, familySb.size));
                logEvent->SetContentNoCopy(kSaddr.LogKey(), StringView(saddrSb.data, saddrSb.size));
                logEvent->SetContentNoCopy(kDaddr.LogKey(), StringView(daddrSb.data, daddrSb.size));
                logEvent->SetContentNoCopy(kSport.LogKey(), StringView(sportSb.data, sportSb.size));
                logEvent->SetContentNoCopy(kDport.LogKey(), StringView(dportSb.data, dportSb.size));
                logEvent->SetContentNoCopy(kNetNs.LogKey(), StringView(netnsSb.data, netnsSb.size));

                struct timespec ts = ConvertKernelTimeToUnixTime(innerEvent->mTimestamp);
                logEvent->SetTimestamp(ts.tv_sec, ts.tv_nsec);

                // set callnames
                switch (innerEvent->mEventType) {
                    case KernelEventType::TCP_SENDMSG_EVENT: {
                        logEvent->SetContentNoCopy(kCallName.LogKey(),
                                                   StringView(NetworkSecurityManager::kTcpSendMsgValue));
                        logEvent->SetContentNoCopy(kEventType.LogKey(), StringView(AbstractManager::kKprobeValue));
                        break;
                    }
                    case KernelEventType::TCP_CONNECT_EVENT: {
                        logEvent->SetContentNoCopy(kCallName.LogKey(),
                                                   StringView(NetworkSecurityManager::kTcpConnectValue));
                        logEvent->SetContentNoCopy(kEventType.LogKey(), StringView(AbstractManager::kKprobeValue));
                        break;
                    }
                    case KernelEventType::TCP_CLOSE_EVENT: {
                        logEvent->SetContentNoCopy(kCallName.LogKey(),
                                                   StringView(NetworkSecurityManager::kTcpCloseValue));
                        logEvent->SetContentNoCopy(kEventType.LogKey(), StringView(AbstractManager::kKprobeValue));
                        break;
                    }
                    default:
                        break;
                }
            }
        });
    }
    {
        std::lock_guard lk(mContextMutex);
        if (this->mPipelineCtx == nullptr) {
            return 0;
        }
        LOG_DEBUG(sLogger, ("event group size", eventGroup.GetEvents().size()));
        ADD_COUNTER(mPushLogsTotal, eventGroup.GetEvents().size());
        ADD_COUNTER(mPushLogGroupTotal, 1);
        std::unique_ptr<ProcessQueueItem> item
            = std::make_unique<ProcessQueueItem>(std::move(eventGroup), this->mPluginIndex);
        if (QueueStatus::OK != ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
            LOG_WARNING(sLogger,
                        ("configName", mPipelineCtx->GetConfigName())("pluginIdx", this->mPluginIndex)(
                            "[NetworkSecurityEvent] push queue failed!", ""));
        }
    }
    return 0;
}

int NetworkSecurityManager::Init(const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) {
    const auto* securityOpts = std::get_if<SecurityOptions*>(&options);
    if (!securityOpts) {
        LOG_ERROR(sLogger, ("Invalid options type for NetworkSecurityManager", ""));
        return -1;
    }

    mInited = true;

    std::shared_ptr<ScheduleConfig> scheduleConfig
        = std::make_shared<ScheduleConfig>(PluginType::NETWORK_SECURITY, std::chrono::seconds(2));
    ScheduleNext(std::chrono::steady_clock::now(), scheduleConfig);

    std::unique_ptr<PluginConfig> pc = std::make_unique<PluginConfig>();
    pc->mPluginType = PluginType::NETWORK_SECURITY;
    NetworkSecurityConfig config;
    SecurityOptions* opts = std::get<SecurityOptions*>(options);
    config.mOptions = opts->mOptionList;
    config.mPerfBufferSpec
        = {{"sock_secure_output",
            128,
            this,
            [](void* ctx, int cpu, void* data, uint32_t size) { HandleNetworkKernelEvent(ctx, cpu, data, size); },
            [](void* ctx, int cpu, unsigned long long cnt) { HandleNetworkKernelEventLoss(ctx, cpu, cnt); }}};
    pc->mConfig = std::move(config);

    return mEBPFAdapter->StartPlugin(PluginType::NETWORK_SECURITY, std::move(pc)) ? 0 : 1;
}

int NetworkSecurityManager::Destroy() {
    mInited = false;
    return mEBPFAdapter->StopPlugin(PluginType::NETWORK_SECURITY) ? 0 : 1;
}

std::array<size_t, 2> GenerateAggKeyForNetworkEvent(const std::shared_ptr<CommonEvent>& in) {
    auto* event = static_cast<NetworkEvent*>(in.get());
    // calculate agg key
    std::array<size_t, 2> result{};
    result.fill(0UL);
    std::hash<uint64_t> hasher;

    std::array<uint64_t, 2> arr1 = {uint64_t(event->mPid), event->mKtime};
    for (uint64_t x : arr1) {
        AttrHashCombine(result[0], hasher(x));
    }
    std::array<uint64_t, 5> arr2 = {uint64_t(event->mDaddr),
                                    uint64_t(event->mSaddr),
                                    uint64_t(event->mDport),
                                    uint64_t(event->mSport),
                                    uint64_t(event->mNetns)};

    for (uint64_t x : arr2) {
        AttrHashCombine(result[1], hasher(x));
    }
    return result;
}

int NetworkSecurityManager::HandleEvent(const std::shared_ptr<CommonEvent>& event) {
    auto* networkEvent = static_cast<NetworkEvent*>(event.get());
    LOG_DEBUG(sLogger,
              ("receive event, pid", event->mPid)("ktime", event->mKtime)("saddr", networkEvent->mSaddr)(
                  "daddr", networkEvent->mDaddr)("sport", networkEvent->mSport)("dport", networkEvent->mDport)(
                  "eventType", magic_enum::enum_name(event->mEventType)));
    if (networkEvent == nullptr) {
        LOG_ERROR(sLogger,
                  ("failed to convert CommonEvent to NetworkEvent, kernel event type",
                   magic_enum::enum_name(event->GetKernelEventType()))("PluginType",
                                                                       magic_enum::enum_name(event->GetPluginType())));
        return 1;
    }

    // calculate agg key
    std::array<size_t, 2> result = GenerateAggKeyForNetworkEvent(event);

    {
        WriteLock lk(mLock);
        bool ret = mAggregateTree.Aggregate(event, result);
        LOG_DEBUG(sLogger, ("after aggregate", ret));
    }
    return 0;
}

} // namespace logtail::ebpf

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

#include "ebpf/plugin/file_security/FileSecurityManager.h"

#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/queue/ProcessQueueItem.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/HashUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/TimeKeeper.h"
#include "common/TimeUtil.h"
#include "common/magic_enum.hpp"
#include "ebpf/Config.h"
#include "ebpf/type/table/BaseElements.h"
#include "logger/Logger.h"


namespace logtail::ebpf {

class EBPFServer;

const std::string FileSecurityManager::kMmapValue = "security_mmap_file";
const std::string FileSecurityManager::kTruncateValue = "security_path_truncate";
const std::string FileSecurityManager::kPermissionValue = "security_file_permission";
const std::string FileSecurityManager::kPermissionReadValue = "read";
const std::string FileSecurityManager::kPermissionWriteValue = "write";

void HandleFileKernelEvent(void* ctx, int, void* data, __u32) {
    if (!ctx) {
        LOG_ERROR(sLogger, ("ctx is null", ""));
        return;
    }
    auto* ss = static_cast<FileSecurityManager*>(ctx);
    if (ss == nullptr) {
        return;
    }
    auto* event = static_cast<file_data_t*>(data);
    ss->RecordFileEvent(event);
}

void HandleFileKernelEventLoss(void* ctx, int, __u64 num) {
    LOG_WARNING(sLogger, ("kernel event loss, lost_count", num)("type", "file security"));
    if (!ctx) {
        LOG_ERROR(sLogger, ("ctx is null", "")("lost network kernel events num", num));
        return;
    }
    auto* ss = static_cast<FileSecurityManager*>(ctx);
    if (ss == nullptr) {
        return;
    }
    ss->UpdateLossKernelEventsTotal(num);
}

void FileSecurityManager::UpdateLossKernelEventsTotal(uint64_t cnt) {
    ADD_COUNTER(mLossKernelEventsTotal, cnt);
}

void FileSecurityManager::RecordFileEvent(file_data_t* rawEvent) {
    ADD_COUNTER(mRecvKernelEventsTotal, 1);
    if (rawEvent == nullptr) {
        LOG_WARNING(sLogger, ("rawEvent is null", "file event lost"));
        return;
    }
    std::unique_ptr<FileRetryableEvent> event(CreateFileRetryableEvent(rawEvent));
    if (event == nullptr) {
        LOG_WARNING(sLogger, ("FileRetryableEvent is null", "file retry event lost"));
        return;
    }
    if (!event->HandleMessage()) {
        EventCache().AddEvent(std::move(event));
    }
}

FileRetryableEvent* FileSecurityManager::CreateFileRetryableEvent(file_data_t* eventPtr) {
    auto processCacheMgr = GetProcessCacheManager();
    if (processCacheMgr == nullptr) {
        LOG_WARNING(sLogger, ("ProcessCacheManager is null", "file raw event lost"));
        return nullptr;
    }
    return new FileRetryableEvent(std::max(1, INT32_FLAG(ebpf_event_retry_limit)),
                                  *eventPtr,
                                  processCacheMgr->GetProcessCache(),
                                  mCommonEventQueue);
}

FileSecurityManager::FileSecurityManager(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
                                         const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                                         moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                                         EventPool* pool,
                                         RetryableEventCache& retryableEventCache)
    : AbstractManager(processCacheManager, eBPFAdapter, queue, pool),

      mRetryableEventCache(retryableEventCache),
      mAggregateTree(
          4096,
          [](std::unique_ptr<FileEventGroup>& base, const std::shared_ptr<CommonEvent>& other) {
              base->mInnerEvents.emplace_back(other);
          },
          [](const std::shared_ptr<CommonEvent>& ce, std::shared_ptr<SourceBuffer>&) {
              auto* in = static_cast<FileEvent*>(ce.get());
              return std::make_unique<FileEventGroup>(in->mPid, in->mKtime);
          }) {
}

int FileSecurityManager::SendEvents() {
    if (!IsRunning()) {
        return 0;
    }
    auto nowMs = TimeKeeper::GetInstance()->NowMs();
    bool timeTriggered = nowMs - mLastSendTimeMs >= mSendIntervalMs;
    bool eventCountTriggered
        = mAggregateTree.EventCount() >= static_cast<size_t>(INT32_FLAG(ebpf_max_aggregate_events));
    if (!timeTriggered && !eventCountTriggered) {
        return 0;
    }
    mLastSendTimeMs = nowMs;

    SIZETAggTree<FileEventGroup, std::shared_ptr<CommonEvent>> aggTree(this->mAggregateTree.GetAndReset());

    auto nodes = aggTree.GetNodesWithAggDepth(1);
    LOG_DEBUG(sLogger, ("enter aggregator ...", nodes.size()));
    if (nodes.empty()) {
        LOG_DEBUG(sLogger, ("empty nodes...", ""));
        return 0;
    }
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup sharedEventGroup(sourceBuffer);
    PipelineEventGroup eventGroup(sourceBuffer);
    for (auto& node : nodes) {
        LOG_DEBUG(sLogger, ("child num", node->mChild.size()));
        // convert to a item and push to process queue
        auto processCacheMgr = GetProcessCacheManager();
        if (processCacheMgr == nullptr) {
            LOG_WARNING(sLogger, ("ProcessCacheManager is null", ""));
            return 0;
        }
        aggTree.ForEach(node, [&](const FileEventGroup* group) {
            // set process tag
            auto sharedEvent = sharedEventGroup.CreateLogEvent(true, mEventPool);
            bool hit = processCacheMgr->AttachProcessData(group->mPid, group->mKtime, *sharedEvent, eventGroup);

            for (const auto& commonEvent : group->mInnerEvents) {
                auto* innerEvent = static_cast<FileEvent*>(commonEvent.get());
                auto* logEvent = eventGroup.AddLogEvent(true, mEventPool);
                // attach process tags
                for (const auto& it : *sharedEvent) {
                    logEvent->SetContentNoCopy(it.first, it.second);
                }
                struct timespec ts = ConvertKernelTimeToUnixTime(innerEvent->mTimestamp);
                logEvent->SetTimestamp(ts.tv_sec, ts.tv_nsec);
                auto pathSb = sourceBuffer->CopyString(innerEvent->mPath);
                logEvent->SetContentNoCopy(kFilePath.LogKey(), StringView(pathSb.data, pathSb.size));
                // set callnames
                switch (innerEvent->mEventType) {
                    case KernelEventType::FILE_PATH_TRUNCATE: {
                        logEvent->SetContentNoCopy(kCallName.LogKey(), StringView(FileSecurityManager::kTruncateValue));
                        logEvent->SetContentNoCopy(kEventType.LogKey(), StringView(AbstractManager::kKprobeValue));
                        break;
                    }
                    case KernelEventType::FILE_MMAP: {
                        logEvent->SetContentNoCopy(kCallName.LogKey(), StringView(FileSecurityManager::kMmapValue));
                        logEvent->SetContentNoCopy(kEventType.LogKey(), StringView(AbstractManager::kKprobeValue));
                        break;
                    }
                    case KernelEventType::FILE_PERMISSION_EVENT: {
                        logEvent->SetContentNoCopy(kCallName.LogKey(),
                                                   StringView(FileSecurityManager::kPermissionValue));
                        logEvent->SetContentNoCopy(kEventType.LogKey(), StringView(AbstractManager::kKprobeValue));
                        break;
                    }
                    case KernelEventType::FILE_PERMISSION_EVENT_WRITE: {
                        logEvent->SetContentNoCopy(kCallName.LogKey(),
                                                   StringView(FileSecurityManager::kPermissionWriteValue));
                        logEvent->SetContentNoCopy(kEventType.LogKey(), StringView(AbstractManager::kKprobeValue));
                        break;
                    }
                    case KernelEventType::FILE_PERMISSION_EVENT_READ: {
                        logEvent->SetContentNoCopy(kCallName.LogKey(),
                                                   StringView(FileSecurityManager::kPermissionReadValue));
                        logEvent->SetContentNoCopy(kEventType.LogKey(), StringView(AbstractManager::kKprobeValue));
                        break;
                    }
                    default:
                        break;
                }
                if (!hit) {
                    LOG_WARNING(
                        sLogger,
                        ("failed to finalize process tags for pid ", group->mPid)("ktime", group->mKtime)(
                            "path", innerEvent->mPath)("eventType", magic_enum::enum_name(innerEvent->mEventType)));
                }
            }
        });
    }
    {
        if (this->mPipelineCtx == nullptr) {
            return 0;
        }
        LOG_DEBUG(sLogger, ("event group size", eventGroup.GetEvents().size()));
        size_t eventCount = eventGroup.GetEvents().size();
        ADD_COUNTER(mPushLogsTotal, eventCount);
        ADD_COUNTER(mPushLogGroupTotal, 1);
        std::unique_ptr<ProcessQueueItem> item
            = std::make_unique<ProcessQueueItem>(std::move(eventGroup), this->mPluginIndex);
        if (QueueStatus::OK != ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
            LOG_WARNING(sLogger,
                        ("configName", mPipelineCtx->GetConfigName())("pluginIdx", this->mPluginIndex)(
                            "[FileSecurityEvent] push queue failed!", ""));
            ADD_COUNTER(mPushLogFailedTotal, eventCount);
        }
    }
    return 0;
}

int FileSecurityManager::Init() {
    if (mInited) {
        return 0;
    }
    mInited = true;
    return 0;
}

int FileSecurityManager::AddOrUpdateConfig(const CollectionPipelineContext* ctx,
                                           uint32_t index,
                                           const PluginMetricManagerPtr& metricMgr,
                                           const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) {
    if (!ctx) {
        LOG_ERROR(sLogger, ("ctx is null", ""));
        return -1;
    }

    // init metrics ...
    if (metricMgr) {
        MetricLabels eventTypeLabels = {{METRIC_LABEL_KEY_EVENT_TYPE, METRIC_LABEL_VALUE_EVENT_TYPE_LOG}};
        auto ref = metricMgr->GetOrCreateReentrantMetricsRecordRef(eventTypeLabels);
        mRefAndLabels.emplace_back(eventTypeLabels);
        mPushLogsTotal = ref->GetCounter(METRIC_PLUGIN_OUT_EVENTS_TOTAL);
        mPushLogGroupTotal = ref->GetCounter(METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL);
    }

    if (RegisteredConfigCount() != 0) {
        // update
        LOG_DEBUG(sLogger, ("FileSecurity Update", ""));
        // update config (BPF tailcall, filter map etc.)
        if (update(options)) {
            LOG_WARNING(sLogger, ("FileSecurity Update failed", ""));
            return 1;
        }
        // resume
        if (resume(options)) {
            LOG_WARNING(sLogger, ("FileSecurity Resume failed", ""));
            return 1;
        }
        return 0;
    }

    std::unique_ptr<PluginConfig> pc = std::make_unique<PluginConfig>();
    pc->mPluginType = PluginType::FILE_SECURITY;
    FileSecurityConfig config;
    SecurityOptions* opts = std::get<SecurityOptions*>(options);
    if (!opts) {
        LOG_ERROR(sLogger, ("SecurityOptions is null", ""));
        return -1;
    }
    config.mOptions = opts->mOptionList;
    config.mPerfBufferSpec
        = {{"file_secure_output",
            128,
            this,
            [](void* ctx, int cpu, void* data, uint32_t size) { HandleFileKernelEvent(ctx, cpu, data, size); },
            [](void* ctx, int cpu, unsigned long long cnt) { HandleFileKernelEventLoss(ctx, cpu, cnt); }}};
    pc->mConfig = std::move(config);

    bool res = mEBPFAdapter->StartPlugin(PluginType::FILE_SECURITY, std::move(pc));
    if (!res) {
        LOG_WARNING(sLogger, ("start file security plugin", "failed"));
        return 1;
    }
    LOG_INFO(sLogger, ("start file security plugin", "success"));

    mConfigName = ctx->GetConfigName();
    mMetricMgr = metricMgr;
    mPluginIndex = index;
    mPipelineCtx = ctx;
    mQueueKey = ctx->GetProcessQueueKey();
    mRegisteredConfigCount = 1;

    return 0;
}

int FileSecurityManager::RemoveConfig(const std::string&) {
    for (auto& item : mRefAndLabels) {
        if (mMetricMgr) {
            mMetricMgr->ReleaseReentrantMetricsRecordRef(item);
        }
    }
    mRegisteredConfigCount = 0;
    mConfigName.clear();
    auto res = mEBPFAdapter->StopPlugin(PluginType::FILE_SECURITY);
    if (res) {
        LOG_INFO(sLogger, ("stop file security plugin", "success"));
    } else {
        LOG_WARNING(sLogger, ("stop file security plugin", "failed"));
    }
    mRetryableEventCache.Clear();
    return res ? 0 : 1;
}

std::array<size_t, 2> GenerateAggKeyForFileEvent(const std::shared_ptr<CommonEvent>& ce) {
    auto* event = static_cast<FileEvent*>(ce.get());
    // calculate agg key
    std::array<size_t, 2> result{};
    result.fill(0UL);
    std::hash<uint64_t> hasher;
    std::array<uint64_t, 2> arr = {uint64_t(event->mPid), event->mKtime};
    for (uint64_t x : arr) {
        AttrHashCombine(result[0], hasher(x));
    }
    std::hash<std::string> strHasher;
    AttrHashCombine(result[1], strHasher(event->mPath));
    return result;
}

int FileSecurityManager::HandleEvent(const std::shared_ptr<CommonEvent>& event) {
    if (!event) {
        LOG_ERROR(sLogger, ("cannot handle", "event is null"));
        return 1;
    }
    auto* fileEvent = static_cast<FileEvent*>(event.get());
    LOG_DEBUG(sLogger,
              ("receive event, pid", fileEvent->mPid)("ktime", fileEvent->mKtime)("path", fileEvent->mPath)(
                  "eventType", magic_enum::enum_name(event->mEventType)));
    if (fileEvent == nullptr) {
        LOG_ERROR(sLogger,
                  ("failed to convert CommonEvent to FileEvent, kernel event type",
                   magic_enum::enum_name(event->GetKernelEventType()))("PluginType",
                                                                       magic_enum::enum_name(event->GetPluginType())));
        return 1;
    }

    // calculate agg key
    std::array<size_t, 2> hashResult = GenerateAggKeyForFileEvent(event);
    {
        WriteLock lk(mLock);
        bool ret = mAggregateTree.Aggregate(event, hashResult);
        LOG_DEBUG(sLogger, ("after aggregate", ret));
    }
    return 0;
}

int FileSecurityManager::Destroy() {
    mInited = false;
    LOG_INFO(sLogger, ("FileSecurityManager destroy", ""));
    return 0;
}

} // namespace logtail::ebpf

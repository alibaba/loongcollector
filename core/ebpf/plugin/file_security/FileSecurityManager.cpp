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

#include "FileSecurityManager.h"

#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/queue/ProcessQueueItem.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/HashUtil.h"
#include "common/TimeKeeper.h"
#include "common/TimeUtil.h"
#include "common/magic_enum.hpp"
#include "ebpf/Config.h"
#include "ebpf/type/table/BaseElements.h"
#include "logger/Logger.h"

namespace logtail {
namespace ebpf {

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
                                         const PluginMetricManagerPtr& metricManager)
    : AbstractManager(processCacheManager, eBPFAdapter, queue, metricManager),
      mAggregateTree(
          4096,
          [](std::unique_ptr<FileEventGroup>& base, const std::shared_ptr<CommonEvent>& other) {
              base->mInnerEvents.emplace_back(other);
          },
          [](const std::shared_ptr<CommonEvent>& ce, std::shared_ptr<SourceBuffer>&) {
              auto* in = static_cast<FileEvent*>(ce.get());
              return std::make_unique<FileEventGroup>(in->mPid, in->mKtime, in->mPath);
          }) {
}

int FileSecurityManager::SendEvents() {
    if (!IsRunning()) {
        return 0;
    }
    auto nowMs = TimeKeeper::GetInstance()->NowMs();
    if (nowMs - mLastSendTimeMs < mSendIntervalMs) {
        return 0;
    }

    WriteLock lk(this->mLock);
    SIZETAggTree<FileEventGroup, std::shared_ptr<CommonEvent>> aggTree(this->mAggregateTree.GetAndReset());
    lk.unlock();

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
            auto sharedEvent = sharedEventGroup.CreateLogEvent();
            bool hit = processCacheMgr->FinalizeProcessTags(group->mPid, group->mKtime, *sharedEvent);
            if (!hit) {
                LOG_WARNING(sLogger, ("failed to finalize process tags for pid ", group->mPid)("ktime", group->mKtime));
            }

            auto pathSb = sourceBuffer->CopyString(group->mPath);
            for (const auto& innerEvent : group->mInnerEvents) {
                auto* logEvent = eventGroup.AddLogEvent();
                // attach process tags
                for (const auto& it : *sharedEvent) {
                    logEvent->SetContentNoCopy(it.first, it.second);
                }
                struct timespec ts = ConvertKernelTimeToUnixTime(innerEvent->mTimestamp);
                logEvent->SetTimestamp(ts.tv_sec, ts.tv_nsec);
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
        int maxRetry = 5;
        for (int retry = 0; retry < maxRetry; ++retry) {
            if (QueueStatus::OK == ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (retry == maxRetry - 1) {
                LOG_WARNING(sLogger,
                            ("configName", mPipelineCtx->GetConfigName())("pluginIdx", this->mPluginIndex)(
                                "[ProcessSecurityEvent] push queue failed!", ""));
                // TODO: Alarm discard data
            }
        }
    }
    return 0;
}

int FileSecurityManager::Init(const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) {
    std::unique_ptr<PluginConfig> pc = std::make_unique<PluginConfig>();
    pc->mPluginType = PluginType::FILE_SECURITY;
    FileSecurityConfig config;
    SecurityOptions* opts = std::get<SecurityOptions*>(options);
    config.mOptions = opts->mOptionList;
    config.mPerfBufferSpec
        = {{"file_secure_output",
            128,
            this,
            [](void* ctx, int cpu, void* data, uint32_t size) { HandleFileKernelEvent(ctx, cpu, data, size); },
            [](void* ctx, int cpu, unsigned long long cnt) { HandleFileKernelEventLoss(ctx, cpu, cnt); }}};
    pc->mConfig = std::move(config);

    auto res = mEBPFAdapter->StartPlugin(PluginType::FILE_SECURITY, std::move(pc));
    LOG_INFO(sLogger, ("start file probe, status", res));
    if (!res) {
        LOG_WARNING(sLogger, ("failed to start file probe", ""));
        return 1;
    }

    mInited = true;
    return 0;
}

std::array<size_t, 2> GenerateAggKeyForFileEvent(const std::shared_ptr<CommonEvent>& ce) {
    FileEvent* event = static_cast<FileEvent*>(ce.get());
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

int FileSecurityManager::PollPerfBuffer(int maxWaitTimeMs) {
    auto now = TimeKeeper::GetInstance()->NowSec();
    if (now > mLastEventCacheRetryTime + INT32_FLAG(ebpf_event_retry_interval_sec)) {
        EventCache().HandleEvents();
        mLastEventCacheRetryTime = now;
        LOG_DEBUG(sLogger, ("retry cache size", EventCache().Size()));
    }
    int zero = 0;
    return mEBPFAdapter->PollPerfBuffers(PluginType::FILE_SECURITY, kDefaultMaxBatchConsumeSize, &zero, maxWaitTimeMs);
}

int FileSecurityManager::HandleEvent(const std::shared_ptr<CommonEvent>& event) {
    if (!event) {
        return 1;
    }
    auto* fileEvent = static_cast<FileEvent*>(event.get());
    LOG_DEBUG(sLogger,
              ("receive event, pid", event->mPid)("ktime", event->mKtime)("path", fileEvent->mPath)(
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

    auto res = mEBPFAdapter->StopPlugin(PluginType::FILE_SECURITY);
    LOG_INFO(sLogger, ("stop file plugin, status", res));
    mRetryableEventCache.Clear();
    return res ? 0 : 1;
}

} // namespace ebpf
} // namespace logtail

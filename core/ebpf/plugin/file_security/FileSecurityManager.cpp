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

#include "FileSecurityManager.h"

#include "common/magic_enum.hpp"
#include "ebpf/Config.h"
#include "ebpf/type/PeriodicalEvent.h"
#include "logger/Logger.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/queue/ProcessQueueItem.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"

namespace logtail {
namespace ebpf {


void HandleFileKernelEvent(void* ctx, int cpu, void* data, __u32 data_sz) {
    if (!ctx) {
        LOG_ERROR(sLogger, ("ctx is null", ""));
        return;
    }
    FileSecurityManager* ss = static_cast<FileSecurityManager*>(ctx);
    if (ss == nullptr)
        return;
    file_data_t* event = static_cast<file_data_t*>(data);
    ss->RecordFileEvent(event);
    // TODO @qianlu.kk  self monitor
    //   ss->UpdateRecvKernelEventsTotal();
    return;
}

void HandleFileKernelEventLoss(void* ctx, int cpu, __u64 num) {
    if (!ctx) {
        LOG_ERROR(sLogger, ("ctx is null", "")("lost network kernel events num", num));
        return;
    }
    FileSecurityManager* ss = static_cast<FileSecurityManager*>(ctx);
    if (ss == nullptr)
        return;
    //   ss->UpdateLossKernelEventsTotal(lost_cnt);

    return;
}


void FileSecurityManager::RecordFileEvent(file_data_t* event) {
    KernelEventType type;
    switch (event->func) {
        case TRACEPOINT_FUNC_SECURITY_FILE_PERMISSION:
            type = KernelEventType::FILE_PERMISSION_EVENT;
            break;
        case TRACEPOINT_FUNC_SECURITY_MMAP_FILE:
            type = KernelEventType::FILE_MMAP;
            break;
        case TRACEPOINT_FUNC_SECURITY_PATH_TRUNCATE:
            type = KernelEventType::FILE_PATH_TRUNCATE;
            break;
        default:
            return;
    }
    std::string path = &event->path[4];
    std::shared_ptr<FileEvent> evt
        = std::make_shared<FileEvent>(event->key.pid, event->key.ktime, type, event->timestamp, path);
    LOG_DEBUG(sLogger, ("[record_file_event] path", path));
    // compose to generic event
    // enqueue
    mCommonEventQueue.enqueue(std::move(evt));
}

FileSecurityManager::FileSecurityManager(std::shared_ptr<BaseManager>& baseMgr,
                                         std::shared_ptr<SourceManager> sourceManager,
                                         moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                                         std::shared_ptr<Timer> scheduler)
    : AbstractManager(baseMgr, sourceManager, queue, scheduler),
      mAggregateTree(
          4096,
          [this](std::unique_ptr<FileEventGroup>& base, const std::shared_ptr<FileEvent>& other) {
              base->mInnerEvents.emplace_back(std::move(other));
          },
          [this](const std::shared_ptr<FileEvent>& in) {
              return std::make_unique<FileEventGroup>(in->mPid, in->mKtime, in->mPath);
          }) {
}

int FileSecurityManager::Init(const std::variant<SecurityOptions*, ObserverNetworkOption*> options) {
    // set init flag ...
    mFlag = true;
    mStartUid++;
    std::unique_ptr<AggregateEvent> event = std::make_unique<AggregateEvent>(
        2,
        [this](const std::chrono::steady_clock::time_point& execTime) { // handler
            if (!this->mFlag || this->mSuspendFlag) {
                return false;
            }

            WriteLock lk(this->mLock);
            auto aggTree = std::move(this->mAggregateTree);
            lk.unlock();

            auto nodes = aggTree.GetNodesWithAggDepth(1);
            LOG_DEBUG(sLogger, ("enter aggregator ...", nodes.size()));
            if (nodes.empty()) {
                LOG_DEBUG(sLogger, ("empty nodes...", ""));
                return true;
            }

            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            for (auto& node : nodes) {
                LOG_DEBUG(sLogger, ("child num", node->child.size()));
                // convert to a item and push to process queue
                SizedMap processTags;
                bool init = false;
                aggTree.ForEach(node, [&](const FileEventGroup* group) {
                    // set process tag
                    if (!init) {
                        auto bm = GetBaseManager();
                        if (bm == nullptr) {
                            LOG_WARNING(sLogger, ("basemanager is null", ""));
                            return;
                        }
                        processTags = bm->FinalizeProcessTags(sourceBuffer, group->mPid, group->mKtime);
                        init = true;
                    }
                    if (processTags.mInner.empty()) {
                        LOG_ERROR(sLogger,
                                  ("failed to finalize process tags for pid ", group->mPid)("ktime", group->mKtime));
                        return;
                    }

                    for (auto innerEvent : group->mInnerEvents) {
                        auto* logEvent = eventGroup.AddLogEvent();
                        // attach process tags
                        for (auto it = processTags.mInner.begin(); it != processTags.mInner.end(); it++) {
                            logEvent->SetContentNoCopy(it->first, it->second);
                        }
                        auto ts = innerEvent->mTimestamp + this->mTimeDiff.count();
                        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::nanoseconds(ts));
                        // set timestamp
                        logEvent->SetTimestamp(seconds.count(), ts);
                        logEvent->SetContent("path", group->mPath);
                        // set callnames
                        switch (innerEvent->mEventType) {
                            case KernelEventType::FILE_PATH_TRUNCATE: {
                                logEvent->SetContent("call_name", std::string("security_path_truncate"));
                                logEvent->SetContent("event_type", std::string("kprobe"));
                                break;
                            }
                            case KernelEventType::FILE_MMAP: {
                                logEvent->SetContent("call_name", std::string("security_mmap_file"));
                                logEvent->SetContent("event_type", std::string("kprobe"));
                                break;
                            }
                            case KernelEventType::FILE_PERMISSION_EVENT: {
                                logEvent->SetContent("call_name", std::string("security_file_permission"));
                                logEvent->SetContent("event_type", std::string("kprobe"));
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
                    return true;
                }
                LOG_DEBUG(sLogger, ("event group size", eventGroup.GetEvents().size()));
                std::unique_ptr<ProcessQueueItem> item
                    = std::make_unique<ProcessQueueItem>(std::move(eventGroup), this->mPluginIndex);
                if (QueueStatus::OK != ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
                    LOG_WARNING(sLogger,
                                ("configName", mPipelineCtx->GetConfigName())("pluginIdx", this->mPluginIndex)(
                                    "[FileSecurityEvent] push queue failed!", ""));
                }
            }
            aggTree.Clear();
            return true;
        },
        [this](int currentUid) { // validator
            auto isStop = !this->mFlag.load() || currentUid != this->mStartUid;
            if (isStop) {
                LOG_INFO(sLogger,
                            ("stop schedule, mflag", this->mFlag)("currentUid", currentUid)("pluginUid",
                                                                                                     this->mStartUid));
            }
            return isStop;
        },
        mStartUid);

    mScheduler->PushEvent(std::move(event));

    std::unique_ptr<PluginConfig> pc = std::make_unique<PluginConfig>();
    pc->mPluginType = PluginType::FILE_SECURITY;
    FileSecurityConfig config;
    SecurityOptions* opts = std::get<SecurityOptions*>(options);
    config.options_ = opts->mOptionList;
    config.mPerfBufferSpec
        = {{"file_secure_output",
            128,
            this,
            [](void* ctx, int cpu, void* data, uint32_t size) { HandleFileKernelEvent(ctx, cpu, data, size); },
            [](void* ctx, int cpu, unsigned long long cnt) { HandleFileKernelEventLoss(ctx, cpu, cnt); }}};
    pc->mConfig = std::move(config);

    return mSourceManager->StartPlugin(PluginType::FILE_SECURITY, std::move(pc)) ? 0 : 1;
}

std::array<size_t, 2> GenerateAggKey(const std::shared_ptr<FileEvent> event) {
    // calculate agg key
    std::array<size_t, 2> hash_result;
    hash_result.fill(0UL);
    std::hash<uint64_t> hasher;
    std::array<uint64_t, 2> arr = {uint64_t(event->mPid), event->mKtime};
    for (uint64_t x : arr) {
        hash_result[0] ^= hasher(x) + 0x9e3779b9 + (hash_result[0] << 6) + (hash_result[0] >> 2);
    }
    std::hash<std::string> strHasher;
    hash_result[1] ^= strHasher(event->mPath) + 0x9e3779b9 + (hash_result[0] << 6) + (hash_result[0] >> 2);
    return hash_result;
}

int FileSecurityManager::HandleEvent(const std::shared_ptr<CommonEvent> event) {
    auto fileEvent = std::dynamic_pointer_cast<FileEvent>(event);
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
    std::array<size_t, 2> hash_result = GenerateAggKey(fileEvent);
    {
        WriteLock lk(mLock);
        bool ret = mAggregateTree.Aggregate(fileEvent, hash_result);
        LOG_DEBUG(sLogger, ("after aggregate", ret));
    }
    return 0;
}

int FileSecurityManager::Destroy() {
    mFlag = false;
    return mSourceManager->StopPlugin(PluginType::FILE_SECURITY) ? 0 : 1;
}

} // namespace ebpf
} // namespace logtail

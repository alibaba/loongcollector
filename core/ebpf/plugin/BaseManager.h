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

#pragma once

#include <array>
#include <atomic>
#include <queue>
#include <regex>
#include <set>
#include <unordered_map>

#include "common/LRUCache.h"
#include "common/ProcParser.h"
#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/SourceManager.h"
#include "ebpf/type/CommonDataEvent.h"
#include "ebpf/type/ProcessEvent.h"
#include "models/PipelineEventGroup.h"
#include "util/FrequencyManager.h"

namespace logtail {
namespace ebpf {

class BaseManager {
public:
    BaseManager() = delete;
    BaseManager(std::shared_ptr<SourceManager> sm,
                const std::string& hostName,
                const std::string& hostPathPrefix,
                moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue)
        : mSourceManager(sm),
          mCache(65535, 1024),
          mProcParser(hostPathPrefix),
          mHostName(hostName),
          mHostPathPrefix(hostPathPrefix),
          mCommonEventQueue(queue) {}
    ~BaseManager() {}

    bool ContainsKey(const std::string& key) const { return mCache.contains(key); }

    // thread-safe
    const std::shared_ptr<MsgExecveEventUnix> LookupCache(const std::string& key) { return mCache.get(key); }

    // thread-safe
    void ReleaseCache(const std::string& key) { mCache.remove(key); }

    // thread-safe
    void UpdateCache(const std::string& key, std::shared_ptr<MsgExecveEventUnix> value) { mCache.insert(key, value); }

    std::vector<std::shared_ptr<Procs>> ListRunningProcs();
    int WriteProcToBPFMap(const std::shared_ptr<Procs>& proc);
    int SyncAllProc();
    int PushExecveEvent(const std::shared_ptr<Procs> proc);

    void RecordExecveEvent(msg_execve_event* event_ptr);
    void RecordExitEvent(msg_exit* event_ptr);
    void RecordCloneEvent(msg_clone_event* event_ptr);

    std::string GenerateExecId(uint32_t pid, uint64_t ktime);
    std::string GenerateParentExecId(const std::shared_ptr<MsgExecveEventUnix> event);

    void MarkProcessEventFlushStatus(bool isFlush) { mFlushProcessEvent = isFlush; }

    SizedMap FinalizeProcessTags(std::shared_ptr<SourceBuffer> sb, uint32_t pid, uint64_t ktime);

    bool FinalizeProcessTags(PipelineEventGroup& eventGroup, uint32_t pid, uint64_t ktime);

    void PollPerfBuffers();

    bool Init();
    void Stop();

private:
    void HandleCacheUpdate();

    std::atomic_bool mInited = false;
    std::atomic_bool mFlag = false;
    std::shared_ptr<SourceManager> mSourceManager = nullptr;
    lru11::Cache<std::string, std::shared_ptr<MsgExecveEventUnix>, std::mutex> mCache;
    ProcParser mProcParser;
    std::string mHostName;
    std::string mHostPathPrefix;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& mCommonEventQueue;
    std::regex mPidRegex = std::regex("\\d+");

    // record execve event, used to update process cache ...
    moodycamel::BlockingConcurrentQueue<std::unique_ptr<MsgExecveEventUnix>> mRecordQueue;


    int mMaxBatchConsumeSize = 1024;
    int mMaxWaitTimeMS = 200;

    std::atomic_bool mFlushProcessEvent = false;
    std::future<void> mPoller;
    std::future<void> mCacheUpdater;

    FrequencyManager mFrequencyMgr;
};

} // namespace ebpf
} // namespace logtail

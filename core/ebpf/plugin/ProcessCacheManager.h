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

#include <coolbpf/security/data_msg.h>

#include <atomic>
#include <future>
#include <unordered_map>

#include "common/LRUCache.h"
#include "common/ProcParser.h"
#include "common/memory/SourceBuffer.h"
#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/SourceManager.h"
#include "ebpf/type/CommonDataEvent.h"
#include "ebpf/type/ProcessEvent.h"
#include "models/SizedContainer.h"
#include "util/FrequencyManager.h"

namespace logtail {
namespace ebpf {

class ProcessCacheManager {
public:
    ProcessCacheManager() = delete;
    ProcessCacheManager(std::shared_ptr<SourceManager>& sm,
                        const std::string& hostName,
                        const std::string& hostPathPrefix,
                        moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue)
        : mSourceManager(sm),
          mCache(65535, 1024),
          //   mDataCache(1024, 256),
          mProcParser(hostPathPrefix),
          mHostName(hostName),
          mHostPathPrefix(hostPathPrefix),
          mCommonEventQueue(queue) {}
    ~ProcessCacheManager() {}

    bool ContainsKey(const data_event_id& key) const { return mCache.contains(key); }

    // thread-safe
    std::shared_ptr<MsgExecveEventUnix> LookupCache(const data_event_id& key) { return mCache.get(key); }

    // thread-safe
    void ReleaseCache(const data_event_id& key) { mCache.remove(key); }

    // thread-safe
    void UpdateCache(const data_event_id& key, std::shared_ptr<MsgExecveEventUnix>& value) {
        mCache.insert(key, value);
    }

    std::vector<std::shared_ptr<Procs>> ListRunningProcs();
    int WriteProcToBPFMap(const std::shared_ptr<Procs>& proc);
    int SyncAllProc();
    int PushExecveEvent(const std::shared_ptr<Procs>& proc);

    void RecordExecveEvent(msg_execve_event* eventPtr);
    void PostHandlerExecveEvent(msg_execve_event*, std::unique_ptr<MsgExecveEventUnix>&&);
    void RecordExitEvent(msg_exit* eventPtr);
    void RecordCloneEvent(msg_clone_event* eventPtr);
    void RecordDataEvent(msg_data* eventPtr);

    std::string GenerateExecId(uint32_t pid, uint64_t ktime);
    std::string GenerateParentExecId(const std::shared_ptr<MsgExecveEventUnix>& event);

    void MarkProcessEventFlushStatus(bool isFlush) { mFlushProcessEvent = isFlush; }

    SizedMap FinalizeProcessTags(std::shared_ptr<SourceBuffer>& sb, uint32_t pid, uint64_t ktime);

    void PollPerfBuffers();

    void DataAdd(msg_data* data);
    std::string DataGetAndRemove(data_event_desc*);

    bool Init();
    void Stop();

private:
    void HandleCacheUpdate();

    std::atomic_bool mInited = false;
    std::atomic_bool mRunFlag = false;
    std::shared_ptr<SourceManager> mSourceManager = nullptr;

    struct DataEventIdHash {
        std::size_t operator()(const data_event_id& deid) const { return deid.pid ^ ((deid.time >> 12) << 16); }
    };

    struct DataEventIdEqual {
        bool operator()(const data_event_id& lhs, const data_event_id& rhs) const {
            return lhs.pid == rhs.pid && lhs.time == rhs.time;
        }
    };

    using ExecveEventMap = std::unordered_map<
        data_event_id,
        std::list<lru11::KeyValuePair<data_event_id, std::shared_ptr<MsgExecveEventUnix>>>::iterator,
        DataEventIdHash,
        DataEventIdEqual>;
    lru11::Cache<data_event_id, std::shared_ptr<MsgExecveEventUnix>, std::mutex, ExecveEventMap> mCache;
    using DataEventMap = std::unordered_map<data_event_id, std::string, DataEventIdHash, DataEventIdEqual>;

    std::mutex mDataCacheMutex;
    DataEventMap mDataCache; // TODO：ebpf中也没区分filename和args，如果两者都超长会导致filename被覆盖为args

    // lru11::Cache<std::vector<uint64_t>, std::shared_ptr<std::string>, std::mutex, std::map<std::vector<uint64_t>,
    // std::shared_ptr<std::string>>> mDataCache;
    ProcParser mProcParser;
    std::string mHostName;
    std::filesystem::path mHostPathPrefix;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& mCommonEventQueue;

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

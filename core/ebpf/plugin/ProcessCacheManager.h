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
#include <ctime>

#include <atomic>
#include <deque>
#include <future>
#include <mutex>
#include <unordered_map>

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
                        moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue);
    ~ProcessCacheManager() = default;

    bool ContainsKey(const data_event_id& key) const {
        std::lock_guard<std::mutex> lock(mCacheMutex);
        return mCache.find(key) != mCache.end();
    }

    // thread-safe
    std::shared_ptr<MsgExecveEventUnix> LookupCache(const data_event_id& key) {
        std::lock_guard<std::mutex> lock(mCacheMutex);
        auto it = mCache.find(key);
        if (it != mCache.end()) {
            return it->second;
        }
        return nullptr;
    }

    std::vector<std::shared_ptr<Proc>> ListRunningProcs();
    int WriteProcToBPFMap(const std::shared_ptr<Proc>& proc);
    int SyncAllProc();
    void ProcToExecveEvent(const Proc& proc, MsgExecveEventUnix& event);
    void PushExecveEvent(const Proc& proc);

    void RecordExecveEvent(msg_execve_event* eventPtr);
    void PostHandlerExecveEvent(msg_execve_event*, std::shared_ptr<MsgExecveEventUnix>&&);
    void RecordExitEvent(msg_exit* eventPtr);
    void RecordCloneEvent(msg_clone_event* eventPtr);
    void RecordDataEvent(msg_data* eventPtr);

    std::string GenerateExecId(uint32_t pid, uint64_t ktime);
    std::string GenerateParentExecId(const std::shared_ptr<MsgExecveEventUnix>& event);

    void MarkProcessEventFlushStatus(bool isFlush) { mFlushProcessEvent = isFlush; }

    SizedMap FinalizeProcessTags(std::shared_ptr<SourceBuffer>& sb, uint32_t pid, uint64_t ktime);

    bool Init();
    void Stop();

private:
    void pollPerfBuffers();

    void handleCacheUpdate(std::shared_ptr<MsgExecveEventUnix>&& event);

    // thread-safe
    void releaseCache(const data_event_id& key);
    // thread-safe
    void updateCache(const data_event_id& key, std::shared_ptr<MsgExecveEventUnix>&& value);
    // thread-safe
    void clearCache();
    // NOT thread-safe
    void enqueueExpiredEntry(const data_event_id& key, time_t ktime);
    // NOT thread-safe
    void clearExpiredCache(time_t ktime);


    // NOT thread-safe
    void dataAdd(msg_data* data);
    // NOT thread-safe
    std::string dataGetAndRemove(data_event_desc*);
    // NOT thread-safe
    void clearExpiredData(time_t ktime);

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

    using ExecveEventMap
        = std::unordered_map<data_event_id, std::shared_ptr<MsgExecveEventUnix>, DataEventIdHash, DataEventIdEqual>;
    mutable std::mutex mCacheMutex;
    ExecveEventMap mCache;
    struct ExitedEntry {
        time_t time;
        data_event_id key;
    };
    std::deque<ExitedEntry> mCacheExpireQueue;

    using DataEventMap = std::unordered_map<data_event_id, std::string, DataEventIdHash, DataEventIdEqual>;
    mutable std::mutex mDataMapMutex;
    DataEventMap mDataMap; // TODO：ebpf中也没区分filename和args，如果两者都超长会导致filename被覆盖为args
    std::chrono::time_point<std::chrono::system_clock> mLastDataMapClearTime = std::chrono::system_clock::now();

    ProcParser mProcParser;
    std::string mHostName;
    std::filesystem::path mHostPathPrefix;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& mCommonEventQueue;

    std::atomic_bool mFlushProcessEvent = false;
    std::future<void> mPoller;

    FrequencyManager mFrequencyMgr;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ManagerUnittest;
    friend class ProcessCacheManagerUnittest;
#endif
};

} // namespace ebpf
} // namespace logtail

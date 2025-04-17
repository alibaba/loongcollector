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

#pragma once

#include <coolbpf/security/bpf_process_event_type.h>
#include <coolbpf/security/data_msg.h>
#include <ctime>

#include <atomic>
#include <future>
#include <mutex>
#include <unordered_map>

#include "common/ProcParser.h"
#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/EBPFAdapter.h"
#include "ebpf/plugin/ProcessCache.h"
#include "ebpf/type/CommonDataEvent.h"
#include "ebpf/util/FrequencyManager.h"
#include "models/LogEvent.h"
#include "monitor/metric_models/MetricTypes.h"

namespace logtail {
namespace ebpf {

class ProcessCacheManager {
public:
    static constexpr size_t kInitDataMapSize = 1024UL;
    static constexpr size_t kMaxCacheSize = 4194304UL;
    static constexpr size_t kMaxDataMapSize = kInitDataMapSize * 4;
    ProcessCacheManager() = delete;
    ProcessCacheManager(std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                        const std::string& hostName,
                        const std::string& hostPathPrefix,
                        moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                        CounterPtr pollEventsTotal,
                        CounterPtr lossEventsTotal,
                        CounterPtr cacheMissTotal,
                        IntGaugePtr cacheSize);
    ~ProcessCacheManager() = default;

    bool Init();
    void Stop();

    void UpdateRecvEventTotal(uint64_t count = 1);
    void UpdateLossEventTotal(uint64_t count);

    void RecordExecveEvent(msg_execve_event* eventPtr);
    void RecordExitEvent(msg_exit* eventPtr);
    void RecordCloneEvent(msg_clone_event* eventPtr);
    void RecordDataEvent(msg_data* eventPtr);

    std::string GenerateExecId(uint32_t pid, uint64_t ktime);
    void MarkProcessEventFlushStatus(bool isFlush) { mFlushProcessEvent = isFlush; }

    bool FinalizeProcessTags(uint32_t pid, uint64_t ktime, LogEvent& logEvent);

private:
    int syncAllProc();
    std::vector<std::shared_ptr<Proc>> listRunningProcs();
    int writeProcToBPFMap(const std::shared_ptr<Proc>& proc);
    void pushProcEvent(const Proc& proc);

    void pollPerfBuffers();

    std::shared_ptr<ProcessCacheValue> procToProcessCacheValue(const Proc& proc);
    std::shared_ptr<ProcessCacheValue> msgExecveEventToProcessCacheValue(const msg_execve_event& event);
    bool fillProcessDataFields(const msg_execve_event& event, ProcessCacheValue& cacheValue);
    std::shared_ptr<ProcessCacheValue> msgCloneEventToProcessCacheValue(const msg_clone_event& event);

    // thread-safe
    void dataAdd(msg_data* data);
    // thread-safe
    std::string dataGetAndRemove(const data_event_desc* desc);
    // NOT thread-safe
    void clearExpiredData(time_t ktime);

    std::atomic_bool mInited = false;
    std::atomic_bool mRunFlag = false;
    std::shared_ptr<EBPFAdapter> mEBPFAdapter = nullptr;

    ProcessCache mProcessCache;
    using DataEventMap = std::unordered_map<data_event_id, std::string, DataEventIdHash, DataEventIdEqual>;
    mutable std::mutex mDataMapMutex;
    DataEventMap mDataMap; // TODO：ebpf中也没区分filename和args，如果两者都超长会导致filename被覆盖为args
    std::chrono::time_point<std::chrono::system_clock> mLastDataMapClearTime;

    ProcParser mProcParser;
    std::string mHostName;
    std::filesystem::path mHostPathPrefix;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& mCommonEventQueue;

    CounterPtr mPollProcessEventsTotal;
    CounterPtr mLossProcessEventsTotal;
    CounterPtr mProcessCacheMissTotal;
    IntGaugePtr mProcessCacheSize;

    std::atomic_bool mFlushProcessEvent = false;
    std::future<void> mPoller;

    FrequencyManager mFrequencyMgr;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessCacheManagerUnittest;
#endif
};

} // namespace ebpf
} // namespace logtail

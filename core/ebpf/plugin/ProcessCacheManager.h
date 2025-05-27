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

#include "common/ProcParser.h"
#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/EBPFAdapter.h"
#include "ebpf/plugin/ProcessCache.h"
#include "ebpf/plugin/ProcessCloneRetryableEvent.h"
#include "ebpf/plugin/ProcessExecveRetryableEvent.h"
#include "ebpf/plugin/ProcessExitRetryableEvent.h"
#include "ebpf/plugin/ProcessSyncRetryableEvent.h"
#include "ebpf/plugin/RetryableEventCache.h"
#include "ebpf/type/CommonDataEvent.h"
#include "ebpf/util/FrequencyManager.h"
#include "models/LogEvent.h"
#include "monitor/metric_models/MetricTypes.h"

namespace logtail::ebpf {

class ProcessCacheManager {
public:
    ProcessCacheManager() = delete;
    ProcessCacheManager(std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                        const std::string& hostName,
                        const std::string& hostPathPrefix,
                        moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                        CounterPtr pollEventsTotal,
                        CounterPtr lossEventsTotal,
                        CounterPtr processCacheMissTotal,
                        IntGaugePtr processCacheSize,
                        IntGaugePtr processDataMapSize,
                        IntGaugePtr processEventCacheSize);
    ~ProcessCacheManager() = default;

    bool Init();
    void Stop();

    void UpdateRecvEventTotal(uint64_t count = 1);
    void UpdateLossEventTotal(uint64_t count);

    ProcessSyncRetryableEvent* CreateProcessSyncRetryableEvent(const Proc& proc);
    ProcessExecveRetryableEvent* CreateProcessExecveRetryableEvent(msg_execve_event* eventPtr);
    ProcessCloneRetryableEvent* CreateProcessCloneRetryableEvent(msg_clone_event* eventPtr);
    ProcessExitRetryableEvent* CreateProcessExitRetryableEvent(msg_exit* eventPtr);
    void RecordDataEvent(msg_data* eventPtr);
    void MarkProcessEventFlushStatus(bool isFlush) { mFlushProcessEvent = isFlush; }

    bool FinalizeProcessTags(uint32_t pid, uint64_t ktime, LogEvent& logEvent);

    RetryableEventCache& EventCache() { return mRetryableEventCache; }

private:
    int syncAllProc();
    std::vector<std::shared_ptr<Proc>> listRunningProcs();
    int writeProcToBPFMap(const std::shared_ptr<Proc>& proc);

    void pollPerfBuffers();

    std::atomic_bool mInited = false;
    std::atomic_bool mRunFlag = false;
    std::shared_ptr<EBPFAdapter> mEBPFAdapter = nullptr;

    std::filesystem::path mHostPathPrefix;
    ProcParser mProcParser;
    ProcessCache mProcessCache;
    ProcessDataMap mProcessDataMap;
    RetryableEventCache mRetryableEventCache;

    std::string mHostName;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& mCommonEventQueue;

    CounterPtr mPollProcessEventsTotal;
    CounterPtr mLossProcessEventsTotal;
    CounterPtr mProcessCacheMissTotal;
    IntGaugePtr mProcessCacheSize;
    IntGaugePtr mProcessDataMapSize;
    IntGaugePtr mRetryableEventCacheSize;

    std::atomic_bool mFlushProcessEvent = false;
    std::future<void> mPoller;

    FrequencyManager mFrequencyMgr;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessCacheManagerUnittest;
#endif
};

} // namespace logtail::ebpf

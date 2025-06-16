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

#include "ProcessEvent.h"
#include "RetryableEventCache.h"
#include "common/ProcParser.h"
#include "common/queue/blockingconcurrentqueue.h"
#include "coolbpf/security/bpf_process_event_type.h"
#include "ebpf/plugin/ProcessCache.h"
#include "ebpf/plugin/ProcessCacheValue.h"
#include "ebpf/plugin/ProcessDataMap.h"
#include "ebpf/plugin/RetryableEvent.h"

namespace logtail::ebpf {

class ProcessExecveRetryableEvent : public RetryableEvent {
public:
    enum TaskId { kIncrementParentRef, kAttachContainerMeta, kAttachK8sPodMeta, kFlushEvent, kDone };
    explicit ProcessExecveRetryableEvent(
        int retryLimit,
        const msg_execve_event& event,
        const std::string& hostname,
        ProcParser& procParser,
        ProcessDataMap& processDataMap,
        ProcessCache& processCache,
        RetryableEventCache& retryableEventCache,
        moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& commonEventQueue,
        bool flushProcessEvent)
        : RetryableEvent(retryLimit),
          mRawEvent(&event),
          mHostname(hostname),
          mProcParser(procParser),
          mProcessDataMap(processDataMap),
          mProcessCache(processCache),
          mRetryableEventCache(retryableEventCache),
          mCommonEventQueue(commonEventQueue),
          mFlushProcessEvent(flushProcessEvent) {}

    virtual ~ProcessExecveRetryableEvent() = default;

    bool HandleMessage() override;

    bool OnRetry() override;

    void OnDrop() override;

private:
    void fillProcessPlainFields(const msg_execve_event& event, ProcessCacheValue& cacheValue);
    bool fillProcessDataFields(const msg_execve_event& event, ProcessCacheValue& cacheValue);
    bool incrementParentRef();
    bool attachContainerMeta(bool isRetry);
    bool attachK8sPodMeta(bool isRetry);
    bool flushEvent();
    void cleanupCloneParent();

    const msg_execve_event* mRawEvent = nullptr; // only valid in HandleMessage
    const std::string& mHostname;
    ProcParser& mProcParser;
    ProcessDataMap& mProcessDataMap;
    ProcessCache& mProcessCache;
    RetryableEventCache& mRetryableEventCache;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& mCommonEventQueue;

    std::shared_ptr<ProcessCacheValue> mProcessCacheValue;
    std::shared_ptr<ProcessEvent> mProcessEvent;
    struct data_event_id mCleanupKey {};
    bool mFlushProcessEvent;
};

} // namespace logtail::ebpf

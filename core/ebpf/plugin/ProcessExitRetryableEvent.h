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

#include "common/queue/blockingconcurrentqueue.h"
#include "coolbpf/security/bpf_process_event_type.h"
#include "ebpf/plugin/ProcessCache.h"
#include "ebpf/plugin/ProcessCacheValue.h"
#include "ebpf/plugin/RetryableEvent.h"
#include "ebpf/type/CommonDataEvent.h"
#include "ebpf/type/ProcessEvent.h"

namespace logtail::ebpf {

class ProcessExitRetryableEvent : public RetryableEvent {
public:
    enum TaskId { kAttachContainerMeta, kAttachK8sPodMeta, kFlushEvent, kDecrementRef, kDone };
    explicit ProcessExitRetryableEvent(
        int retryLimit,
        const msg_exit& rawEvent,
        ProcessCache& processCache,
        bool flushProcessEvent,
        moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& commonEventQueue)
        : RetryableEvent(retryLimit),
          mRawEvent(&rawEvent),
          mProcessCache(processCache),
          mFlushProcessEvent(flushProcessEvent),
          mCommonEventQueue(commonEventQueue) {}

    virtual ~ProcessExitRetryableEvent() = default;

    bool HandleMessage() override;

    bool OnRetry() override;

    void OnDrop() override;

private:
    bool attachContainerMeta(bool isRetry);
    bool attachK8sPodMeta(bool isRetry);
    bool flushEvent();
    bool decrementRef();

    const msg_exit* mRawEvent = nullptr;
    ProcessCache& mProcessCache;
    bool mFlushProcessEvent = false;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& mCommonEventQueue;

    std::shared_ptr<ProcessCacheValue> mProcessCacheValue;
    std::unique_ptr<msg_exit> mOwnedRawEvent;
    std::shared_ptr<ProcessExitEvent> mProcessExitEvent;
};

} // namespace logtail::ebpf

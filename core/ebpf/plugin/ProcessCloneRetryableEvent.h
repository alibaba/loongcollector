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

#include "common/queue/blockingconcurrentqueue.h"
#include "coolbpf/security/bpf_process_event_type.h"
#include "ebpf/plugin/ProcessCache.h"
#include "ebpf/plugin/ProcessCacheValue.h"
#include "ebpf/plugin/RetryableEvent.h"
#include "ebpf/type/CommonDataEvent.h"
#include "ebpf/type/ProcessEvent.h"

#pragma once

namespace logtail::ebpf {

class ProcessCloneRetryableEvent : public RetryableEvent {
public:
    enum TaskId { kIncrementParentRef, kAttachContainerMeta, kAttachK8sPodMeta, kFlushEvent, kDone };
    ProcessCloneRetryableEvent(int retryLimit,
                               const msg_clone_event& rawEvent,
                               ProcessCache& processCache,
                               std::string& hostname,
                               bool flushProcessEvent,
                               moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& commonEventQueue)
        : RetryableEvent(retryLimit),
          mRawEvent(&rawEvent),
          mProcessCache(processCache),
          mHostname(hostname),
          mFlushProcessEvent(flushProcessEvent),
          mCommonEventQueue(commonEventQueue) {}

    virtual ~ProcessCloneRetryableEvent() = default;

    bool HandleMessage() override;

    bool OnRetry() override;

    void OnDrop() override;

private:
    ProcessCacheValue* cloneProcessCacheValue(const msg_clone_event& event);
    bool incrementParentRef();
    bool attachContainerMeta(bool isRetry);
    bool attachK8sPodMeta(bool isRetry);
    bool flushEvent();
    void cleanupCloneParent();

    const msg_clone_event* mRawEvent = nullptr;
    std::shared_ptr<ProcessCacheValue> mProcessCacheValue;
    ProcessCache& mProcessCache;
    std::string mHostname;
    bool mFlushProcessEvent = false;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& mCommonEventQueue;

    std::unique_ptr<msg_clone_event> mOwnedRawEvent;
    std::shared_ptr<ProcessEvent> mProcessEvent;
};

} // namespace logtail::ebpf

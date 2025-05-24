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

#include "common/ProcParser.h"
#include "coolbpf/security/bpf_process_event_type.h"
#include "ebpf/plugin/ProcessCache.h"
#include "ebpf/plugin/ProcessCacheValue.h"
#include "ebpf/plugin/RetryableEvent.h"
#include "ebpf/type/ProcessEvent.h"

#pragma once

namespace logtail::ebpf {

class ProcessSyncRetryableEvent : public RetryableEvent {
public:
    enum TaskId { kIncrementParentRef, kAttachContainerMeta, kAttachK8sPodMeta, kDone };
    ProcessSyncRetryableEvent(int retryLimit,
                              const Proc& rawEvent,
                              ProcessCache& processCache,
                              const std::string& hostname,
                              ProcParser& procParser)
        : RetryableEvent(retryLimit),
          mRawEvent(rawEvent),
          mProcessCache(processCache),
          mHostname(hostname),
          mProcParser(procParser) {}

    virtual ~ProcessSyncRetryableEvent() = default;

    bool HandleMessage() override;

    bool OnRetry() override;

    void OnDrop() override;

private:
    std::shared_ptr<ProcessCacheValue> procToProcessCacheValue(const Proc& proc);
    bool incrementParentRef();
    bool attachContainerMeta(bool isRetry);
    bool attachK8sPodMeta(bool isRetry);

    const Proc& mRawEvent;
    std::shared_ptr<ProcessCacheValue> mProcessCacheValue;
    ProcessCache& mProcessCache;
    const std::string& mHostname;
    ProcParser& mProcParser;

    std::unique_ptr<msg_clone_event> mOwnedRawEvent;
    std::shared_ptr<ProcessEvent> mProcessEvent;
};

} // namespace logtail::ebpf

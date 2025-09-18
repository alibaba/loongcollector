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

#include "ebpf/plugin/ProcessCache.h"
#include "ebpf/plugin/ProcessCacheValue.h"
#include "ebpf/plugin/RetryableEvent.h"

#pragma once

namespace logtail::ebpf {

class ProcessCleanupRetryableEvent : public RetryableEvent {
public:
    enum TaskId { kDecrementRef, kDone };
    explicit ProcessCleanupRetryableEvent(int retryLimit, const data_event_id& rawEvent, ProcessCache& processCache)
        : RetryableEvent(retryLimit), mKey(rawEvent), mProcessCache(processCache) {}

    virtual ~ProcessCleanupRetryableEvent() = default;

    bool HandleMessage() override;

    bool OnRetry() override;

    void OnDrop() override;

private:
    bool decrementRef();

    std::shared_ptr<ProcessCacheValue> mProcessCacheValue;
    data_event_id mKey;
    ProcessCache& mProcessCache;
};

} // namespace logtail::ebpf

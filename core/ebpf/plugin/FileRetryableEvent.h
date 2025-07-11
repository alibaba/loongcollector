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

#include "ebpf/plugin/ProcessCache.h"
#include "FileEvent.h"
#include "ebpf/plugin/RetryableEvent.h"
#include "common/queue/blockingconcurrentqueue.h"
#include "coolbpf/security/type.h"

namespace logtail::ebpf {

class FileRetryableEvent : public RetryableEvent {
public:
    enum TaskId { kFindProcess, kFlushEvent, kDone };
    explicit FileRetryableEvent(
        int retryLimit, 
        const file_data_t& event,
        ProcessCache& processCache,
        moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& eventQueue,
        bool flushFileEvent)
        : RetryableEvent(retryLimit),
          mRawEvent(&event),
          mProcessCache(processCache), 
          mEventQueue(eventQueue), 
          mFlushFileEvent(flushFileEvent) {}
    
    virtual ~FileRetryableEvent() = default;

    bool HandleMessage() override;
    bool OnRetry() override;
    void OnDrop() override;
    [[nodiscard]] bool CanRetry() const override;

private:
    bool findProcess();
    bool flushEvent();

    const file_data_t* mRawEvent = nullptr;
    ProcessCache& mProcessCache;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& mEventQueue;
    std::shared_ptr<FileEvent> mFileEvent;
    bool mFlushFileEvent;
};

} // namespace logtail::ebpf
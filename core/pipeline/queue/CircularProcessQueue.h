/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstdint>
#include <deque>
#include <memory>

#include "pipeline/queue/ProcessQueueInterface.h"
#include "pipeline/queue/QueueInterface.h"

namespace logtail {

// not thread-safe, should be protected explicitly by queue manager
class CircularProcessQueue : virtual public QueueInterface<std::unique_ptr<ProcessQueueItem>>,
                             public ProcessQueueInterface {
public:
    CircularProcessQueue(size_t cap, int64_t key, uint32_t priority, const PipelineContext& ctx);

    bool Push(std::unique_ptr<ProcessQueueItem>&& item) override;
    bool Pop(std::unique_ptr<ProcessQueueItem>& item) override;
    void SetPipelineForItems(const std::string& name) const override;

    void Reset(size_t cap);

private:
    size_t Size() const override { return mEventCnt; }

    std::deque<std::unique_ptr<ProcessQueueItem>> mQueue;
    size_t mEventCnt = 0;

    CounterPtr mDroppedEventsCnt;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class CircularProcessQueueUnittest;
    friend class ProcessQueueManagerUnittest;
    friend class PipelineUnittest;
#endif
};

} // namespace logtail

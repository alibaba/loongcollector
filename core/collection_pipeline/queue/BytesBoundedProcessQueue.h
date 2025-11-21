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
#include <vector>

#include "collection_pipeline/queue/BoundedProcessQueue.h"
#include "collection_pipeline/queue/ProcessQueueInterface.h"
#include "common/FeedbackInterface.h"

namespace logtail {

// not thread-safe, should be protected explicitly by queue manager
class BytesBoundedProcessQueue : public BoundedProcessQueue {
public:
    BytesBoundedProcessQueue(size_t cap,
                             size_t lowBytes,
                             size_t highBytes,
                             int64_t key,
                             uint32_t priority,
                             const CollectionPipelineContext& ctx)
        : QueueInterface(key, cap, ctx), BoundedProcessQueue(cap, lowBytes, highBytes, key, priority, ctx) {}

private:
    size_t Size() const override { return mCurrentBytesSize; }
    void AddSize(ProcessQueueItem* item) override { mCurrentBytesSize += item->mEventGroup.DataSize(); }
    void SubSize(ProcessQueueItem* item) override { mCurrentBytesSize -= item->mEventGroup.DataSize(); }

    size_t mCurrentBytesSize = 0;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class BytesBoundedProcessQueueUnittest;
    friend class ProcessQueueManagerUnittest;
    friend class ExactlyOnceQueueManagerUnittest;
    friend class PipelineUnittest;
    friend class PipelineUpdateUnittest;
#endif
};

} // namespace logtail

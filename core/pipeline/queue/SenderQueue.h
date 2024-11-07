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

#include <memory>
#include <vector>

#include "pipeline/queue/BoundedSenderQueueInterface.h"
#include "pipeline/queue/QueueKey.h"
#include "pipeline/queue/SenderQueueItem.h"

namespace logtail {

class Flusher;

// not thread-safe, should be protected explicitly by queue manager
class SenderQueue : public BoundedSenderQueueInterface {
public:
    SenderQueue(
        size_t cap, size_t low, size_t high, QueueKey key, const std::string& flusherId, const PipelineContext& ctx);

    bool Push(std::unique_ptr<SenderQueueItem>&& item) override;
    bool Remove(SenderQueueItem* item) override;
    void GetAvailableItems(std::vector<SenderQueueItem*>& items, int32_t limit) override;
    void SetPipelineForItems(const std::shared_ptr<Pipeline>& p) const override;

private:
    size_t Size() const override { return mSize; }
    void PushFromExtraBuffer(std::unique_ptr<SenderQueueItem>&& item) override;

    std::vector<std::unique_ptr<SenderQueueItem>> mQueue;
    size_t mWrite = 0;
    size_t mRead = 0;
    size_t mSize = 0;

    CounterPtr mFetchTimesCnt;
    CounterPtr mValidFetchTimesCnt;
    CounterPtr mFetchedItemsCnt;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SenderQueueUnittest;
    friend class SenderQueueManagerUnittest;
    friend class FlusherUnittest;
#endif
};

} // namespace logtail

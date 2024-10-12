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

#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "common/FeedbackInterface.h"
#include "pipeline/limiter/ConcurrencyLimiter.h"
#include "pipeline/limiter/RateLimiter.h"
#include "pipeline/queue/QueueParam.h"
#include "pipeline/queue/SenderQueue.h"
#include "pipeline/queue/SenderQueueItem.h"

namespace logtail {

class Flusher;

class SenderQueueManager : public FeedbackInterface {
public:
    SenderQueueManager(const SenderQueueManager&) = delete;
    SenderQueueManager& operator=(const SenderQueueManager&) = delete;

    static SenderQueueManager* GetInstance() {
        static SenderQueueManager instance;
        return &instance;
    }

    void Feedback(QueueKey key) override { Trigger(); }

    bool CreateQueue(QueueKey key,
                     const std::string& flusherId,
                     const PipelineContext& ctx,
                     std::unordered_map<std::string, std::shared_ptr<ConcurrencyLimiter>>&& concurrencyLimitersMap
                     = std::unordered_map<std::string, std::shared_ptr<ConcurrencyLimiter>>(),
                     uint32_t maxRate = 0);
    SenderQueue* GetQueue(QueueKey key);
    bool DeleteQueue(QueueKey key);
    bool ReuseQueue(QueueKey key);
    // 0: success, 1: queue is full, 2: queue not found
    int PushQueue(QueueKey key, std::unique_ptr<SenderQueueItem>&& item);
    void GetAvailableItems(std::vector<SenderQueueItem*>& items, int32_t itemsCntLimit);
    bool RemoveItem(QueueKey key, SenderQueueItem* item);
    void DecreaseConcurrencyLimiterInSendingCnt(QueueKey key);
    bool IsAllQueueEmpty() const;
    void ClearUnusedQueues();
    void NotifyPipelineStop(QueueKey key, const std::string& configName);
    void SetPipelineForItems(QueueKey key, const std::shared_ptr<Pipeline>& p);

    bool Wait(uint64_t ms);
    void Trigger();

    // only used for go pipeline before flushing data to C++ flusher
    bool IsValidToPush(QueueKey key) const;

#ifdef APSARA_UNIT_TEST_MAIN
    void Clear();
    bool IsQueueMarkedDeleted(QueueKey key);
#endif

private:
    SenderQueueManager();
    ~SenderQueueManager() = default;

    BoundedQueueParam mQueueParam;

    mutable std::mutex mQueueMux;
    std::unordered_map<QueueKey, SenderQueue> mQueues;

    mutable std::mutex mGCMux;
    std::unordered_map<QueueKey, time_t> mQueueDeletionTimeMap;

    // RateLimiter mRateLimiter;

    mutable std::mutex mStateMux;
    mutable std::condition_variable mCond;
    bool mValidToPop = false;
    size_t mSenderQueueBeginIndex = 0;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SenderQueueManagerUnittest;
    friend class FlusherRunnerUnittest;
#endif
};

} // namespace logtail

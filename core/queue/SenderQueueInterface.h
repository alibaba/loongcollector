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
#include <optional>
#include <queue>
#include <vector>

#include "common/FeedbackInterface.h"
#include "queue/FeedbackQueue.h"
#include "queue/FeedbackQueueKey.h"
#include "queue/SenderQueueItem.h"
#include "sender/ConcurrencyLimiter.h"
#include "sender/RateLimiter.h"

namespace logtail {

class Flusher;

// not thread-safe, should be protected explicitly by queue manager
class SenderQueueInterface : public FeedbackQueue<std::unique_ptr<SenderQueueItem>> {
public:
    static void SetFeedback(FeedbackInterface* feedback);

    SenderQueueInterface(size_t cap, size_t low, size_t high, QueueKey key)
        : FeedbackQueue<std::unique_ptr<SenderQueueItem>>(key, cap, low, high) {}

    bool Pop(std::unique_ptr<SenderQueueItem>& item) override { return false; }

    virtual bool Remove(SenderQueueItem* item) = 0;
    virtual void GetAllAvailableItems(std::vector<SenderQueueItem*>& items, bool withLimits = true) = 0;

    void SetRateLimiter(uint32_t maxRate);
    void SetConcurrencyLimiters(std::vector<std::shared_ptr<ConcurrencyLimiter>>&& limiters);

#ifdef APSARA_UNIT_TEST_MAIN
    std::optional<RateLimiter>& GetRateLimiter() { return mRateLimiter; }
    std::vector<std::shared_ptr<ConcurrencyLimiter>>& GetConcurrencyLimiters() { return mConcurrencyLimiters; }
#endif

protected:
    static FeedbackInterface* sFeedback;

    void GiveFeedback() const;
    void Reset(size_t cap, size_t low, size_t high);

    std::optional<RateLimiter> mRateLimiter;
    std::vector<std::shared_ptr<ConcurrencyLimiter>> mConcurrencyLimiters;

    std::queue<std::unique_ptr<SenderQueueItem>> mExtraBuffer;

private:
    bool IsDownStreamQueuesValidToPush() const override { return true; }
};

} // namespace logtail

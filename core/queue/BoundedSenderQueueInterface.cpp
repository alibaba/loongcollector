// Copyright 2024 iLogtail Authors
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

#include "queue/BoundedSenderQueueInterface.h"


using namespace std;

namespace logtail {

FeedbackInterface* BoundedSenderQueueInterface::sFeedback = nullptr;

void BoundedSenderQueueInterface::SetFeedback(FeedbackInterface* feedback) {
    if (feedback == nullptr) {
        // should not happen
        return;
    }
    sFeedback = feedback;
}

void BoundedSenderQueueInterface::SetRateLimiter(uint32_t maxRate) {
    if (maxRate > 0) {
        mRateLimiter = RateLimiter(maxRate);
    }
}

void BoundedSenderQueueInterface::SetConcurrencyLimiters(std::vector<std::shared_ptr<ConcurrencyLimiter>>&& limiters) {
    mConcurrencyLimiters.clear();
    for (auto& item : limiters) {
        if (item == nullptr) {
            // should not happen
            continue;
        }
        mConcurrencyLimiters.emplace_back(item);
    }
}

void BoundedSenderQueueInterface::GiveFeedback() const {
    // 0 is just a placeholder
    sFeedback->Feedback(0);
}

void BoundedSenderQueueInterface::Reset(size_t cap, size_t low, size_t high) {
    queue<unique_ptr<SenderQueueItem>>().swap(mExtraBuffer);
    mRateLimiter.reset();
    mConcurrencyLimiters.clear();
    BoundedQueueInterface::Reset(low, high);
    QueueInterface::Reset(cap);
}

} // namespace logtail

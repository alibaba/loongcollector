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

#include "pipeline/queue/BoundedSenderQueueInterface.h"


using namespace std;

namespace logtail {

FeedbackInterface* BoundedSenderQueueInterface::sFeedback = nullptr;

BoundedSenderQueueInterface::BoundedSenderQueueInterface(
    size_t cap, size_t low, size_t high, QueueKey key, const string& flusherId, const PipelineContext& ctx)
    : QueueInterface(key, cap, ctx), BoundedQueueInterface<std::unique_ptr<SenderQueueItem>>(key, cap, low, high, ctx) {
    mMetricsRecordRef.AddLabels({{METRIC_LABEL_KEY_COMPONENT_NAME, "sender_queue"}});
    mMetricsRecordRef.AddLabels({{METRIC_LABEL_KEY_FLUSHER_NODE_ID, flusherId}});
    mExtraBufferSize = mMetricsRecordRef.CreateIntGauge("extra_buffer_size");
    mExtraBufferDataSizeBytes = mMetricsRecordRef.CreateIntGauge("extra_buffer_data_size_bytes");
}

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

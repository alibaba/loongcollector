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
    mMetricsRecordRef.AddLabels({{METRIC_LABEL_KEY_COMPONENT_NAME, METRIC_LABEL_VALUE_COMPONENT_NAME_SENDER_QUEUE}});
    mMetricsRecordRef.AddLabels({{METRIC_LABEL_KEY_FLUSHER_PLUGIN_ID, flusherId}});
    mExtraBufferSize = mMetricsRecordRef.CreateIntGauge(METRIC_COMPONENT_QUEUE_EXTRA_BUFFER_SIZE);
    mFetchRejectedByRateLimiterTimesCnt = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_RATE_LIMITER_TIMES_TOTAL);
    mExtraBufferDataSizeBytes = mMetricsRecordRef.CreateIntGauge(METRIC_COMPONENT_QUEUE_EXTRA_BUFFER_SIZE_BYTES);
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

void BoundedSenderQueueInterface::SetConcurrencyLimiters(std::unordered_map<std::string, std::shared_ptr<ConcurrencyLimiter>>&& concurrencyLimitersMap) {
    mConcurrencyLimiters.clear();
    for (const auto& item : concurrencyLimitersMap) {
        if (item.second == nullptr) {
            // should not happen
            continue;
        }
        mConcurrencyLimiters.emplace_back(item.second, mMetricsRecordRef.CreateCounter(ConcurrencyLimiter::GetLimiterMetricName(item.first)));
    }
}

void BoundedSenderQueueInterface::OnSendingSuccess() {
    for (auto& limiter : mConcurrencyLimiters) {
        if (limiter.first != nullptr) {
            limiter.first->OnSuccess();
        }
    }
}

void BoundedSenderQueueInterface::DecreaseSendingCnt() {
    for (auto& limiter : mConcurrencyLimiters) {
        if (limiter.first != nullptr) {
            limiter.first->OnSendDone();
        }
    }
}

void BoundedSenderQueueInterface::GiveFeedback() const {
    // 0 is just a placeholder
    sFeedback->Feedback(0);
}

void BoundedSenderQueueInterface::Reset(size_t cap, size_t low, size_t high) {
    deque<unique_ptr<SenderQueueItem>>().swap(mExtraBuffer);
    mRateLimiter.reset();
    mConcurrencyLimiters.clear();
    BoundedQueueInterface::Reset(low, high);
    QueueInterface::Reset(cap);
}

} // namespace logtail

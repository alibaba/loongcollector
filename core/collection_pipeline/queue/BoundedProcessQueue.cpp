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

#include "collection_pipeline/queue/BoundedProcessQueue.h"

#include "collection_pipeline/CollectionPipelineManager.h"

using namespace std;

namespace logtail {

BoundedProcessQueue::BoundedProcessQueue(
    size_t cap, size_t low, size_t high, int64_t key, uint32_t priority, const CollectionPipelineContext& ctx)
    : QueueInterface(key, cap, ctx),
      BoundedQueueInterface(key, cap, low, high, ctx),
      ProcessQueueInterface(key, cap, priority, ctx) {
    if (ctx.IsExactlyOnceEnabled()) {
        mMetricsRecordRef.AddLabels({{METRIC_LABEL_KEY_EXACTLY_ONCE_ENABLED, "true"}});
    }
    WriteMetrics::GetInstance()->CommitMetricsRecordRef(mMetricsRecordRef);
}

bool BoundedProcessQueue::Push(unique_ptr<ProcessQueueItem>&& item) {
    if (!IsValidToPush()) {
        return false;
    }
    item->mEnqueTime = chrono::system_clock::now();
    auto size = item->mEventGroup.DataSize();
    mQueue.push_back(std::move(item));
    ChangeStateIfNeededAfterPush();

    ADD_COUNTER(mInItemsTotal, 1);
    ADD_COUNTER(mInItemDataSizeBytes, size);
    SET_GAUGE(mQueueSizeTotal, Size());
    ADD_COUNTER(mQueueDataSizeByte, size);
    SET_GAUGE(mValidToPushFlag, IsValidToPush());
    return true;
}

bool BoundedProcessQueue::Pop(unique_ptr<ProcessQueueItem>& item) {
    ADD_COUNTER(mFetchTimesCnt, 1);
    if (Empty()) {
        return false;
    }
    ADD_COUNTER(mValidFetchTimesCnt, 1);
    if (!IsValidToPop()) {
        return false;
    }
    item = std::move(mQueue.front());
    mQueue.pop_front();
    item->AddPipelineInProcessCnt(GetConfigName());
    if (ChangeStateIfNeededAfterPop()) {
        GiveFeedback();
    }

    ADD_COUNTER(mOutItemsTotal, 1);
    ADD_COUNTER(mTotalDelayMs, chrono::system_clock::now() - item->mEnqueTime);
    SET_GAUGE(mQueueSizeTotal, Size());
    SUB_GAUGE(mQueueDataSizeByte, item->mEventGroup.DataSize());
    SET_GAUGE(mValidToPushFlag, IsValidToPush());
    return true;
}

void BoundedProcessQueue::SetPipelineForItems(const std::shared_ptr<CollectionPipeline>& p) const {
    for (auto& item : mQueue) {
        if (!item->mPipeline) {
            item->mPipeline = p;
        }
    }
}

void BoundedProcessQueue::SetUpStreamFeedbacks(vector<FeedbackInterface*>&& feedbacks) {
    mUpStreamFeedbacks.clear();
    for (auto& item : feedbacks) {
        if (item == nullptr) {
            // should not happen
            continue;
        }
        mUpStreamFeedbacks.emplace_back(item);
    }
}

void BoundedProcessQueue::GiveFeedback() const {
    for (auto& item : mUpStreamFeedbacks) {
        item->Feedback(mKey);
    }
}

} // namespace logtail

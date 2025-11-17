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

#include "collection_pipeline/queue/BytesBoundedProcessQueue.h"

#include "collection_pipeline/CollectionPipelineManager.h"

using namespace std;

namespace logtail {

BytesBoundedProcessQueue::BytesBoundedProcessQueue(
    size_t maxBytes, size_t lowBytes, size_t highBytes, int64_t key, uint32_t priority, const CollectionPipelineContext& ctx)
    : QueueInterface(key, maxBytes, ctx),
      BytesBoundedQueueInterface(key, maxBytes, lowBytes, highBytes, ctx),
      ProcessQueueInterface(key, maxBytes, priority, ctx) {
    if (ctx.IsExactlyOnceEnabled()) {
        mMetricsRecordRef.AddLabels({{METRIC_LABEL_KEY_EXACTLY_ONCE_ENABLED, "true"}});
    }
    WriteMetrics::GetInstance()->CommitMetricsRecordRef(mMetricsRecordRef);
}

bool BytesBoundedProcessQueue::Push(unique_ptr<ProcessQueueItem>&& item) {
    if (!IsValidToPush()) {
        return false;
    }
    item->mEnqueTime = chrono::system_clock::now();
    size_t itemBytes = item->mEventGroup.DataSize();
    
    // Update bytes tracking before push
    mCurrentBytesSize += itemBytes;
    mQueue.emplace_back(std::move(item));
    
    // Check if reached high watermark
    ChangeStateIfNeededAfterPush(itemBytes);

    ADD_COUNTER(mInItemsTotal, 1);
    ADD_COUNTER(mInItemDataSizeBytes, itemBytes);
    SET_GAUGE(mQueueSizeTotal, Size());
    SET_GAUGE(mQueueDataSizeByte, mCurrentBytesSize);
    SET_GAUGE(mValidToPushFlag, IsValidToPush());
    return true;
}

bool BytesBoundedProcessQueue::Pop(unique_ptr<ProcessQueueItem>& item) {
    ADD_COUNTER(mFetchTimesCnt, 1);
    if (Empty()) {
        return false;
    }
    ADD_COUNTER(mValidFetchTimesCnt, 1);
    if (!IsValidToPop()) {
        return false;
    }
    
    item = std::move(mQueue.front());
    size_t itemBytes = item->mEventGroup.DataSize();
    mQueue.pop_front();
    mCurrentBytesSize -= itemBytes;
    
    item->AddPipelineInProcessCnt(GetConfigName());
    
    // Check if reached low watermark and can resume accepting data
    if (ChangeStateIfNeededAfterPop(itemBytes)) {
        GiveFeedback();
    }

    ADD_COUNTER(mOutItemsTotal, 1);
    ADD_COUNTER(mTotalDelayMs, chrono::system_clock::now() - item->mEnqueTime);
    SET_GAUGE(mQueueSizeTotal, Size());
    SET_GAUGE(mQueueDataSizeByte, mCurrentBytesSize);
    SET_GAUGE(mValidToPushFlag, IsValidToPush());
    return true;
}

void BytesBoundedProcessQueue::SetUpStreamFeedbacks(vector<FeedbackInterface*>&& feedbacks) {
    mUpStreamFeedbacks.clear();
    for (auto& item : feedbacks) {
        if (item == nullptr) {
            // should not happen
            continue;
        }
        mUpStreamFeedbacks.emplace_back(item);
    }
}

void BytesBoundedProcessQueue::GiveFeedback() const {
    for (auto& item : mUpStreamFeedbacks) {
        item->Feedback(mKey);
    }
}

} // namespace logtail


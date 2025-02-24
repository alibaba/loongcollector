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

#include "collection_pipeline/queue/CircularProcessQueue.h"

#include "collection_pipeline/CollectionPipelineManager.h"
#include "collection_pipeline/queue/QueueKeyManager.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail {

CircularProcessQueue::CircularProcessQueue(size_t cap,
                                           int64_t key,
                                           uint32_t priority,
                                           const CollectionPipelineContext& ctx)
    : QueueInterface<std::unique_ptr<ProcessQueueItem>>(key, cap, ctx), ProcessQueueInterface(key, cap, priority, ctx) {
    mMetricsRecordRef.AddLabels({{METRIC_LABEL_KEY_QUEUE_TYPE, "circular"}});
    mDiscardedEventsTotal = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_QUEUE_DISCARDED_EVENTS_TOTAL);
    WriteMetrics::GetInstance()->CommitMetricsRecordRef(mMetricsRecordRef);
}

bool CircularProcessQueue::Push(unique_ptr<ProcessQueueItem>&& item) {
    size_t newCnt = item->mEventGroup.GetEvents().size();
    while (!mQueue.empty() && mEventCnt + newCnt > mCapacity) {
        auto cnt = mQueue.front()->mEventGroup.GetEvents().size();
        auto size = mQueue.front()->mEventGroup.DataSize();
        mEventCnt -= cnt;
        mQueue.pop_front();
        SET_GAUGE(mQueueSizeTotal, Size());
        SUB_GAUGE(mQueueDataSizeByte, size);
        ADD_COUNTER(mDiscardedEventsTotal, cnt);
    }
    if (mEventCnt + newCnt > mCapacity) {
        return false;
    }
    item->mEnqueTime = chrono::system_clock::now();
    auto size = item->mEventGroup.DataSize();
    mQueue.push_back(std::move(item));
    mEventCnt += newCnt;

    ADD_COUNTER(mInItemsTotal, 1);
    ADD_COUNTER(mInItemDataSizeBytes, size);
    SET_GAUGE(mQueueSizeTotal, Size());
    ADD_GAUGE(mQueueDataSizeByte, size);
    return true;
}

bool CircularProcessQueue::Pop(unique_ptr<ProcessQueueItem>& item) {
    ADD_COUNTER(mFetchTimesCnt, 1);
    if (Empty()) {
        return false;
    }
    ADD_COUNTER(mValidFetchTimesCnt, 1);
    if (!IsValidToPop()) {
        return false;
    }
    item = std::move(mQueue.front());
    item->AddPipelineInProcessCnt(GetConfigName());
    mQueue.pop_front();
    mEventCnt -= item->mEventGroup.GetEvents().size();

    ADD_COUNTER(mOutItemsTotal, 1);
    ADD_COUNTER(mTotalDelayMs, std::chrono::system_clock::now() - item->mEnqueTime);
    SET_GAUGE(mQueueSizeTotal, Size());
    SUB_GAUGE(mQueueDataSizeByte, item->mEventGroup.DataSize());
    return true;
}

void CircularProcessQueue::SetPipelineForItems(const std::shared_ptr<CollectionPipeline>& p) const {
    for (auto& item : mQueue) {
        if (!item->mPipeline) {
            item->mPipeline = p;
        }
    }
}

void CircularProcessQueue::Reset(size_t cap) {
    // it seems more reasonable to retain extra items and process them immediately, however this contray to current
    // framework design so we simply discard extra items, considering that it is a rare case to change capacity
    uint32_t cnt = 0;
    while (!mQueue.empty() && mEventCnt > cap) {
        mEventCnt -= mQueue.front()->mEventGroup.GetEvents().size();
        mQueue.pop_front();
        ++cnt;
    }
    if (cnt > 0) {
        LOG_WARNING(sLogger,
                    ("new circular process queue capacity is smaller than old queue size",
                     "discard old data")("discard cnt", cnt)("config", QueueKeyManager::GetInstance()->GetName(mKey)));
    }
    ProcessQueueInterface::Reset();
    QueueInterface::Reset(cap);
}

} // namespace logtail

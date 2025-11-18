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

#include "collection_pipeline/queue/QueueInterface.h"

namespace logtail {

template <typename T>
class BytesBoundedQueueInterface : virtual public QueueInterface<T> {
public:
    BytesBoundedQueueInterface(
        QueueKey key, size_t maxBytes, size_t lowBytes, size_t highBytes, const CollectionPipelineContext& ctx)
        : QueueInterface<T>(key, maxBytes, ctx), mLowBytesWatermark(lowBytes), mHighBytesWatermark(highBytes) {
        this->mMetricsRecordRef.AddLabels({{METRIC_LABEL_KEY_QUEUE_TYPE, "bytes_bounded"}});
        mValidToPushFlag = this->mMetricsRecordRef.CreateIntGauge(METRIC_COMPONENT_QUEUE_VALID_TO_PUSH_FLAG);
        SET_GAUGE(mValidToPushFlag, mValidToPush);
    }
    virtual ~BytesBoundedQueueInterface() = default;

    BytesBoundedQueueInterface(const BytesBoundedQueueInterface& que) = delete;
    BytesBoundedQueueInterface& operator=(const BytesBoundedQueueInterface&) = delete;

    bool IsValidToPush() const { return mValidToPush; }

protected:
    bool Full() const { return mCurrentBytesSize >= this->mCapacity; }

    bool ChangeStateIfNeededAfterPush(size_t /*addedBytes*/) {
        if (mCurrentBytesSize >= mHighBytesWatermark) {
            mValidToPush = false;
            return true;
        }
        return false;
    }

    bool ChangeStateIfNeededAfterPop(size_t /*removedBytes*/) {
        if (!mValidToPush && mCurrentBytesSize <= mLowBytesWatermark) {
            mValidToPush = true;
            return true;
        }
        return false;
    }

    void Reset(size_t lowBytes, size_t highBytes) {
        mLowBytesWatermark = lowBytes;
        mHighBytesWatermark = highBytes;
        mValidToPush = true;
    }

    size_t mCurrentBytesSize = 0;
    IntGaugePtr mValidToPushFlag;

private:
    virtual void GiveFeedback() const = 0;
    virtual size_t Size() const = 0;

    size_t mLowBytesWatermark = 0;
    size_t mHighBytesWatermark = 0;

    bool mValidToPush = true;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class BytesBoundedProcessQueueUnittest;
    friend class ProcessQueueManagerUnittest;
    friend class PipelineUpdateUnittest;
#endif
};

} // namespace logtail

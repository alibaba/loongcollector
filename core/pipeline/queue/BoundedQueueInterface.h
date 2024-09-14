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

#include "pipeline/queue/QueueInterface.h"

namespace logtail {

template <typename T>
class BoundedQueueInterface : virtual public QueueInterface<T> {
public:
    BoundedQueueInterface(QueueKey key, size_t cap, size_t low, size_t high, const PipelineContext& ctx)
        : QueueInterface<T>(key, cap, ctx), mLowWatermark(low), mHighWatermark(high) {
        this->mMetricsRecordRef.AddLabels({{METRIC_LABEL_KEY_QUEUE_TYPE, "bounded"}});
        mValidToPushFlag = this->mMetricsRecordRef.CreateIntGauge("valid_to_push");
    }
    virtual ~BoundedQueueInterface() = default;

    BoundedQueueInterface(const BoundedQueueInterface& que) = delete;
    BoundedQueueInterface& operator=(const BoundedQueueInterface&) = delete;

    bool IsValidToPush() const { return mValidToPush; }

protected:
    bool Full() const { return this->Size() == this->mCapacity; }

    bool ChangeStateIfNeededAfterPush() {
        if (this->Size() == mHighWatermark) {
            mValidToPush = false;
            return true;
        }
        return false;
    }

    bool ChangeStateIfNeededAfterPop() {
        if (!mValidToPush && this->Size() == mLowWatermark) {
            mValidToPush = true;
            return true;
        }
        return false;
    }

    void Reset(size_t low, size_t high) {
        mLowWatermark = low;
        mHighWatermark = high;
        mValidToPush = true;
    }

    IntGaugePtr mValidToPushFlag;

private:
    virtual void GiveFeedback() const = 0;
    virtual size_t Size() const = 0;

    size_t mLowWatermark = 0;
    size_t mHighWatermark = 0;

    bool mValidToPush = true;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class BoundedProcessQueueUnittest;
    friend class CircularProcessQueueUnittest;
    friend class ExactlyOnceSenderQueueUnittest;
    friend class ProcessQueueManagerUnittest;
    friend class ExactlyOnceQueueManagerUnittest;
    friend class SenderQueueManagerUnittest;
#endif
};

} // namespace logtail

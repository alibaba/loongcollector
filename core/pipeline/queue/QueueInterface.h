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

#include "monitor/LogtailMetric.h"
#include "monitor/MetricConstants.h"
#include "pipeline/PipelineContext.h"
#include "pipeline/queue/QueueKey.h"

namespace logtail {

template <typename T>
class QueueInterface {
public:
    QueueInterface(QueueKey key, size_t cap, const PipelineContext& ctx) : mKey(key), mCapacity(cap) {
        WriteMetrics::GetInstance()->CreateMetricsRecordRef(mMetricsRecordRef,
                                                            {
                                                                {METRIC_LABEL_PROJECT, ctx.GetProjectName()},
                                                                {METRIC_LABEL_CONFIG_NAME, ctx.GetConfigName()},
                                                            });

        mInItemsCnt = mMetricsRecordRef.CreateCounter(METRIC_IN_ITEMS_CNT);
        mInItemDataSizeBytes = mMetricsRecordRef.CreateCounter(METRIC_IN_ITEM_SIZE_BYTES);
        mOutItemsCnt = mMetricsRecordRef.CreateCounter(METRIC_OUT_ITEMS_CNT);
        mTotalDelayMs = mMetricsRecordRef.CreateCounter(METRIC_TOTAL_DELAY_MS);
        mQueueSize = mMetricsRecordRef.CreateIntGauge("queue_size");
        mQueueDataSizeByte = mMetricsRecordRef.CreateIntGauge("queue_data_size_bytes");
    }
    virtual ~QueueInterface() = default;

    QueueInterface(const QueueInterface& que) = delete;
    QueueInterface& operator=(const QueueInterface&) = delete;

    virtual bool Push(T&& item) = 0;
    virtual bool Pop(T& item) = 0;

    bool Empty() const { return Size() == 0; }

    QueueKey GetKey() const { return mKey; }

    void Reset(size_t cap) { mCapacity = cap; }

protected:
    const QueueKey mKey;
    size_t mCapacity = 0;

    mutable MetricsRecordRef mMetricsRecordRef;
    CounterPtr mInItemsCnt;
    CounterPtr mInItemDataSizeBytes;
    CounterPtr mOutItemsCnt;
    CounterPtr mTotalDelayMs;
    IntGaugePtr mQueueSize;
    IntGaugePtr mQueueDataSizeByte;

private:
    virtual size_t Size() const = 0;
};

} // namespace logtail
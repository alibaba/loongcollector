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

#include <chrono>
#include <string>

#include "models/PipelineEventPtr.h"
#include "monitor/MetricConstants.h"
#include "pipeline/batch/BatchedEvents.h"
#include "pipeline/plugin/interface/Flusher.h"

namespace logtail {

inline size_t GetInputSize(const PipelineEventPtr& p) {
    return p->DataSize();
}

inline size_t GetInputSize(const BatchedEvents& p) {
    return p.mSizeBytes;
}

inline size_t GetInputSize(const BatchedEventsList& p) {
    size_t size = 0;
    for (const auto& e : p) {
        size += GetInputSize(e);
    }
    return size;
}

// T: PipelineEventPtr, BatchedEvents, BatchedEventsList
template <typename T>
class Serializer {
public:
    Serializer(Flusher* f) : mFlusher(f) {
        WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
            mMetricsRecordRef,
            {{METRIC_LABEL_PROJECT, f->GetContext().GetProjectName()},
             {METRIC_LABEL_CONFIG_NAME, f->GetContext().GetConfigName()},
             {METRIC_LABEL_KEY_COMPONENT_NAME, "serializer"},
             {METRIC_LABEL_KEY_FLUSHER_NODE_ID, f->GetNodeID()}});
        mInItemsCnt = mMetricsRecordRef.CreateCounter(METRIC_IN_ITEMS_CNT);
        mInItemSizeBytes = mMetricsRecordRef.CreateCounter(METRIC_IN_ITEM_SIZE_BYTES);
        mOutItemsCnt = mMetricsRecordRef.CreateCounter(METRIC_OUT_ITEMS_CNT);
        mOutItemSizeBytes = mMetricsRecordRef.CreateCounter(METRIC_OUT_ITEM_SIZE_BYTES);
        mDiscardedItemsCnt = mMetricsRecordRef.CreateCounter("discarded_items_cnt");
        mDiscardedItemSizeBytes = mMetricsRecordRef.CreateCounter("discarded_item_data_size_bytes");
        mTotalDelayMs = mMetricsRecordRef.CreateCounter(METRIC_TOTAL_DELAY_MS);
    }
    virtual ~Serializer() = default;

    bool DoSerialize(T&& p, std::string& output, std::string& errorMsg) {
        auto inputSize = GetInputSize(p);
        mInItemsCnt->Add(1);
        mInItemSizeBytes->Add(inputSize);

        auto before = std::chrono::system_clock::now();
        auto res = Serialize(std::move(p), output, errorMsg);
        mTotalDelayMs->Add(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - before).count());

        if (res) {
            mOutItemsCnt->Add(1);
            mOutItemSizeBytes->Add(output.size());
        } else {
            mDiscardedItemsCnt->Add(1);
            mDiscardedItemSizeBytes->Add(inputSize);
        }
        return res;
    }

protected:
    // if serialized output contains output related info, it can be obtained via this member
    const Flusher* mFlusher = nullptr;

    mutable MetricsRecordRef mMetricsRecordRef;
    CounterPtr mInItemsCnt;
    CounterPtr mInItemSizeBytes;
    CounterPtr mOutItemsCnt;
    CounterPtr mOutItemSizeBytes;
    CounterPtr mDiscardedItemsCnt;
    CounterPtr mDiscardedItemSizeBytes;
    CounterPtr mTotalDelayMs;

private:
    virtual bool Serialize(T&& p, std::string& res, std::string& errorMsg) = 0;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SerializerUnittest;
#endif
};

using EventSerializer = Serializer<PipelineEventPtr>;
using EventGroupSerializer = Serializer<BatchedEvents>;
using EventGroupListSerializer = Serializer<BatchedEventsList>;

} // namespace logtail

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

#include "common/compression/Compressor.h"

#include <chrono>

#include "monitor/MetricConstants.h"

using namespace std;

namespace logtail {

void Compressor::SetMetricRecordRef(MetricLabels&& labels, DynamicMetricLabels&& dynamicLabels) {
    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
        mMetricsRecordRef, std::move(labels), std::move(dynamicLabels));
    mInItemsCnt = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_IN_ITEMS_CNT);
    mInItemSizeBytes = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_IN_ITEM_SIZE_BYTES);
    mOutItemsCnt = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_OUT_ITEMS_CNT);
    mOutItemSizeBytes = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_OUT_ITEM_SIZE_BYTES);
    mTotalDelayMs = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_TOTAL_DELAY_MS);
    mDiscardedItemsCnt = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_DISCARDED_ITEMS_CNT);
    mDiscardedItemSizeBytes = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_DISCARDED_ITEMS_SIZE_BYTES);
}

bool Compressor::DoCompress(const string& input, string& output, string& errorMsg) {
    if (mMetricsRecordRef != nullptr) {
        mInItemsCnt->Add(1);
        mInItemSizeBytes->Add(input.size());
    }

    auto before = chrono::system_clock::now();
    auto res = Compress(input, output, errorMsg);

    if (mMetricsRecordRef != nullptr) {
        mTotalDelayMs->Add(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - before).count());
        if (res) {
            mOutItemsCnt->Add(1);
            mOutItemSizeBytes->Add(output.size());
        } else {
            mDiscardedItemsCnt->Add(1);
            mDiscardedItemSizeBytes->Add(input.size());
        }
    }
    return res;
}

} // namespace logtail

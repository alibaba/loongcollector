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

#include "monitor/metric_constants/MetricConstants.h"

using namespace std;

namespace logtail {

void Compressor::SetMetricRecordRef(MetricLabels&& labels, DynamicMetricLabels&& dynamicLabels) {
    WriteMetrics::GetInstance()->CreateMetricsRecordRef(
        mMetricsRecordRef, MetricCategory::METRIC_CATEGORY_COMPONENT, std::move(labels), std::move(dynamicLabels));
    mInItemsTotal = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_IN_ITEMS_TOTAL);
    mInItemSizeBytes = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_IN_SIZE_BYTES);
    mOutItemsTotal = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_OUT_ITEMS_TOTAL);
    mOutItemSizeBytes = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_OUT_SIZE_BYTES);
    mTotalProcessMs = mMetricsRecordRef.CreateTimeCounter(METRIC_COMPONENT_TOTAL_PROCESS_TIME_MS);
    mDiscardedItemsTotal = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_DISCARDED_ITEMS_TOTAL);
    mDiscardedItemSizeBytes = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_DISCARDED_SIZE_BYTES);
    WriteMetrics::GetInstance()->CommitMetricsRecordRef(mMetricsRecordRef);
}

bool Compressor::DoCompress(const string& input, string& output, string& errorMsg) {
    if (mMetricsRecordRef != nullptr) {
        ADD_COUNTER(mInItemsTotal, 1);
        ADD_COUNTER(mInItemSizeBytes, input.size());
    }

    auto before = chrono::system_clock::now();
    auto res = Compress(input, output, errorMsg);

    if (mMetricsRecordRef != nullptr) {
        ADD_COUNTER(mTotalProcessMs, chrono::system_clock::now() - before);
        if (res) {
            ADD_COUNTER(mOutItemsTotal, 1);
            ADD_COUNTER(mOutItemSizeBytes, output.size());
        } else {
            ADD_COUNTER(mDiscardedItemsTotal, 1);
            ADD_COUNTER(mDiscardedItemSizeBytes, input.size());
        }
    }
    return res;
}

} // namespace logtail

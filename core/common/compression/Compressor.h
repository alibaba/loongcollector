/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <string>

#include "monitor/LogtailMetric.h"
#include "common/compression/CompressType.h"

namespace logtail {

class Compressor {
public:
    Compressor(CompressType type) : mType(type) {}
    virtual ~Compressor() = default;

    bool DoCompress(const std::string& input, std::string& output, std::string& errorMsg);

#ifdef APSARA_UNIT_TEST_MAIN
    // buffer shoudl be reserved for output before calling this function
    virtual bool UnCompress(const std::string& input, std::string& output, std::string& errorMsg) = 0;
#endif

    CompressType GetCompressType() const { return mType; }
    void SetMetricRecordRef(MetricLabels&& labels, DynamicMetricLabels&& dynamicLabels = {});

protected:
    mutable MetricsRecordRef mMetricsRecordRef;
    CounterPtr mInItemsTotal;
    CounterPtr mInItemSizeBytes;
    CounterPtr mOutItemsTotal;
    CounterPtr mOutItemSizeBytes;
    CounterPtr mDiscardedItemsTotal;
    CounterPtr mDiscardedItemSizeBytes;
    TimeCounterPtr mTotalProcessMs;

private:
    virtual bool Compress(const std::string& input, std::string& output, std::string& errorMsg) = 0;

    CompressType mType = CompressType::NONE;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class CompressorUnittest;
    friend class CompressorFactoryUnittest;
#endif
};

} // namespace logtail

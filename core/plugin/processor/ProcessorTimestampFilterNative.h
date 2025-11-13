/*
 * Copyright 2023 iLogtail Authors
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

#include <climits>

#include <limits>

#include "collection_pipeline/plugin/interface/Processor.h"

namespace logtail {
class ProcessorTimestampFilterNative : public Processor {
public:
    static const std::string sName;

    // Timestamp precision constants
    static constexpr const char* kTimestampPrecisionSecond = "second";
    static constexpr const char* kTimestampPrecisionMillisecond = "millisecond";
    static constexpr const char* kTimestampPrecisionNanosecond = "nanosecond";

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config) override;
    void Process(PipelineEventGroup& logGroup) override;

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override;

private:
    /// @return false if event should be discarded
    bool ProcessEvent(PipelineEventPtr& e);

    /// Parse timestamp string based on precision
    /// @return true if parse success, false otherwise
    bool ParseTimestamp(const StringView& timestampStr, int64_t& timestampNs, time_t& timestampSec);

    /// Get timestamp from event (either from SourceKey field or event timestamp)
    /// @return true if success, false otherwise
    bool GetEventTimestamp(const PipelineEventPtr& e, int64_t& timestampNs, time_t& timestampSec);

    // Source field name. Empty means use event timestamp.
    std::string mSourceKey;
    // Timestamp precision: kTimestampPrecisionSecond, kTimestampPrecisionMillisecond, kTimestampPrecisionNanosecond
    std::string mTimestampPrecision = kTimestampPrecisionSecond;
    // Whether to use nanosecond precision comparison
    bool mUseNanosecondComparison = false;
    // Upper bound timestamp in seconds (used when precision is second)
    time_t mUpperBoundSec = std::numeric_limits<time_t>::max();
    // Lower bound timestamp in seconds (used when precision is second)
    time_t mLowerBoundSec = 0;
    // Upper bound timestamp in nanoseconds (used when precision is millisecond or nanosecond)
    int64_t mUpperBoundNs = std::numeric_limits<int64_t>::max();
    // Lower bound timestamp in nanoseconds (used when precision is millisecond or nanosecond)
    int64_t mLowerBoundNs = 0;

    CounterPtr mDiscardedEventsTotal;
    CounterPtr mOutFailedEventsTotal;
    CounterPtr mOutKeyNotFoundEventsTotal;
    CounterPtr mOutSuccessfulEventsTotal;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorTimestampFilterNativeUnittest;
#endif
};

} // namespace logtail

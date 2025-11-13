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

#include "plugin/processor/ProcessorTimestampFilterNative.h"

#include <cctype>

#include <algorithm>

#include "common/ParamExtractor.h"
#include "common/StringTools.h"
#include "models/LogEvent.h"
#include "monitor/metric_constants/MetricConstants.h"

namespace logtail {

const std::string ProcessorTimestampFilterNative::sName = "processor_timestamp_filter_native";

bool ProcessorTimestampFilterNative::Init(const Json::Value& config) {
    std::string errorMsg;

    // SourceKey: optional, default empty (use event timestamp)
    if (!GetOptionalStringParam(config, "SourceKey", mSourceKey, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }

    // TimestampPrecision: optional, default "second"
    std::string precisionStr;
    if (!GetOptionalStringParam(config, "TimestampPrecision", precisionStr, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    } else if (!precisionStr.empty()) {
        // Normalize to lowercase
        std::transform(precisionStr.begin(), precisionStr.end(), precisionStr.begin(), ::tolower);
        if (precisionStr == kTimestampPrecisionSecond) {
            mTimestampPrecision = kTimestampPrecisionSecond;
            mUseNanosecondComparison = false;
        } else if (precisionStr == kTimestampPrecisionMillisecond) {
            mTimestampPrecision = kTimestampPrecisionMillisecond;
            mUseNanosecondComparison = true;
        } else if (precisionStr == kTimestampPrecisionNanosecond) {
            mTimestampPrecision = kTimestampPrecisionNanosecond;
            mUseNanosecondComparison = true;
        } else {
            PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                                  mContext->GetAlarm(),
                                  "param TimestampPrecision is not valid, use default: second",
                                  mTimestampPrecision,
                                  sName,
                                  mContext->GetConfigName(),
                                  mContext->GetProjectName(),
                                  mContext->GetLogstoreName(),
                                  mContext->GetRegion());
            mUseNanosecondComparison = false;
        }
    }

    // UpperBound: optional, default max timestamp
    int64_t upperBoundInt64 = 0;
    if (!GetOptionalInt64Param(config, "UpperBound", upperBoundInt64, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    } else if (upperBoundInt64 > 0) {
        if (mUseNanosecondComparison) {
            // Use nanosecond precision
            if (mTimestampPrecision == kTimestampPrecisionMillisecond) {
                mUpperBoundNs = upperBoundInt64 * 1000000LL; // ms to ns
            } else {
                // nanosecond
                mUpperBoundNs = upperBoundInt64;
            }
        } else {
            // Use second precision - no conversion needed
            mUpperBoundSec = static_cast<time_t>(upperBoundInt64);
        }
    } else {
        // Default: use max value based on precision
        if (mUseNanosecondComparison) {
            mUpperBoundNs = std::numeric_limits<int64_t>::max();
        } else {
            mUpperBoundSec = std::numeric_limits<time_t>::max();
        }
    }

    // LowerBound: optional, default 0
    int64_t lowerBoundInt64 = 0;
    if (!GetOptionalInt64Param(config, "LowerBound", lowerBoundInt64, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    } else {
        if (mUseNanosecondComparison) {
            // Use nanosecond precision (allow 0 and negative values)
            if (mTimestampPrecision == kTimestampPrecisionMillisecond) {
                mLowerBoundNs = lowerBoundInt64 * 1000000LL; // ms to ns
            } else {
                // nanosecond
                mLowerBoundNs = lowerBoundInt64;
            }
        } else {
            // Use second precision - no conversion needed (allow 0 and negative values)
            mLowerBoundSec = static_cast<time_t>(lowerBoundInt64);
        }
    }

    mDiscardedEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_DISCARDED_EVENTS_TOTAL);
    mOutFailedEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_FAILED_EVENTS_TOTAL);
    mOutKeyNotFoundEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_KEY_NOT_FOUND_EVENTS_TOTAL);
    mOutSuccessfulEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_SUCCESSFUL_EVENTS_TOTAL);

    return true;
}

void ProcessorTimestampFilterNative::Process(PipelineEventGroup& logGroup) {
    if (logGroup.GetEvents().empty()) {
        return;
    }

    EventsContainer& events = logGroup.MutableEvents();
    size_t wIdx = 0;
    for (size_t rIdx = 0; rIdx < events.size(); ++rIdx) {
        if (ProcessEvent(events[rIdx])) {
            if (wIdx != rIdx) {
                events[wIdx] = std::move(events[rIdx]);
            }
            ++wIdx;
        }
    }
    events.resize(wIdx);
}

bool ProcessorTimestampFilterNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<LogEvent>();
}

bool ProcessorTimestampFilterNative::ProcessEvent(PipelineEventPtr& e) {
    if (!IsSupportedEvent(e)) {
        ADD_COUNTER(mOutFailedEventsTotal, 1);
        return true; // Keep unsupported events
    }

    int64_t timestampNs = 0;
    time_t timestampSec = 0;
    if (!GetEventTimestamp(e, timestampNs, timestampSec)) {
        ADD_COUNTER(mOutFailedEventsTotal, 1);
        return true; // Keep events that fail to get timestamp
    }

    // Check bounds based on precision
    bool shouldDiscard = false;
    if (mUseNanosecondComparison) {
        // Compare in nanoseconds for millisecond/nanosecond precision
        if (timestampNs < mLowerBoundNs || timestampNs > mUpperBoundNs) {
            shouldDiscard = true;
        }
    } else {
        // Compare in seconds for second precision (more efficient)
        if (timestampSec < mLowerBoundSec || timestampSec > mUpperBoundSec) {
            shouldDiscard = true;
        }
    }

    if (shouldDiscard) {
        ADD_COUNTER(mDiscardedEventsTotal, 1);
        return false; // Discard event
    }

    ADD_COUNTER(mOutSuccessfulEventsTotal, 1);
    return true; // Keep event
}

bool ProcessorTimestampFilterNative::GetEventTimestamp(const PipelineEventPtr& e,
                                                       int64_t& timestampNs,
                                                       time_t& timestampSec) {
    if (mSourceKey.empty()) {
        // Use event timestamp directly
        timestampSec = e->GetTimestamp();
        if (mUseNanosecondComparison) {
            // Convert to nanoseconds for millisecond/nanosecond precision
            auto nanosecondOpt = e->GetTimestampNanosecond();
            uint32_t nanosecond = nanosecondOpt.has_value() ? nanosecondOpt.value() : 0;
            timestampNs = static_cast<int64_t>(timestampSec) * 1000000000LL + nanosecond;
        }
        // For second precision, timestampNs is not used, skip conversion for efficiency
        return true;
    }

    // Get timestamp from SourceKey field
    const LogEvent& logEvent = e.Cast<LogEvent>();
    if (!logEvent.HasContent(mSourceKey)) {
        ADD_COUNTER(mOutKeyNotFoundEventsTotal, 1);
        return false;
    }

    const StringView& timestampStr = logEvent.GetContent(mSourceKey);
    return ParseTimestamp(timestampStr, timestampNs, timestampSec);
}

bool ProcessorTimestampFilterNative::ParseTimestamp(const StringView& timestampStr,
                                                    int64_t& timestampNs,
                                                    time_t& timestampSec) {
    int64_t timestampValue = 0;
    if (!StringTo(timestampStr, timestampValue)) {
        return false;
    }

    if (mTimestampPrecision == kTimestampPrecisionSecond) {
        // Second precision - use seconds directly (timestampNs not used)
        timestampSec = static_cast<time_t>(timestampValue);
    } else if (mTimestampPrecision == kTimestampPrecisionMillisecond) {
        // Millisecond precision - convert to nanoseconds
        timestampNs = timestampValue * 1000000LL; // ms to ns
        timestampSec = static_cast<time_t>(timestampNs / 1000000000LL);
    } else {
        // Nanosecond precision
        timestampNs = timestampValue;
        timestampSec = static_cast<time_t>(timestampNs / 1000000000LL);
    }

    return true;
}

} // namespace logtail

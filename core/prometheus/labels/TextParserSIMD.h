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

#include <immintrin.h>

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <string>

#include "models/MetricEvent.h"
#include "models/PipelineEventGroup.h"
#include "models/StringView.h"

namespace logtail::prom {

enum class TextState { Start, Done, Error };

// no strict grammar for prom
class TextParserSIMD {
public:
    TextParserSIMD() = default;
    explicit TextParserSIMD(bool honorTimestamps);

    void SetDefaultTimestamp(uint64_t defaultTimestamp, uint32_t defaultNanoSec);

    PipelineEventGroup Parse(const std::string& content, uint64_t defaultTimestamp, uint32_t defaultNanoSec);

    bool ParseLine(StringView line, MetricEvent& metricEvent);

private:
    std::optional<size_t> FindFirstLetter(const char* s, size_t len, char target);
    std::optional<size_t> FindFirstWhiteSpace(const char* s, size_t len);
    std::optional<size_t> FindWhiteSpaceAndExemplar(const char* s, size_t len);

    std::optional<size_t> SkipTrailingWhitespace(const char* s, size_t pos);
    inline size_t SkipLeadingWhitespace(const char* s, size_t len, size_t pos);

    void HandleError(const std::string& errMsg);

    void HandleStart(MetricEvent& metricEvent, const char* s, size_t len);
    void HandleMetricName(MetricEvent& metricEvent, const char* s, size_t len);
    void HandleLabelName(MetricEvent& metricEvent, const char* s, size_t len);
    void HandleLabelValue(MetricEvent& metricEvent, const char* s, size_t len);
    void HandleSampleValue(MetricEvent& metricEvent, const char* s, size_t len);
    void HandleTimestamp(MetricEvent& metricEvent, const char* s, size_t len);

    TextState mState{TextState::Start};
    bool mEscape{false};

    StringView mLabelName;

    bool mHonorTimestamps{true};
    time_t mDefaultTimestamp{0};
    uint32_t mDefaultNanoTimestamp{0};

#ifdef APSARA_UNIT_TEST_MAIN
    friend class TextParserUnittest;
#endif
};

} // namespace logtail::prom

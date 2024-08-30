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

#include <string>

#include "models/MetricEvent.h"
#include "models/PipelineEventGroup.h"

namespace logtail {

enum class TextState { Start, Done, Error };

class TextParser {
public:
    TextParser() = default;

    PipelineEventGroup Parse(const std::string& content, uint64_t defaultTimestamp, uint32_t defaultNanoTs);
    PipelineEventGroup BuildLogGroup(const std::string& content);

    bool ParseLine(StringView line, uint64_t defaultTimestamp, uint32_t defaultNanoTs, MetricEvent& metricEvent);

private:
    void HandleError(const std::string& errMsg);

    void HandleStart(MetricEvent& metricEvent);
    void HandleMetricName(MetricEvent& metricEvent);
    void HandleOpenBrace(MetricEvent& metricEvent);
    void HandleLabelName(MetricEvent& metricEvent);
    void HandleEqualSign(MetricEvent& metricEvent);
    void HandleLabelValue(MetricEvent& metricEvent);
    void HandleCommaOrCloseBrace(MetricEvent& metricEvent);
    void HandleSampleValue(MetricEvent& metricEvent);
    void HandleTimestamp(MetricEvent& metricEvent);
    void HandleSpace(MetricEvent& metricEvent);

    inline void SkipLeadingWhitespace();

    TextState mState{TextState::Start};
    StringView mLine;
    std::size_t mPos{0};

    StringView mLabelName;
    std::string mEscapedLabelValue;
    double mSampleValue{0.0};
    time_t mTimestamp{0};
    uint32_t mNanoTimestamp{0};
    std::size_t mTokenLength{0};
    std::string mDoubleStr;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class TextParserUnittest;
#endif
};

} // namespace logtail

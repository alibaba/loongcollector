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

#include "prometheus/labels/TextParser.h"

#include <boost/algorithm/string.hpp>
#include <charconv>
#include <cmath>
#include <string>

#include "common/StringTools.h"
#include "logger/Logger.h"
#include "models/MetricEvent.h"
#include "models/PipelineEventGroup.h"
#include "models/StringView.h"
#include "prometheus/Constants.h"
#include "prometheus/Utils.h"

using namespace std;

namespace logtail {

bool IsValidNumberChar(char c) {
    static const unordered_set<char> sValidChars
        = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '-', '+', 'e', 'E', 'I',
           'N', 'F', 'T', 'Y', 'i', 'n', 'f', 't', 'y', 'X', 'x', 'N', 'n', 'A', 'a'};
    return sValidChars.count(c);
};

TextParser::TextParser(bool honorTimestamps) : mHonorTimestamps(honorTimestamps) {
}

void TextParser::SetDefaultTimestamp(uint64_t defaultTimestamp, uint32_t defaultNanoSec) {
    mDefaultTimestamp = defaultTimestamp;
    mDefaultNanoTimestamp = defaultNanoSec;
}

PipelineEventGroup TextParser::Parse(const string& content, uint64_t defaultTimestamp, uint32_t defaultNanoSec) {
    SetDefaultTimestamp(defaultTimestamp, defaultNanoSec);
    auto eGroup = PipelineEventGroup(make_shared<SourceBuffer>());
    vector<StringView> lines;
    // pre-reserve vector size by 1024 which is experience value per line
    lines.reserve(content.size() / 1024);
    SplitStringView(content, '\n', lines);
    for (const auto& line : lines) {
        if (!IsValidMetric(line)) {
            continue;
        }
        auto metricEvent = eGroup.CreateMetricEvent();
        if (ParseLine(line, *metricEvent)) {
            eGroup.MutableEvents().emplace_back(std::move(metricEvent), false, nullptr);
        }
    }

    return eGroup;
}

PipelineEventGroup TextParser::BuildLogGroup(const string& content) {
    PipelineEventGroup eGroup(std::make_shared<SourceBuffer>());

    vector<StringView> lines;
    // pre-reserve vector size by 1024 which is experience value per line
    lines.reserve(content.size() / 1024);
    SplitStringView(content, '\n', lines);
    for (const auto& line : lines) {
        if (!IsValidMetric(line)) {
            continue;
        }
        auto* logEvent = eGroup.AddLogEvent();
        logEvent->SetContent(prometheus::PROMETHEUS, line);
    }

    return eGroup;
}

bool TextParser::ParseLine(StringView line, MetricEvent& metricEvent) {
    mLine = line;
    mPos = 0;
    mState = TextState::Start;
    mLabelName.clear();
    mTokenLength = 0;

    HandleStart(metricEvent);

    if (mState == TextState::Done) {
        return true;
    }

    return false;
}

// start to parse metric sample:test_metric{k1="v1", k2="v2" } 9.9410452992e+10 1715829785083 # exemplarsxxx
void TextParser::HandleStart(MetricEvent& metricEvent) {
    SkipLeadingWhitespace();
    auto c = (mPos < mLine.size()) ? mLine[mPos] : '\0';
    if (std::isalpha(c) || c == '_' || c == ':') {
        HandleMetricName(metricEvent);
    } else {
        HandleError("expected metric name");
    }
}

// parse:test_metric{k1="v1", k2="v2" } 9.9410452992e+10 1715829785083 # exemplarsxxx
void TextParser::HandleMetricName(MetricEvent& metricEvent) {
    char c = (mPos < mLine.size()) ? mLine[mPos] : '\0';
    while (std::isalpha(c) || c == '_' || c == ':' || std::isdigit(c)) {
        ++mTokenLength;
        ++mPos;
        c = (mPos < mLine.size()) ? mLine[mPos] : '\0';
    }
    metricEvent.SetNameNoCopy(mLine.substr(mPos - mTokenLength, mTokenLength));
    mTokenLength = 0;
    SkipLeadingWhitespace();
    if (mPos < mLine.size()) {
        if (mLine[mPos] == '{') {
            ++mPos;
            SkipLeadingWhitespace();
            HandleLabelName(metricEvent);
        } else {
            HandleSampleValue(metricEvent);
        }
    } else {
        HandleError("error end of metric name");
    }
}

// parse:k1="v1", k2="v2" } 9.9410452992e+10 1715829785083 # exemplarsxxx
void TextParser::HandleLabelName(MetricEvent& metricEvent) {
    char c = (mPos < mLine.size()) ? mLine[mPos] : '\0';
    if (std::isalpha(c) || c == '_') {
        while (std::isalpha(c) || c == '_' || std::isdigit(c)) {
            ++mTokenLength;
            ++mPos;
            c = (mPos < mLine.size()) ? mLine[mPos] : '\0';
        }
        mLabelName = mLine.substr(mPos - mTokenLength, mTokenLength);
        mTokenLength = 0;
        SkipLeadingWhitespace();
        if (mPos == mLine.size() || mLine[mPos] != '=') {
            HandleError("expected '=' after label name");
            return;
        }
        ++mPos;
        SkipLeadingWhitespace();
        HandleEqualSign(metricEvent);
    } else if (c == '}') {
        ++mPos;
        SkipLeadingWhitespace();
        HandleSampleValue(metricEvent);
    } else {
        HandleError("invalid character in label name");
    }
}

// parse:"v1", k2="v2" } 9.9410452992e+10 1715829785083 # exemplarsxxx
void TextParser::HandleEqualSign(MetricEvent& metricEvent) {
    if (mPos < mLine.size() && mLine[mPos] == '"') {
        ++mPos;
        HandleLabelValue(metricEvent);
    } else {
        HandleError("expected '\"' after '='");
    }
}

// parse:v1", k2="v2" } 9.9410452992e+10 1715829785083 # exemplarsxxx
void TextParser::HandleLabelValue(MetricEvent& metricEvent) {
    // left quote has been consumed
    // LableValue supports escape char
    bool escaped = false;
    auto lPos = mPos;
    while (mPos < mLine.size() && mLine[mPos] != '"') {
        if (mLine[mPos] != '\\') {
            if (escaped) {
                mEscapedLabelValue.push_back(mLine[mPos]);
            }
            ++mPos;
            ++mTokenLength;
        } else {
            if (escaped == false) {
                // first meet escape char
                escaped = true;
                mEscapedLabelValue = mLine.substr(lPos, mPos - lPos).to_string();
            }
            if (mPos + 1 < mLine.size()) {
                // check next char, if it is valid escape char, we can consume two chars and push one escaped char
                // if not, we neet to push the two chars
                // valid escape char: \", \\, \n
                switch (mLine[lPos + 1]) {
                    case '\\':
                    case '\"':
                        mEscapedLabelValue.push_back(mLine[mPos + 1]);
                        break;
                    case 'n':
                        mEscapedLabelValue.push_back('\n');
                        break;
                    default:
                        mEscapedLabelValue.push_back('\\');
                        mEscapedLabelValue.push_back(mLine[mPos + 1]);
                        break;
                }
                mPos += 2;
            } else {
                mEscapedLabelValue.push_back(mLine[mPos + 1]);
                ++mPos;
            }
        }
    }

    if (mPos == mLine.size()) {
        HandleError("unexpected end of input in label value");
        return;
    }

    if (!escaped) {
        metricEvent.SetTagNoCopy(mLabelName, mLine.substr(mPos - mTokenLength, mTokenLength));
    } else {
        metricEvent.SetTag(mLabelName.to_string(), mEscapedLabelValue);
        mEscapedLabelValue.clear();
    }
    mTokenLength = 0;
    ++mPos;
    SkipLeadingWhitespace();
    if (mPos < mLine.size() && (mLine[mPos] == ',' || mLine[mPos] == '}')) {
        HandleCommaOrCloseBrace(metricEvent);
    } else {
        HandleError("unexpected end of input in label value");
    }
}

// parse:, k2="v2" } 9.9410452992e+10 1715829785083 # exemplarsxxx
// or parse:} 9.9410452992e+10 1715829785083 # exemplarsxxx
void TextParser::HandleCommaOrCloseBrace(MetricEvent& metricEvent) {
    char c = (mPos < mLine.size()) ? mLine[mPos] : '\0';
    if (c == ',') {
        ++mPos;
        SkipLeadingWhitespace();
        HandleLabelName(metricEvent);
    } else if (c == '}') {
        ++mPos;
        SkipLeadingWhitespace();
        HandleSampleValue(metricEvent);
    } else {
        HandleError("expected ',' or '}' after label value");
    }
}

// parse:9.9410452992e+10 1715829785083 # exemplarsxxx
void TextParser::HandleSampleValue(MetricEvent& metricEvent) {
    while (mPos < mLine.size() && IsValidNumberChar(mLine[mPos])) {
        ++mPos;
        ++mTokenLength;
    }

    if (mPos < mLine.size() && mLine[mPos] != ' ' && mLine[mPos] != '\t' && mLine[mPos] != '#') {
        HandleError("unexpected end of input in sample value");
        return;
    }

    auto tmpSampleValue = mLine.substr(mPos - mTokenLength, mTokenLength);
    mDoubleStr = tmpSampleValue.to_string();

    try {
        mSampleValue = std::stod(mDoubleStr);
    } catch (...) {
        HandleError("invalid sample value");
        mTokenLength = 0;
        return;
    }
    mDoubleStr.clear();

    metricEvent.SetValue<UntypedSingleValue>(mSampleValue);
    mTokenLength = 0;
    SkipLeadingWhitespace();
    if (mPos == mLine.size() || mLine[mPos] == '#' || !mHonorTimestamps) {
        metricEvent.SetTimestamp(mDefaultTimestamp, mDefaultNanoTimestamp);
        mState = TextState::Done;
    } else {
        HandleTimestamp(metricEvent);
    }
}

// parse:1715829785083 # exemplarsxxx
// timestamp will be 1715829785.083 in OpenMetrics
void TextParser::HandleTimestamp(MetricEvent& metricEvent) {
    // '#' is for exemplars, and we don't need it
    while (mPos < mLine.size() && IsValidNumberChar(mLine[mPos])) {
        ++mPos;
        ++mTokenLength;
    }
    if (mPos < mLine.size() && mLine[mPos] != ' ' && mLine[mPos] != '\t' && mLine[mPos] != '#') {
        HandleError("unexpected end of input in sample timestamp");
        return;
    }

    auto tmpTimestamp = mLine.substr(mPos - mTokenLength, mTokenLength);
    if (tmpTimestamp.size() == 0) {
        mState = TextState::Done;
        return;
    }
    mDoubleStr = tmpTimestamp.to_string();
    double milliTimestamp = 0;
    try {
        milliTimestamp = stod(mDoubleStr);
    } catch (...) {
        HandleError("invalid timestamp");
        mTokenLength = 0;
        return;
    }
    mDoubleStr.clear();

    if (milliTimestamp > 1ULL << 63) {
        HandleError("timestamp overflow");
        mTokenLength = 0;
        return;
    }
    if (milliTimestamp < 1UL << 31) {
        milliTimestamp *= 1000;
    }
    time_t timestamp = (int64_t)milliTimestamp / 1000;
    auto ns = ((int64_t)milliTimestamp % 1000) * 1000000;
    if (mHonorTimestamps) {
        metricEvent.SetTimestamp(timestamp, ns);
    } else {
        metricEvent.SetTimestamp(mDefaultTimestamp, mDefaultNanoTimestamp);
    }

    mTokenLength = 0;

    mState = TextState::Done;
}

void TextParser::HandleError(const string& errMsg) {
    LOG_WARNING(sLogger, ("text parser error parsing line", mLine.to_string() + errMsg));
    mState = TextState::Error;
}

inline void TextParser::SkipLeadingWhitespace() {
    while (mPos < mLine.length() && (mLine[mPos] == ' ' || mLine[mPos] == '\t')) {
        mPos++;
    }
}

} // namespace logtail

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

#include "prometheus/labels/TextParserSIMD.h"

#include <immintrin.h>

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <string>

#include "logger/Logger.h"
#include "models/MetricEvent.h"
#include "models/PipelineEventGroup.h"
#include "models/StringView.h"
#include "prometheus/Utils.h"

using namespace std;

namespace logtail::prom {

TextParserSIMD::TextParserSIMD(bool honorTimestamps) : mHonorTimestamps(honorTimestamps) {
}

void TextParserSIMD::SetDefaultTimestamp(uint64_t defaultTimestamp, uint32_t defaultNanoSec) {
    mDefaultTimestamp = defaultTimestamp;
    mDefaultNanoTimestamp = defaultNanoSec;
}

PipelineEventGroup
TextParserSIMD::Parse(const std::string& content, uint64_t defaultTimestamp, uint32_t defaultNanoSec) {
    SetDefaultTimestamp(defaultTimestamp, defaultNanoSec);
    auto eGroup = PipelineEventGroup(std::make_shared<SourceBuffer>());
    std::vector<StringView> lines;
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

bool TextParserSIMD::ParseLine(StringView line, MetricEvent& metricEvent) {
    mState = TextState::Start;
    mLabelName.clear();
    mEscape = FindFirstLetter(line.data(), line.size(), '\\').has_value();

    HandleStart(metricEvent, line.data(), line.size());

    if (mState == TextState::Done) {
        return true;
    }

    return false;
}

std::optional<size_t> TextParserSIMD::FindFirstLetter(const char* s, size_t len, char target) {
    size_t res = 0;

    __m128i targetVec = _mm_set1_epi8(target);

    while (res + 16 < len) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&s[res]));

        __m128i cmp = _mm_cmpeq_epi8(chunk, targetVec);

        int mask = _mm_movemask_epi8(cmp);

        if (mask != 0) {
            return res + __builtin_ffs(mask) - 1;
        }

        res += 16;
    }

    while (res < len) {
        if (s[res] == target) {
            return res;
        }
        res++;
    }
    return std::nullopt;
}
std::optional<size_t> TextParserSIMD::FindFirstWhiteSpace(const char* s, size_t len) {
    size_t res = 0;

    static __m128i sTargetVec1 = _mm_set1_epi8(' ');
    static __m128i sTargetVec2 = _mm_set1_epi8('\t');

    while (res + 16 < len) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&s[res]));

        __m128i cmp1 = _mm_cmpeq_epi8(chunk, sTargetVec1);
        __m128i cmp2 = _mm_cmpeq_epi8(chunk, sTargetVec2);

        int mask1 = _mm_movemask_epi8(cmp1);
        int mask2 = _mm_movemask_epi8(cmp2);

        if (mask1 != 0) {
            return res + __builtin_ffs(mask1) - 1;
        }
        if (mask2 != 0) {
            return res + __builtin_ffs(mask2) - 1;
        }

        res += 16;
    }

    while (res < len) {
        if (s[res] == ' ' || s[res] == '\t') {
            return res;
        }
        res++;
    }
    return std::nullopt;
}

std::optional<size_t> TextParserSIMD::FindWhiteSpaceAndExemplar(const char* s, size_t len) {
    size_t res = 0;

    static __m128i sTargetVec1 = _mm_set1_epi8(' ');
    static __m128i sTargetVec2 = _mm_set1_epi8('\t');
    static __m128i sTargetVec3 = _mm_set1_epi8('#');

    while (res + 16 < len) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&s[res]));

        __m128i cmp1 = _mm_cmpeq_epi8(chunk, sTargetVec1);
        __m128i cmp2 = _mm_cmpeq_epi8(chunk, sTargetVec2);
        __m128i cmp3 = _mm_cmpeq_epi8(chunk, sTargetVec3);

        int mask1 = _mm_movemask_epi8(cmp1);
        int mask2 = _mm_movemask_epi8(cmp2);
        int mask3 = _mm_movemask_epi8(cmp3);

        if (mask1 != 0) {
            return res + __builtin_ffs(mask1) - 1;
        }
        if (mask2 != 0) {
            return res + __builtin_ffs(mask2) - 1;
        }
        if (mask3 != 0) {
            return res + __builtin_ffs(mask3) - 1;
        }

        res += 16;
    }

    while (res < len) {
        if (s[res] == ' ' || s[res] == '\t' || s[res] == '#') {
            return res;
        }
        res++;
    }
    return std::nullopt;
}

std::optional<size_t> TextParserSIMD::SkipTrailingWhitespace(const char* s, size_t pos) {
    for (; pos > 0 && (s[pos] == ' ' || s[pos] == '\t'); --pos) {
    }
    if (pos == 0 && (s[pos] == ' ' || s[pos] == '\t')) {
        return std::nullopt;
    }
    return pos;
}


void TextParserSIMD::HandleError(const std::string& errMsg) {
    LOG_WARNING(sLogger, ("text parser error parsing line", errMsg));
    mState = TextState::Error;
}

void TextParserSIMD::HandleStart(MetricEvent& metricEvent, const char* s, const size_t len) {
    auto pos = SkipLeadingWhitespace(s, len, 0);
    HandleMetricName(metricEvent, s + pos, len - pos);
}
void TextParserSIMD::HandleMetricName(MetricEvent& metricEvent, const char* s, size_t len) {
    auto pos = FindFirstLetter(s, len, '{');
    if (pos.has_value()) {
        auto endPos = SkipTrailingWhitespace(s, pos.value() - 1);
        if (endPos.has_value()) {
            metricEvent.SetNameNoCopy(StringView(s, endPos.value() + 1));
        } else {
            HandleError("error end of metric name");
            return;
        }
        auto nextPos = SkipLeadingWhitespace(s, len, pos.value() + 1);
        HandleLabelName(metricEvent, s + nextPos, len - nextPos);
    } else {
        auto nextPos = FindFirstWhiteSpace(s, len);
        if (nextPos.has_value()) {
            metricEvent.SetNameNoCopy(StringView(s, nextPos.value()));
            auto nextNextPos = SkipLeadingWhitespace(s, len, nextPos.value());
            HandleSampleValue(metricEvent, s + nextNextPos, len - nextNextPos);
        }
    }
}
void TextParserSIMD::HandleLabelName(MetricEvent& metricEvent, const char* s, size_t len) {
    auto pos = FindFirstLetter(s, len, '=');
    if (pos.has_value()) {
        auto endPos = SkipTrailingWhitespace(s, pos.value() - 1);
        if (endPos.has_value()) {
            if (FindFirstLetter(s, endPos.value(), '"').has_value()) {
                HandleError("invalid character in label name");
                return;
            }
            mLabelName = StringView(s, endPos.value() + 1);
        } else {
            HandleError("error end of metric name");
            return;
        }
        auto nextPos = SkipLeadingWhitespace(s, len, pos.value() + 1);
        HandleLabelValue(metricEvent, s + nextPos, len - nextPos);
    } else {
        if (len > 0 && s[0] == '}') {
            auto nextPos = SkipLeadingWhitespace(s, len, 1);
            HandleSampleValue(metricEvent, s + nextPos, len - nextPos);
        } else {
            HandleError("invalid character in label name");
        }
    }
}
void TextParserSIMD::HandleLabelValue(MetricEvent& metricEvent, const char* s, size_t len) {
    // left quote has been consumed
    // LableValue supports escape char
    if (len == 0 || s[0] != '"') {
        HandleError("invalid character in label value");
        return;
    }
    s = s + 1;
    len--;
    size_t nextPos = 0;
    if (mEscape) {
        // slow path
        // escape char
        std::string labelValue;
        size_t pos = 0;
        for (size_t i = 0; i < len; i++) {
            if (s[i] == '\\') {
                if (i + 1 < len) {
                    switch (s[i + 1]) {
                        case 'n':
                            labelValue.push_back('\n');
                            break;
                        case '\\':
                        case '\"':
                            labelValue.push_back(s[i + 1]);
                            break;
                        default:
                            labelValue.push_back('\\');
                            labelValue.push_back(s[i + 1]);
                            break;
                    }
                    i++;
                } else {
                    HandleError("invalid escape char");
                    return;
                }
            } else if (s[i] == '"') {
                pos = i;
                break;
            } else {
                labelValue.push_back(s[i]);
            }
        }
        auto sb = metricEvent.GetSourceBuffer()->CopyString(labelValue);
        metricEvent.SetTag(mLabelName, StringView(sb.data, sb.size));
        nextPos = SkipLeadingWhitespace(s, len, pos + 1);
    } else {
        const auto pos = FindFirstLetter(s, len, '"');
        if (pos.has_value()) {
            metricEvent.SetTagNoCopy(mLabelName, StringView(s, pos.value()));
            nextPos = SkipLeadingWhitespace(s, len, pos.value() + 1);
        } else {
            HandleError("invalid character in label value");
            return;
        }
    }
    if (s[nextPos] == ',') {
        nextPos++;
        nextPos = SkipLeadingWhitespace(s, len, nextPos);
        if (s[nextPos] == '}') {
            nextPos++;
            nextPos = SkipLeadingWhitespace(s, len, nextPos);
            HandleSampleValue(metricEvent, s + nextPos, len - nextPos);
            return;
        }
        HandleLabelName(metricEvent, s + nextPos, len - nextPos);
    } else if (s[nextPos] == '}') {
        nextPos++;
        nextPos = SkipLeadingWhitespace(s, len, nextPos);
        HandleSampleValue(metricEvent, s + nextPos, len - nextPos);
    } else {
        HandleError("invalid character in label value");
    }
}
void TextParserSIMD::HandleSampleValue(MetricEvent& metricEvent, const char* s, size_t len) {
    auto pos = FindWhiteSpaceAndExemplar(s, len);
    size_t valueLen = 0;
    if (pos.has_value()) {
        valueLen = pos.value();
    } else {
        valueLen = len;
    }
    if (valueLen == 0) {
        HandleError("invalid sample value");
        return;
    }
    auto tmpSampleValue = StringView(s, valueLen);
    try {
        auto sampleValue = std::stod(tmpSampleValue.to_string());
        metricEvent.SetValue<UntypedSingleValue>(sampleValue);
    } catch (...) {
        HandleError("invalid sample value");
        return;
    }
    if ((pos.has_value() && s[pos.value()] == '#') || valueLen == len) {
        metricEvent.SetTimestamp(mDefaultTimestamp, mDefaultNanoTimestamp);
        mState = TextState::Done;
        return;
    }
    s = s + pos.value() + 1;
    len -= pos.value() + 1;
    auto nextPos = SkipLeadingWhitespace(s, len, 0);
    HandleTimestamp(metricEvent, s + nextPos, len - nextPos);
}
void TextParserSIMD::HandleTimestamp(MetricEvent& metricEvent, const char* s, size_t len) {
    // '#' is for exemplars, and we don't need it
    auto pos = FindWhiteSpaceAndExemplar(s, len);
    size_t valueLen = 0;
    if (pos.has_value()) {
        valueLen = pos.value();
    } else {
        valueLen = len;
    }
    if (valueLen == 0) {
        mState = TextState::Done;
        return;
    }
    auto tmpTimestamp = StringView(s, valueLen);
    double milliTimestamp = 0;
    try {
        milliTimestamp = std::stod(tmpTimestamp.to_string());
    } catch (...) {
        HandleError("invalid timestamp");
        return;
    }

    if (milliTimestamp > 1ULL << 63) {
        HandleError("timestamp overflow");
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
    mState = TextState::Done;
}

inline size_t TextParserSIMD::SkipLeadingWhitespace(const char* s, size_t len, size_t pos) {
    while (pos < len && (s[pos] == ' ' || s[pos] == '\t')) {
        pos++;
    }
    return pos;
}


} // namespace logtail::prom

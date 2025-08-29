/*
 * Copyright 2025 iLogtail Authors
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

#include "host_monitor/common/FastFieldParser.h"

#include <algorithm>

namespace logtail {

// FastFieldParser implementation
bool FastFieldParser::SeekToField(size_t index) {
    Reset();
    for (size_t i = 0; i < index; ++i) {
        if (mCurrentPos >= mLine.size()) {
            return false;
        }

        SkipDelimiters();
        if (mCurrentPos >= mLine.size()) {
            return false;
        }

        // 跳过当前字段
        mCurrentPos = FindNextDelimiter(mCurrentPos);
    }

    SkipDelimiters();
    return mCurrentPos < mLine.size();
}

std::string_view FastFieldParser::GetCurrentField() {
    if (mCurrentPos >= mLine.size()) {
        return {};
    }

    size_t fieldStart = mCurrentPos;
    size_t fieldEnd = FindNextDelimiter(fieldStart);

    return mLine.substr(fieldStart, fieldEnd - fieldStart);
}

std::string_view FastFieldParser::GetField(size_t index) {
    if (!SeekToField(index)) {
        return {};
    }
    return GetCurrentField();
}

std::vector<std::string_view> FastFieldParser::GetFields(size_t startIndex, size_t count) {
    std::vector<std::string_view> result;
    result.reserve(count);

    if (!SeekToField(startIndex)) {
        return result;
    }

    for (size_t i = 0; i < count && mCurrentPos < mLine.size(); ++i) {
        SkipDelimiters();
        if (mCurrentPos >= mLine.size()) {
            break;
        }

        result.push_back(GetCurrentField());
        mCurrentPos = FindNextDelimiter(mCurrentPos);
    }

    return result;
}

bool FastFieldParser::FieldStartsWith(size_t index, std::string_view prefix) {
    auto field = GetField(index);
    return field.size() >= prefix.size() && field.substr(0, prefix.size()) == prefix;
}

size_t FastFieldParser::GetFieldCount() {
    Reset();
    size_t count = 0;

    while (mCurrentPos < mLine.size()) {
        SkipDelimiters();
        if (mCurrentPos >= mLine.size()) {
            break;
        }

        count++;
        mCurrentPos = FindNextDelimiter(mCurrentPos);
    }

    return count;
}

void FastFieldParser::SkipDelimiters() {
    while (mCurrentPos < mLine.size() && mLine[mCurrentPos] == mDelimiter) {
        mCurrentPos++;
    }
}

size_t FastFieldParser::FindNextDelimiter(size_t start) {
    size_t pos = start;
    while (pos < mLine.size() && mLine[pos] != mDelimiter) {
        pos++;
    }
    return pos;
}

// NetDevParser implementation
bool NetDevParser::ParseDeviceStats(std::string_view& deviceName, std::vector<uint64_t>& stats) {
    // 网络设备行格式: "  eth0: 1234 5678 ..."
    auto colonPos = mLine.find(':');
    if (colonPos == std::string_view::npos) {
        return false;
    }

    // 提取设备名（去除前导空格）
    auto nameStart = mLine.find_first_not_of(' ');
    if (nameStart == std::string_view::npos || nameStart >= colonPos) {
        return false;
    }

    deviceName = mLine.substr(nameStart, colonPos - nameStart);

    // 解析统计数据（冒号后的部分）
    auto statsLine = mLine.substr(colonPos + 1);
    FastFieldParser parser(statsLine);

    stats.clear();
    stats.reserve(16); // 网络设备通常有16个统计字段

    size_t fieldCount = parser.GetFieldCount();
    for (size_t i = 0; i < fieldCount; ++i) {
        auto value = parser.GetFieldAs<uint64_t>(i, 0);
        stats.push_back(value);
    }

    return !stats.empty();
}

// FastParse namespace functions
namespace FastParse {

std::string_view GetField(std::string_view line, size_t index, char delimiter) {
    FastFieldParser parser(line, delimiter);
    return parser.GetField(index);
}

bool FieldStartsWith(std::string_view line, size_t index, std::string_view prefix, char delimiter) {
    FastFieldParser parser(line, delimiter);
    return parser.FieldStartsWith(index, prefix);
}

} // namespace FastParse

} // namespace logtail

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
#include <vector>

namespace logtail {

StringView FastFieldParser::GetField(size_t index) {
    auto iter = mSplitter.begin();
    auto end = mSplitter.end();

    for (size_t i = 0; i < index && iter != end; ++i, ++iter) {
        // 跳过前面的字段
    }

    if (iter == end) {
        return StringView{}; // 返回空视图
    }

    return *iter;
}


bool FastFieldParser::FieldStartsWith(size_t index, StringView prefix) {
    auto field = GetField(index);
    return field.size() >= prefix.size() && field.substr(0, prefix.size()) == prefix;
}

size_t FastFieldParser::GetFieldCount() {
    size_t count = 0;
    auto iter = mSplitter.begin();
    auto end = mSplitter.end();
    while (iter != end) {
        ++count;
        ++iter;
    }
    return count;
}

// NetDevParser implementation
bool NetDevParser::ParseDeviceStats(StringView& deviceName, std::vector<uint64_t>& stats) {
    // 网络设备行格式: "  eth0: 1234 5678 ..."
    auto colonPos = mLine.find(':');
    if (colonPos == StringView::npos) {
        return false;
    }

    // 提取设备名（去除前导空格）
    auto nameStart = mLine.find_first_not_of(' ');
    if (nameStart == StringView::npos || nameStart >= colonPos) {
        return false;
    }

    deviceName = mLine.substr(nameStart, colonPos - nameStart);

    // 解析统计数据（冒号后的部分）
    auto statsLine = mLine.substr(colonPos + 1);
    FastFieldParser parser(statsLine);

    stats.clear();
    stats.reserve(16); // 网络设备通常有16个统计字段

    for (auto iter = parser.begin(); iter != parser.end(); ++iter) {
        uint64_t value;
        stats.push_back(StringTo(*iter, value) ? value : 0);
    }

    return !stats.empty();
}

// FastParse namespace functions
namespace FastParse {

StringView GetField(StringView line, size_t index, char delimiter) {
    FastFieldParser parser(line, delimiter);
    return parser.GetField(index);
}

bool FieldStartsWith(StringView line, size_t index, StringView prefix, char delimiter) {
    FastFieldParser parser(line, delimiter);
    return parser.FieldStartsWith(index, prefix);
}

} // namespace FastParse

} // namespace logtail

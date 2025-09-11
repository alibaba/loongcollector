/*
 * Copyright 2025 iLogtail Authors
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

#include "common/DynamicStringFormatter.h"

namespace logtail {

DynamicStringFormatter::DynamicStringFormatter() = default;

DynamicStringFormatter::~DynamicStringFormatter() = default;

bool DynamicStringFormatter::Init(const std::string& formatString) {
    mFormatTemplate = formatString;
    mPlaceholders.clear();
    mStaticParts.clear();

    return ParseFormatString(formatString);
}

bool DynamicStringFormatter::Format(const ValueProvider& valueProvider, std::string& result) const {
    if (!IsDynamic()) {
        result = mFormatTemplate;
        return true;
    }

    result.clear();

    for (size_t i = 0; i < mPlaceholders.size(); ++i) {
        if (i < mStaticParts.size()) {
            result += mStaticParts[i];
        }

        const auto& placeholder = mPlaceholders[i];
        std::string placeholderValue = valueProvider(placeholder.name);

        if (placeholderValue.empty()) {
            return false;
        }

        result += placeholderValue;
    }

    if (!mStaticParts.empty() && mPlaceholders.size() < mStaticParts.size()) {
        result += mStaticParts.back();
    }

    return true;
}

bool DynamicStringFormatter::ParseFormatString(const std::string& formatString) {
    size_t startPos = 0;
    size_t nextPos = 0;

    while (nextPos < formatString.length()) {
        size_t percentPos = formatString.find("%{", nextPos);
        size_t dollarPos = formatString.find("${", nextPos);

        size_t placeholderStart;
        std::string placeholderPrefix;

        if (percentPos == std::string::npos && dollarPos == std::string::npos) {
            break;
        } else if (percentPos == std::string::npos) {
            placeholderStart = dollarPos;
            placeholderPrefix = "${";
        } else if (dollarPos == std::string::npos) {
            placeholderStart = percentPos;
            placeholderPrefix = "%{";
        } else {
            if (percentPos < dollarPos) {
                placeholderStart = percentPos;
                placeholderPrefix = "%{";
            } else {
                placeholderStart = dollarPos;
                placeholderPrefix = "${";
            }
        }

        size_t placeholderEnd = formatString.find("}", placeholderStart);
        if (placeholderEnd == std::string::npos) {
            return false;
        }

        if (placeholderStart > startPos) {
            mStaticParts.emplace_back(formatString.substr(startPos, placeholderStart - startPos));
        }

        std::string placeholderName = formatString.substr(placeholderStart + 2, placeholderEnd - placeholderStart - 2);

        if (placeholderName.empty()) {
            return false;
        }

        PlaceholderInfo placeholderInfo;
        placeholderInfo.name = placeholderName;
        placeholderInfo.prefix = placeholderPrefix;
        placeholderInfo.position = mStaticParts.size() - 1;

        mPlaceholders.emplace_back(std::move(placeholderInfo));
        startPos = placeholderEnd + 1;
        nextPos = startPos;
    }

    if (startPos < formatString.length()) {
        mStaticParts.emplace_back(formatString.substr(startPos));
    }

    return true;
}

} // namespace logtail

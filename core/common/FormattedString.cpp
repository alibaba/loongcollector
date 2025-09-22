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

#include "common/FormattedString.h"

#include <cstdlib>

#include <unordered_set>

#include "models/LogEvent.h"

namespace logtail {

bool FormattedString::Init(const std::string& formatString) {
    mTemplate = formatString;
    mStaticParts.clear();
    mPlaceholderNames.clear();
    mRequiredKeys.clear();

    return ParseFormatString(formatString);
}

bool FormattedString::Format(const PipelineEventPtr& event, const GroupTags& groupTags, std::string& result) const {
    if (!IsDynamic()) {
        result = mTemplate;
        return true;
    }

    if (mStaticParts.size() != mPlaceholderNames.size() + 1) {
        return false;
    }

    result.clear();
    result.reserve(mTemplate.size());

    for (size_t idx = 0; idx < mPlaceholderNames.size(); ++idx) {
        result += mStaticParts[idx];
        const auto& key = mPlaceholderNames[idx];
        if (key.rfind("content.", 0) == 0) {
            if (event->GetType() != PipelineEvent::Type::LOG) {
                return false;
            }
            const auto& logEvent = event.Cast<LogEvent>();
            StringView fieldKey(key.data() + 8, key.size() - 8);
            StringView value = logEvent.GetContent(fieldKey);
            if (value.empty()) {
                return false;
            }
            result.append(value.data(), value.size());
            continue;
        }
        if (key.rfind("tag.", 0) == 0) {
            if (groupTags.empty()) {
                return false;
            }
            StringView fieldKey(key.data() + 4, key.size() - 4);
            auto it = groupTags.find(fieldKey);
            if (it == groupTags.end() || it->second.empty()) {
                return false;
            }
            const auto& value = it->second;
            result.append(value.data(), value.size());
            continue;
        }
    }

    result += mStaticParts.back();
    return true;
}

bool FormattedString::ParseFormatString(const std::string& formatString) {
    std::unordered_set<std::string> requiredKeySet;

    const size_t length = formatString.size();
    size_t position = 0;
    std::string currentStatic;

    while (position < length) {
        size_t percentPos = formatString.find("%{", position);
        size_t dollarPos = formatString.find("${", position);

        size_t nextPos = std::string::npos;
        bool isDynamicPlaceholder = false;

        if (percentPos != std::string::npos && (dollarPos == std::string::npos || percentPos < dollarPos)) {
            nextPos = percentPos;
            isDynamicPlaceholder = true;
        } else if (dollarPos != std::string::npos) {
            nextPos = dollarPos;
        }

        if (nextPos == std::string::npos) {
            break;
        }

        currentStatic.append(formatString, position, nextPos - position);

        size_t closingBrace = formatString.find('}', nextPos);
        if (closingBrace == std::string::npos) {
            return false;
        }

        const size_t nameStart = nextPos + 2;
        if (nameStart >= closingBrace) {
            return false;
        }
        const std::string placeholderName = formatString.substr(nameStart, closingBrace - nameStart);

        if (isDynamicPlaceholder) {
            mStaticParts.emplace_back(std::move(currentStatic));
            currentStatic.clear();

            mPlaceholderNames.emplace_back(placeholderName);
            if (requiredKeySet.insert(placeholderName).second) {
                mRequiredKeys.emplace_back(placeholderName);
            }
        } else {
            const char* envValue = std::getenv(placeholderName.c_str());
            if (envValue != nullptr) {
                currentStatic.append(envValue);
            }
        }

        position = closingBrace + 1;
    }

    if (position < length) {
        currentStatic.append(formatString, position, length - position);
    }

    mStaticParts.emplace_back(std::move(currentStatic));
    return true;
}

} // namespace logtail

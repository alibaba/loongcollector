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

#include "logger/Logger.h"
#include "models/LogEvent.h"

namespace logtail {

namespace {
static std::string ExpandEnvPlaceholders(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    size_t pos = 0;
    while (pos < input.size()) {
        size_t start = input.find("${", pos);
        if (start == std::string::npos) {
            out.append(input, pos, std::string::npos);
            break;
        }
        // copy preceding static part
        out.append(input, pos, start - pos);

        size_t end = input.find('}', start + 2);
        if (end == std::string::npos) {
            // No closing brace; append the rest verbatim
            out.append(input, start, std::string::npos);
            break;
        }
        const size_t nameStart = start + 2;
        if (nameStart >= end) {
            // Empty var name, keep literal
            out.append(input, start, end - start + 1);
            pos = end + 1;
            continue;
        }
        std::string varName = input.substr(nameStart, end - nameStart);
        const char* envVal = std::getenv(varName.c_str());
        if (envVal == nullptr) {
            LOG_WARNING(sLogger,
                        ("FormattedString env placeholder missing", varName)("action", "use empty string as default"));
            // replace by empty string
        } else {
            out.append(envVal);
        }
        pos = end + 1;
    }
    return out;
}
} // namespace

bool FormattedString::Init(const std::string& formatString) {
    mTemplate = ExpandEnvPlaceholders(formatString);
    mStaticParts.clear();
    mPlaceholderNames.clear();
    mRequiredKeys.clear();

    return ParseFormatString(mTemplate);
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
                LOG_WARNING(sLogger,
                            ("FormattedString dynamic placeholder requires LOG event",
                             key)("actual_type", static_cast<int>(event->GetType())));
                return false;
            }
            const auto& logEvent = event.Cast<LogEvent>();
            StringView fieldKey(key.data() + 8, key.size() - 8);
            StringView value = logEvent.GetContent(fieldKey);
            if (value.empty()) {
                LOG_WARNING(sLogger,
                            ("FormattedString missing or empty content field",
                             std::string(fieldKey.data(), fieldKey.size()))("placeholder", key));
                return false;
            }
            result.append(value.data(), value.size());
            continue;
        }
        if (key.rfind("tag.", 0) == 0) {
            if (groupTags.empty()) {
                LOG_WARNING(sLogger, ("FormattedString no group tags available for placeholder", key));
                return false;
            }
            StringView fieldKey(key.data() + 4, key.size() - 4);
            auto it = groupTags.find(fieldKey);
            if (it == groupTags.end() || it->second.empty()) {
                LOG_WARNING(sLogger,
                            ("FormattedString missing or empty tag key",
                             std::string(fieldKey.data(), fieldKey.size()))("placeholder", key));
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
        size_t nextPos = formatString.find("%{", position);
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

        mStaticParts.emplace_back(std::move(currentStatic));
        currentStatic.clear();

        mPlaceholderNames.emplace_back(placeholderName);
        if (requiredKeySet.insert(placeholderName).second) {
            mRequiredKeys.emplace_back(placeholderName);
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

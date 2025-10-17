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
#include <cstring>

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
        out.append(input, pos, start - pos);

        size_t end = input.find('}', start + 2);
        if (end == std::string::npos) {
            out.append(input, start, std::string::npos);
            break;
        }
        const size_t nameStart = start + 2;
        if (nameStart >= end) {
            out.append(input, start, end - start + 1);
            pos = end + 1;
            continue;
        }
        std::string varName = input.substr(nameStart, end - nameStart);
        const char* envVal = std::getenv(varName.c_str());
        if (envVal == nullptr) {
            LOG_WARNING(sLogger,
                        ("FormattedString env placeholder missing", varName)("action", "use empty string as default"));
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
        const auto& staticSeg = mStaticParts[idx];
        result.append(staticSeg.data(), staticSeg.size());
        const auto& key = mPlaceholderNames[idx];
        if (key.size() >= 8 && std::memcmp(key.data(), "content.", 8) == 0) {
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
                             std::string(fieldKey.data(), fieldKey.size()))("placeholder",
                                                                            std::string(key.data(), key.size())));
                return false;
            }
            result.append(value.data(), value.size());
            continue;
        }
        if (key.size() >= 4 && std::memcmp(key.data(), "tag.", 4) == 0) {
            if (groupTags.empty()) {
                LOG_WARNING(
                    sLogger,
                    ("FormattedString no group tags available for placeholder", std::string(key.data(), key.size())));
                return false;
            }
            StringView fieldKey(key.data() + 4, key.size() - 4);
            auto it = groupTags.find(fieldKey);
            if (it == groupTags.end() || it->second.empty()) {
                LOG_WARNING(sLogger,
                            ("FormattedString missing or empty tag key", std::string(fieldKey.data(), fieldKey.size()))(
                                "placeholder", std::string(key.data(), key.size())));
                return false;
            }
            const auto& value = it->second;
            result.append(value.data(), value.size());
            continue;
        }
    }

    const auto& tail = mStaticParts.back();
    result.append(tail.data(), tail.size());
    return true;
}

bool FormattedString::ParseFormatString(const std::string& formatString) {
    std::unordered_set<std::string> requiredKeySet;

    const size_t length = formatString.size();
    size_t position = 0;

    while (position < length) {
        size_t nextPos = formatString.find("%{", position);
        if (nextPos == std::string::npos) {
            break;
        }

        mStaticParts.emplace_back(StringView(formatString.data() + position, nextPos - position));

        size_t closingBrace = formatString.find('}', nextPos);
        if (closingBrace == std::string::npos) {
            return false;
        }

        const size_t nameStart = nextPos + 2;
        if (nameStart >= closingBrace) {
            return false;
        }

        mPlaceholderNames.emplace_back(StringView(formatString.data() + nameStart, closingBrace - nameStart));

        std::string placeholderName(formatString.data() + nameStart, closingBrace - nameStart);
        if (requiredKeySet.insert(placeholderName).second) {
            mRequiredKeys.emplace_back(std::move(placeholderName));
        }

        position = closingBrace + 1;
    }

    if (position <= length) {
        mStaticParts.emplace_back(StringView(formatString.data() + position, length - position));
    }

    return true;
}

} // namespace logtail

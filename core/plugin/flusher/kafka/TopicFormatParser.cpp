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

#include "plugin/flusher/kafka/TopicFormatParser.h"

#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/RawEvent.h"
#include "models/SpanEvent.h"

namespace logtail {

TopicFormatParser::TopicFormatParser() = default;

TopicFormatParser::~TopicFormatParser() = default;

bool TopicFormatParser::Init(const std::string& formatString) {
    mFormatTemplate = formatString;
    mRequiredFields.clear();
    mStaticParts.clear();
    mPlaceholderPositions.clear();

    return ParseFormatString(formatString);
}

bool TopicFormatParser::FormatTopic(const PipelineEventPtr& event,
                                    std::string& result,
                                    const GroupTags& groupTags) const {
    if (!IsDynamic()) {
        result = mFormatTemplate;
        return true;
    }

    result.clear();

    for (size_t i = 0; i < mRequiredFields.size(); ++i) {
        if (i < mStaticParts.size()) {
            result += mStaticParts[i];
        }

        const auto& fieldRef = mRequiredFields[i];
        std::string fieldValue = ExtractFieldValue(event, fieldRef, groupTags);

        if (fieldValue.empty()) {
            return false;
        }

        result += fieldValue;
    }

    if (!mStaticParts.empty() && mRequiredFields.size() < mStaticParts.size()) {
        result += mStaticParts.back();
    }

    return true;
}

std::string TopicFormatParser::ExtractFieldValue(const PipelineEventPtr& event,
                                                 const FieldRef& fieldRef,
                                                 const GroupTags& groupTags) const {
    switch (fieldRef.type) {
        case FieldType::CONTENT: {
            if (event->GetType() != PipelineEvent::Type::LOG) {
                return "";
            }
            const auto& logEvent = event.Cast<LogEvent>();
            StringView key(fieldRef.fieldName);
            StringView v = logEvent.GetContent(key);
            return v.empty() ? std::string("") : v.to_string();
        }
        case FieldType::TAG: {
            switch (event->GetType()) {
                case PipelineEvent::Type::LOG:
                case PipelineEvent::Type::METRIC:
                case PipelineEvent::Type::SPAN:
                case PipelineEvent::Type::RAW: {
                    if (groupTags.empty())
                        return "";
                    auto it = groupTags.find(StringView(fieldRef.fieldName));
                    return (it != groupTags.end()) ? it->second.to_string() : "";
                }
                default:
                    return "";
            }
        }
        case FieldType::ENV: {
            switch (event->GetType()) {
                case PipelineEvent::Type::LOG:
                case PipelineEvent::Type::METRIC:
                case PipelineEvent::Type::SPAN:
                case PipelineEvent::Type::RAW: {
                    if (groupTags.empty())
                        return "";
                    auto it = groupTags.find(StringView(fieldRef.fieldName));
                    return (it != groupTags.end()) ? it->second.to_string() : "";
                }
                default:
                    return "";
            }
        }
        default:
            return "";
    }
}

bool TopicFormatParser::ParseFormatString(const std::string& formatString) {
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

        std::string placeholder = formatString.substr(placeholderStart + 2, placeholderEnd - placeholderStart - 2);
        FieldRef fieldRef;

        if (placeholderPrefix == "%{") {
            if (placeholder.rfind("content.", 0) == 0) {
                fieldRef.type = FieldType::CONTENT;
                fieldRef.fieldName = placeholder.substr(8);
            } else if (placeholder.rfind("tag.", 0) == 0) {
                fieldRef.type = FieldType::TAG;
                fieldRef.fieldName = placeholder.substr(4);
            } else {
                return false;
            }

            if (fieldRef.fieldName.empty()) {
                return false;
            }
        } else if (placeholderPrefix == "${") {
            fieldRef.type = FieldType::ENV;
            fieldRef.fieldName = placeholder;

            if (fieldRef.fieldName.empty()) {
                return false;
            }
        }

        mRequiredFields.emplace_back(std::move(fieldRef));
        startPos = placeholderEnd + 1;
        nextPos = startPos;
    }

    if (startPos < formatString.length()) {
        mStaticParts.emplace_back(formatString.substr(startPos));
    }

    return true;
}

} // namespace logtail

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

#include "common/TopicFormatParser.h"

#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/RawEvent.h"
#include "models/SpanEvent.h"

namespace logtail {

TopicFormatParser::TopicFormatParser() = default;

TopicFormatParser::~TopicFormatParser() = default;

bool TopicFormatParser::Init(const std::string& formatString) {
    return mFormatter.Init(formatString);
}

bool TopicFormatParser::FormatTopic(const PipelineEventPtr& event,
                                    std::string& result,
                                    const GroupTags& groupTags) const {
    if (!IsDynamic()) {
        result = mFormatter.GetFormatTemplate();
        return true;
    }

    ValueProvider valueProvider = [this, &event, &groupTags](const std::string& placeholder) -> std::string {
        return CreateValueProvider(event, groupTags, placeholder);
    };

    return mFormatter.Format(valueProvider, result);
}

std::string TopicFormatParser::CreateValueProvider(const PipelineEventPtr& event,
                                                   const GroupTags& groupTags,
                                                   const std::string& placeholder) const {
    if (placeholder.rfind("content.", 0) == 0) {
        if (event->GetType() != PipelineEvent::Type::LOG) {
            return "";
        }
        const auto& logEvent = event.Cast<LogEvent>();
        std::string fieldName = placeholder.substr(8);
        StringView key(fieldName);
        StringView v = logEvent.GetContent(key);
        return v.empty() ? std::string("") : v.to_string();
    }

    if (placeholder.rfind("tag.", 0) == 0) {
        switch (event->GetType()) {
            case PipelineEvent::Type::LOG:
            case PipelineEvent::Type::METRIC:
            case PipelineEvent::Type::SPAN:
            case PipelineEvent::Type::RAW: {
                if (groupTags.empty())
                    return "";
                std::string fieldName = placeholder.substr(4);
                auto it = groupTags.find(StringView(fieldName));
                return (it != groupTags.end()) ? it->second.to_string() : "";
            }
            default:
                return "";
        }
    }

    switch (event->GetType()) {
        case PipelineEvent::Type::LOG:
        case PipelineEvent::Type::METRIC:
        case PipelineEvent::Type::SPAN:
        case PipelineEvent::Type::RAW: {
            if (groupTags.empty())
                return "";
            auto it = groupTags.find(StringView(placeholder));
            return (it != groupTags.end()) ? it->second.to_string() : "";
        }
        default:
            return "";
    }
}

} // namespace logtail

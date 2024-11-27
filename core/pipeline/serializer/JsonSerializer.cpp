// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "pipeline/serializer/JsonSerializer.h"

#include "constants/SpanConstants.h"
#include "protobuf/sls/LogGroupSerializer.h"

using namespace std;

namespace logtail {

const string JSON_KEY_TIME = "__time__";

bool JsonEventGroupSerializer::Serialize(BatchedEvents&& group, string& res, string& errorMsg) {
    if (group.mEvents.empty()) {
        errorMsg = "empty event group";
        return false;
    }

    PipelineEvent::Type eventType = group.mEvents[0]->GetType();
    if (eventType == PipelineEvent::Type::NONE) {
        // should not happen
        errorMsg = "unsupported event type in event group";
        return false;
    }

    Json::Value groupTags;
    for (const auto& tag : group.mTags.mInner) {
        groupTags[tag.first.to_string()] = tag.second.to_string();
    }

    std::ostringstream oss;
    switch (eventType) {
        case PipelineEvent::Type::LOG:
            for (size_t i = 0; i < group.mEvents.size(); ++i) {
                const auto& e = group.mEvents[i].Cast<LogEvent>();
                Json::Value eventJson;
                // tags
                eventJson.copy(groupTags);
                // time
                eventJson[JSON_KEY_TIME] = e.GetTimestamp();
                // contents
                for (const auto& kv : e) {
                    eventJson[kv.first.to_string()] = kv.second.to_string();
                }
                Json::StreamWriterBuilder writer;
                writer["indentation"] = "";
                oss << Json::writeString(writer, eventJson);
            }
            break;
        case PipelineEvent::Type::METRIC:
            for (size_t i = 0; i < group.mEvents.size(); ++i) {
                const auto& e = group.mEvents[i].Cast<MetricEvent>();
                if (e.Is<std::monostate>()) {
                    continue;
                }
                Json::Value eventJson;
                // tags
                eventJson.copy(groupTags);
                // time
                eventJson[JSON_KEY_TIME] = e.GetTimestamp();
                // __labels__
                eventJson[METRIC_RESERVED_KEY_LABELS] = Json::Value();
                for (auto tag = e.TagsBegin(); tag != e.TagsEnd(); tag++) {
                    eventJson[METRIC_RESERVED_KEY_LABELS][tag->first.to_string()] = tag->second.to_string();
                }
                // __name__
                eventJson[METRIC_RESERVED_KEY_NAME] = e.GetName().to_string();
                // __value__
                if (e.Is<UntypedSingleValue>()) {
                    eventJson[METRIC_RESERVED_KEY_VALUE] = e.GetValue<UntypedSingleValue>()->mValue;
                } else if (e.Is<UntypedMultiDoubleValues>()) {
                    eventJson[METRIC_RESERVED_KEY_VALUE] = Json::Value();
                    for (auto value = e.GetValue<UntypedMultiDoubleValues>()->ValusBegin();
                         value != e.GetValue<UntypedMultiDoubleValues>()->ValusEnd();
                         value++) {
                        eventJson[METRIC_RESERVED_KEY_VALUE][value->first.to_string()] = value->second;
                    }
                }
                Json::StreamWriterBuilder writer;
                writer["indentation"] = "";
                oss << Json::writeString(writer, eventJson);
            }
            break;
        case PipelineEvent::Type::SPAN:
            LOG_ERROR(
                sLogger,
                ("invalid event type", "span type is not supported")("config", mFlusher->GetContext().GetConfigName()));
            break;
        case PipelineEvent::Type::RAW:
            LOG_ERROR(
                sLogger,
                ("invalid event type", "raw type is not supported")("config", mFlusher->GetContext().GetConfigName()));
            break;
        default:
            break;
    }
    res = oss.str();
    return true;
}

} // namespace logtail
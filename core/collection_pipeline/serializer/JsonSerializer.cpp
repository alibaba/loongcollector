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

#include "collection_pipeline/serializer/JsonSerializer.h"

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "constants/SpanConstants.h"
#include "models/MetricValue.h"
#include "protobuf/sls/LogGroupSerializer.h"

using namespace std;

namespace logtail {

// Helper function to serialize common fields (tags and time)
template <typename WriterType>
void SerializeCommonFields(const SizedMap& tags, uint64_t timestamp, WriterType& writer) {
    // Serialize tags
    for (const auto& tag : tags.mInner) {
        writer.Key(tag.first.to_string().c_str());
        writer.String(tag.second.to_string().c_str());
    }
    // Serialize time
    writer.Key("__time__");
    writer.Uint64(timestamp);
}

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

    // Create reusable StringBuffer and Writer
    rapidjson::StringBuffer jsonBuffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(jsonBuffer);
    auto resetBuffer = [&jsonBuffer, &writer]() {
        jsonBuffer.Clear(); // Clear the buffer for reuse
        writer.Reset(jsonBuffer);
    };

    // TODO: should support nano second
    switch (eventType) {
        case PipelineEvent::Type::LOG:
            for (const auto& item : group.mEvents) {
                const auto& e = item.Cast<LogEvent>();
                if (e.Empty()) {
                    continue;
                }
                resetBuffer();

                writer.StartObject();
                SerializeCommonFields(group.mTags, e.GetTimestamp(), writer);
                // contents
                for (const auto& kv : e) {
                    writer.Key(kv.first.to_string().c_str());
                    writer.String(kv.second.to_string().c_str());
                }
                writer.EndObject();
                res.append(jsonBuffer.GetString());
                res.append("\n");
            }
            break;
        case PipelineEvent::Type::METRIC:
            // TODO: key should support custom key
            for (const auto& item : group.mEvents) {
                const auto& e = item.Cast<MetricEvent>();
                if (e.Is<std::monostate>()) {
                    continue;
                }
                resetBuffer();

                writer.StartObject();
                SerializeCommonFields(group.mTags, e.GetTimestamp(), writer);
                // __labels__
                writer.Key(METRIC_RESERVED_KEY_LABELS.c_str());
                writer.StartObject();
                for (auto tag = e.TagsBegin(); tag != e.TagsEnd(); tag++) {
                    writer.Key(tag->first.to_string().c_str());
                    writer.String(tag->second.to_string().c_str());
                }
                writer.EndObject();
                // __name__
                writer.Key(METRIC_RESERVED_KEY_NAME.c_str());
                writer.String(e.GetName().to_string().c_str());
                // __value__
                writer.Key(METRIC_RESERVED_KEY_VALUE.c_str());
                if (e.Is<UntypedSingleValue>()) {
                    writer.Double(e.GetValue<UntypedSingleValue>()->mValue);
                } else if (e.Is<UntypedMultiDoubleValues>()) {
                    writer.StartObject();
                    for (auto value = e.GetValue<UntypedMultiDoubleValues>()->ValuesBegin();
                         value != e.GetValue<UntypedMultiDoubleValues>()->ValuesEnd();
                         value++) {
                        writer.Key(value->first.to_string().c_str());
                        writer.Double(value->second.Value);
                    }
                    writer.EndObject();
                }
                writer.EndObject();
                res.append(jsonBuffer.GetString());
                res.append("\n");
            }
            break;
        case PipelineEvent::Type::RAW:
            for (const auto& item : group.mEvents) {
                const auto& e = item.Cast<RawEvent>();
                if (e.GetContent().empty()) {
                    continue;
                }
                resetBuffer();

                writer.StartObject();
                SerializeCommonFields(group.mTags, e.GetTimestamp(), writer);
                // content
                writer.Key(DEFAULT_CONTENT_KEY.c_str());
                writer.String(e.GetContent().to_string().c_str());
                writer.EndObject();
                res.append(jsonBuffer.GetString());
                res.append("\n");
            }
            break;
        case PipelineEvent::Type::SPAN:
            for (const auto& item : group.mEvents) {
                const auto& e = item.Cast<SpanEvent>();

                resetBuffer();

                writer.StartObject();
                SerializeCommonFields(group.mTags, e.GetTimestamp(), writer);

                writer.Key(DEFAULT_TRACE_TAG_TRACE_ID.data(), DEFAULT_TRACE_TAG_TRACE_ID.size());
                writer.String(e.GetTraceId().data(), e.GetTraceId().size());
                writer.Key(DEFAULT_TRACE_TAG_SPAN_ID.data(), DEFAULT_TRACE_TAG_SPAN_ID.size());
                writer.String(e.GetSpanId().data(), e.GetSpanId().size());
                writer.Key(DEFAULT_TRACE_TAG_PARENT_ID.data(), DEFAULT_TRACE_TAG_PARENT_ID.size());
                writer.String(e.GetParentSpanId().data(), e.GetParentSpanId().size());
                writer.Key(DEFAULT_TRACE_TAG_SPAN_NAME.data(), DEFAULT_TRACE_TAG_SPAN_NAME.size());
                writer.String(e.GetName().data(), e.GetName().size());

                writer.Key(DEFAULT_TRACE_TAG_START_TIME_NANO.data(), DEFAULT_TRACE_TAG_START_TIME_NANO.size());
                writer.Uint64(e.GetStartTimeNs());
                writer.Key(DEFAULT_TRACE_TAG_END_TIME_NANO.data(), DEFAULT_TRACE_TAG_END_TIME_NANO.size());
                writer.Uint64(e.GetEndTimeNs());
                writer.Key(DEFAULT_TRACE_TAG_DURATION.data(), DEFAULT_TRACE_TAG_DURATION.size());
                writer.Uint64(e.GetEndTimeNs() - e.GetStartTimeNs());

                writer.Key(DEFAULT_TRACE_TAG_ATTRIBUTES.data(), DEFAULT_TRACE_TAG_ATTRIBUTES.size());
                writer.StartObject();
                for (auto it = e.TagsBegin(); it != e.TagsEnd(); ++it) {
                    writer.Key(it->first.data(), it->first.size());
                    writer.String(it->second.data(), it->second.size());
                }
                writer.EndObject();

                writer.Key(DEFAULT_TRACE_TAG_SCOPE.data(), DEFAULT_TRACE_TAG_SCOPE.size());
                writer.StartObject();
                for (auto it = e.ScopeTagsBegin(); it != e.ScopeTagsEnd(); ++it) {
                    writer.Key(it->first.data(), it->first.size());
                    writer.String(it->second.data(), it->second.size());
                }
                writer.EndObject();

                writer.EndObject();
                res.append(jsonBuffer.GetString());
                res.append("\n");
            }
            break;
        default:
            break;
    }
    return !res.empty();
}

} // namespace logtail

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

#include "collection_pipeline/serializer/CustomSingleSerializer.h"

#include <cstdlib>

#include "constants/Constants.h"
#include "logger/Logger.h"
#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/MetricValue.h"
#include "models/RawEvent.h"
#include "models/SpanEvent.h"

using namespace std;

namespace logtail {

const string kProtocolKeyTime = "time";
const string kProtocolKeyContent = "contents";
const string kProtocolKeyTag = "tags";

bool CustomSingleSerializer::Serialize(BatchedEvents&& group, string& res, string& errorMsg) {
    if (group.mEvents.empty()) {
        errorMsg = "empty event group";
        return false;
    }

    res.clear();

    rapidjson::StringBuffer jsonBuffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(jsonBuffer);

    for (const auto& event : group.mEvents) {
        jsonBuffer.Clear();
        writer.Reset(jsonBuffer);

        writer.StartObject();

        writer.Key(kProtocolKeyTime.c_str());
        writer.Uint(event->GetTimestamp());

        writer.Key(kProtocolKeyContent.c_str());
        writer.StartObject();

        switch (event->GetType()) {
            case PipelineEvent::Type::LOG: {
                const auto& logEvent = event.Cast<LogEvent>();
                for (const auto& kv : logEvent) {
                    writer.Key(kv.first.data(), kv.first.size());
                    writer.String(kv.second.data(), kv.second.size());
                }
                break;
            }
            case PipelineEvent::Type::METRIC: {
                const auto& metricEvent = event.Cast<MetricEvent>();
                writer.Key("__name__");
                writer.String(metricEvent.GetName().data(), metricEvent.GetName().size());

                if (metricEvent.Is<UntypedSingleValue>()) {
                    writer.Key("__value__");
                    writer.String(to_string(metricEvent.GetValue<UntypedSingleValue>()->mValue).c_str());
                } else if (metricEvent.Is<UntypedMultiDoubleValues>()) {
                    const auto* multiValue = metricEvent.GetValue<UntypedMultiDoubleValues>();
                    for (auto it = multiValue->ValuesBegin(); it != multiValue->ValuesEnd(); ++it) {
                        writer.Key(it->first.data(), it->first.size());
                        writer.String(to_string(it->second.Value).c_str());
                    }
                }

                writer.Key("labels");
                writer.StartObject();
                for (auto it = metricEvent.TagsBegin(); it != metricEvent.TagsEnd(); ++it) {
                    writer.Key(it->first.data(), it->first.size());
                    writer.String(it->second.data(), it->second.size());
                }
                writer.EndObject();
                break;
            }
            case PipelineEvent::Type::SPAN: {
                const auto& spanEvent = event.Cast<SpanEvent>();
                writer.Key("trace_id");
                writer.String(spanEvent.GetTraceId().data(), spanEvent.GetTraceId().size());
                writer.Key("span_id");
                writer.String(spanEvent.GetSpanId().data(), spanEvent.GetSpanId().size());
                writer.Key("parent_span_id");
                writer.String(spanEvent.GetParentSpanId().data(), spanEvent.GetParentSpanId().size());
                writer.Key("name");
                writer.String(spanEvent.GetName().data(), spanEvent.GetName().size());
                writer.Key("start_time");
                writer.String(to_string(spanEvent.GetStartTimeNs()).c_str());
                writer.Key("end_time");
                writer.String(to_string(spanEvent.GetEndTimeNs()).c_str());
                writer.Key("duration");
                writer.String(to_string(spanEvent.GetEndTimeNs() - spanEvent.GetStartTimeNs()).c_str());


                writer.Key("attributes");
                writer.StartObject();
                for (auto it = spanEvent.TagsBegin(); it != spanEvent.TagsEnd(); ++it) {
                    writer.Key(it->first.data(), it->first.size());
                    writer.String(it->second.data(), it->second.size());
                }
                writer.EndObject();


                writer.Key("scope");
                writer.StartObject();
                for (auto it = spanEvent.ScopeTagsBegin(); it != spanEvent.ScopeTagsEnd(); ++it) {
                    writer.Key(it->first.data(), it->first.size());
                    writer.String(it->second.data(), it->second.size());
                }
                writer.EndObject();
                break;
            }
            case PipelineEvent::Type::RAW: {
                const auto& rawEvent = event.Cast<RawEvent>();
                writer.Key(DEFAULT_CONTENT_KEY.data(), DEFAULT_CONTENT_KEY.size());
                writer.String(rawEvent.GetContent().data(), rawEvent.GetContent().size());
                break;
            }
            default:
                LOG_WARNING(sLogger,
                            ("unsupported event type", "skip event")("type", static_cast<int>(event->GetType())));
                continue;
        }
        writer.EndObject();


        writer.Key(kProtocolKeyTag.c_str());
        writer.StartObject();


        for (const auto& tag : group.mTags.mInner) {
            if (!tag.first.empty()) {
                writer.Key(tag.first.data(), tag.first.size());
                writer.String(tag.second.data(), tag.second.size());
            }
        }

        writer.EndObject();

        writer.EndObject();

        res.append(jsonBuffer.GetString());
        res.append("\n");
    }

    return !res.empty();
}

} // namespace logtail

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

#include "collection_pipeline/serializer/KafkaSerializer.h"

#include <cstdlib>

#include <unordered_map>

#include "constants/Constants.h"
#include "logger/Logger.h"
#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/MetricValue.h"
#include "models/RawEvent.h"
#include "models/SpanEvent.h"

using namespace std;

namespace logtail {

const string PROTOCOL_KEY_TIME = "time";
const string PROTOCOL_KEY_CONTENT = "contents";
const string PROTOCOL_KEY_TAG = "tags";

const string TAG_PREFIX = "__tag__:";

const unordered_map<string, string> TAG_CONVERSION_MAP = {{"__path__", "log.file.path"},
                                                          {"__hostname__", "host.name"},
                                                          {"_node_ip_", "k8s.node.ip"},
                                                          {"_node_name_", "k8s.node.name"},
                                                          {"_namespace_", "k8s.namespace.name"},
                                                          {"_pod_name_", "k8s.pod.name"},
                                                          {"_pod_ip_", "k8s.pod.ip"},
                                                          {"_pod_uid_", "k8s.pod.uid"},
                                                          {"_container_name_", "container.name"},
                                                          {"_container_ip_", "container.ip"},
                                                          {"_image_name_", "container.image.name"}};


const unordered_map<string, string> SPECIAL_TAG_CONVERSION_MAP = {{"_container_name_", "k8s.container.name"},
                                                                  {"_container_ip_", "k8s.container.ip"},
                                                                  {"_image_name_", "k8s.container.image.name"}};

bool KafkaEventGroupSerializer::Serialize(BatchedEvents&& group, string& res, string& errorMsg) {
    if (group.mEvents.empty()) {
        errorMsg = "empty event group";
        return false;
    }

    vector<CustomSingleLogItem> items;
    ConvertToCustomSingleItems(group, items);

    if (items.empty()) {
        errorMsg = "no valid events after conversion";
        return false;
    }

    if (!SerializeToJSON(items, res)) {
        errorMsg = "failed to serialize to JSON";
        return false;
    }

    return true;
}

void KafkaEventGroupSerializer::ConvertToCustomSingleItems(const BatchedEvents& group,
                                                           vector<CustomSingleLogItem>& items) {
    items.reserve(group.mEvents.size());

    for (const auto& event : group.mEvents) {
        CustomSingleLogItem item;

        switch (event->GetType()) {
            case PipelineEvent::Type::LOG:
                ConvertLogEvent(event.Cast<LogEvent>(), item);
                break;
            case PipelineEvent::Type::METRIC:
                ConvertMetricEvent(event.Cast<MetricEvent>(), item);
                break;
            case PipelineEvent::Type::SPAN:
                ConvertSpanEvent(event.Cast<SpanEvent>(), item);
                break;
            case PipelineEvent::Type::RAW:
                ConvertRawEvent(event.Cast<RawEvent>(), item);
                break;
            default:
                LOG_WARNING(sLogger,
                            ("unsupported event type", "skip event")("type", static_cast<int>(event->GetType())));
                continue;
        }

        const bool isK8s = getenv("KUBERNETES_SERVICE_HOST") != nullptr;
        for (const auto& tag : group.mTags.mInner) {
            const string key = tag.first.to_string();
            if (key.empty()) {
                continue;
            }

            string convertedKey = key;
            bool converted = false;

            // Try K8s-specific conversion first
            if (isK8s) {
                auto it = SPECIAL_TAG_CONVERSION_MAP.find(key);
                if (it != SPECIAL_TAG_CONVERSION_MAP.end()) {
                    convertedKey = it->second;
                    converted = true;
                }
            }

            // Try general conversion if not already converted
            if (!converted) {
                auto it = TAG_CONVERSION_MAP.find(key);
                if (it != TAG_CONVERSION_MAP.end()) {
                    convertedKey = it->second;
                }
            }

            if (!convertedKey.empty()) {
                item.tags[convertedKey] = tag.second.to_string();
            }
        }

        items.push_back(std::move(item));
    }
}

void KafkaEventGroupSerializer::ConvertLogEvent(const LogEvent& event, CustomSingleLogItem& item) {
    if (event.Empty()) {
        return;
    }

    item.time = event.GetTimestamp();

    ConvertLogToMaps(event, item.contents, item.tags);
}

void KafkaEventGroupSerializer::ConvertMetricEvent(const MetricEvent& event, CustomSingleLogItem& item) {
    item.time = event.GetTimestamp();

    item.contents["__name__"] = event.GetName().to_string();

    if (event.Is<UntypedSingleValue>()) {
        item.contents["__value__"] = to_string(event.GetValue<UntypedSingleValue>()->mValue);
    } else if (event.Is<UntypedMultiDoubleValues>()) {
        const auto* multiValue = event.GetValue<UntypedMultiDoubleValues>();
        for (auto it = multiValue->ValuesBegin(); it != multiValue->ValuesEnd(); ++it) {
            item.contents[it->first.to_string()] = to_string(it->second.Value);
        }
    }

    for (auto it = event.TagsBegin(); it != event.TagsEnd(); ++it) {
        item.tags[it->first.to_string()] = it->second.to_string();
    }
}

void KafkaEventGroupSerializer::ConvertSpanEvent(const SpanEvent& event, CustomSingleLogItem& item) {
    item.time = event.GetTimestamp();

    item.contents["trace_id"] = event.GetTraceId().to_string();
    item.contents["span_id"] = event.GetSpanId().to_string();
    item.contents["parent_span_id"] = event.GetParentSpanId().to_string();
    item.contents["name"] = event.GetName().to_string();
    item.contents["start_time"] = to_string(event.GetStartTimeNs());
    item.contents["end_time"] = to_string(event.GetEndTimeNs());
    item.contents["duration"] = to_string(event.GetEndTimeNs() - event.GetStartTimeNs());

    // Convert attributes to JSON string
    rapidjson::StringBuffer attrBuffer;
    rapidjson::Writer<rapidjson::StringBuffer> attrWriter(attrBuffer);
    attrWriter.StartObject();
    for (auto it = event.TagsBegin(); it != event.TagsEnd(); ++it) {
        attrWriter.Key(it->first.to_string().c_str());
        attrWriter.String(it->second.to_string().c_str());
    }
    attrWriter.EndObject();
    item.contents["attributes"] = attrBuffer.GetString();

    // Convert scope to JSON string
    rapidjson::StringBuffer scopeBuffer;
    rapidjson::Writer<rapidjson::StringBuffer> scopeWriter(scopeBuffer);
    scopeWriter.StartObject();
    for (auto it = event.ScopeTagsBegin(); it != event.ScopeTagsEnd(); ++it) {
        scopeWriter.Key(it->first.to_string().c_str());
        scopeWriter.String(it->second.to_string().c_str());
    }
    scopeWriter.EndObject();
    item.contents["scope"] = scopeBuffer.GetString();
}

void KafkaEventGroupSerializer::ConvertRawEvent(const RawEvent& event, CustomSingleLogItem& item) {
    item.time = event.GetTimestamp();

    item.contents[DEFAULT_CONTENT_KEY] = event.GetContent().to_string();
}

void KafkaEventGroupSerializer::ConvertLogToMaps(const LogEvent& event,
                                                 map<string, string>& contents,
                                                 map<string, string>& tags) {
    const bool isK8s = getenv("KUBERNETES_SERVICE_HOST") != nullptr;

    for (const auto& kv : event) {
        const string key = kv.first.to_string();
        const string value = kv.second.to_string();

        if (key == "__log_topic__") {
            tags["log.topic"] = value;
            continue;
        }

        if (key.find(TAG_PREFIX) == 0) {
            string tagName = key.substr(TAG_PREFIX.length());


            if (tagName == "__user_defined_id__") {
                continue;
            }

            bool converted = false;
            if (isK8s) {
                auto it = SPECIAL_TAG_CONVERSION_MAP.find(tagName);
                if (it != SPECIAL_TAG_CONVERSION_MAP.end()) {
                    tagName = it->second;
                    converted = true;
                }
            }
            if (!converted) {
                auto it = TAG_CONVERSION_MAP.find(tagName);
                if (it != TAG_CONVERSION_MAP.end()) {
                    tagName = it->second;
                }
            }

            if (!tagName.empty()) {
                tags[tagName] = value;
            }
        } else {
            string newTagName;
            bool converted = false;

            if (isK8s) {
                auto it = SPECIAL_TAG_CONVERSION_MAP.find(key);
                if (it != SPECIAL_TAG_CONVERSION_MAP.end()) {
                    newTagName = it->second;
                    converted = true;
                }
            }
            if (!converted) {
                auto it = TAG_CONVERSION_MAP.find(key);
                if (it != TAG_CONVERSION_MAP.end()) {
                    newTagName = it->second;
                    converted = true;
                }
            }

            if (converted) {
                if (!newTagName.empty()) {
                    tags[newTagName] = value;
                }
            } else {
                contents[key] = value;
            }
        }
    }
}


bool KafkaEventGroupSerializer::SerializeToJSON(const vector<CustomSingleLogItem>& items, string& output) {
    output.clear();
    output.reserve(items.size() * 512);

    rapidjson::StringBuffer jsonBuffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(jsonBuffer);

    auto resetBuffer = [&jsonBuffer, &writer]() {
        jsonBuffer.Clear();
        writer.Reset(jsonBuffer);
    };

    for (const auto& item : items) {
        resetBuffer();

        writer.StartObject();

        writer.Key(PROTOCOL_KEY_TIME.c_str());
        writer.Uint(item.time);

        writer.Key(PROTOCOL_KEY_CONTENT.c_str());
        writer.StartObject();
        for (const auto& kv : item.contents) {
            writer.Key(kv.first.c_str());
            writer.String(kv.second.c_str());
        }
        writer.EndObject();

        writer.Key(PROTOCOL_KEY_TAG.c_str());
        writer.StartObject();
        for (const auto& kv : item.tags) {
            writer.Key(kv.first.c_str());
            writer.String(kv.second.c_str());
        }
        writer.EndObject();

        writer.EndObject();

        output.append(jsonBuffer.GetString());
        output.append("\n");
    }

    return true;
}

} // namespace logtail

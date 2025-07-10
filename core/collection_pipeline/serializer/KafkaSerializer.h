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

#pragma once

#include <map>
#include <string>
#include <vector>

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "collection_pipeline/serializer/Serializer.h"

namespace logtail {

struct CustomSingleLogItem {
    uint32_t time;
    std::map<std::string, std::string> contents;
    std::map<std::string, std::string> tags;
};

class KafkaEventGroupSerializer : public EventGroupSerializer {
public:
    KafkaEventGroupSerializer(Flusher* f) : EventGroupSerializer(f) {}

private:
    bool Serialize(BatchedEvents&& group, std::string& res, std::string& errorMsg) override;

    void ConvertToCustomSingleItems(const BatchedEvents& group, std::vector<CustomSingleLogItem>& items);
    bool SerializeToJSON(const std::vector<CustomSingleLogItem>& items, std::string& output);

    void ConvertLogEvent(const LogEvent& event, CustomSingleLogItem& item);
    void ConvertMetricEvent(const MetricEvent& event, CustomSingleLogItem& item);
    void ConvertSpanEvent(const SpanEvent& event, CustomSingleLogItem& item);
    void ConvertRawEvent(const RawEvent& event, CustomSingleLogItem& item);

    void ConvertLogToMaps(const LogEvent& event,
                          std::map<std::string, std::string>& contents,
                          std::map<std::string, std::string>& tags);
};

} // namespace logtail

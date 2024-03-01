/*
 * Copyright 2023 iLogtail Authors
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

#include "models/MetricEvent.h"

namespace logtail {

MetricEvent::MetricEvent(PipelineEventGroup* ptr) : PipelineEvent(Type::METRIC, ptr) {
}

uint64_t MetricEvent::EventsSizeBytes() {
    // TODO
    return 0;
}

#ifdef APSARA_UNIT_TEST_MAIN
Json::Value MetricEvent::ToJson() const {
    Json::Value root;
    root["type"] = static_cast<int>(GetType());
    root["timestamp"] = GetTimestamp();
    root["timestampNanosecond"] = GetTimestampNanosecond();
    return root;
}

bool MetricEvent::FromJson(const Json::Value& root) {
    if (root.isMember("timestampNanosecond")) {
        SetTimestamp(root["timestamp"].asInt64(), root["timestampNanosecond"].asInt64());
    } else {
        SetTimestamp(root["timestamp"].asInt64());
    }
    return true;
}
#endif

} // namespace logtail

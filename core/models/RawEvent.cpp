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

#include "models/RawEvent.h"

using namespace std;

namespace logtail {

RawEvent::RawEvent(PipelineEventGroup* ptr) : PipelineEvent(Type::RAW, ptr) {
}

unique_ptr<PipelineEvent> RawEvent::Copy() const {
    return make_unique<RawEvent>(*this);
}

void RawEvent::Reset() {
    PipelineEvent::Reset();
    mContent.clear();
}

void RawEvent::SetContent(const std::string& content) {
    SetContentNoCopy(GetSourceBuffer()->CopyString(content));
}

void RawEvent::SetContentNoCopy(StringView content) {
    mContent = content;
}

void RawEvent::SetContentNoCopy(const StringBuffer& content) {
    mContent = StringView(content.data, content.size);
}

size_t RawEvent::DataSize() const {
    return PipelineEvent::DataSize() + mContent.size();
}

#ifdef APSARA_UNIT_TEST_MAIN
Json::Value RawEvent::ToJson(bool enableEventMeta) const {
    Json::Value root;
    root["type"] = static_cast<int>(GetType());
    root["timestamp"] = GetTimestamp();
    if (GetTimestampNanosecond()) {
        root["timestampNanosecond"] = static_cast<int32_t>(GetTimestampNanosecond().value());
    }
    root["content"] = mContent.to_string();
    return root;
}

bool RawEvent::FromJson(const Json::Value& root) {
    if (root.isMember("timestampNanosecond")) {
        SetTimestamp(root["timestamp"].asInt64(), root["timestampNanosecond"].asInt64());
    } else {
        SetTimestamp(root["timestamp"].asInt64());
    }
    if (root.isMember("content")) {
        SetContent(root["content"].asString());
    }
    return true;
}
#endif

} // namespace logtail

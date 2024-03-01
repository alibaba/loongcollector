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

#include "models/LogEvent.h"

namespace logtail {

LogEvent::LogEvent(PipelineEventGroup* ptr) : PipelineEvent(Type::LOG, ptr) {
}

void LogEvent::SetContent(const StringView& key, const StringView& val) {
    SetContentNoCopy(GetSourceBuffer()->CopyString(key), GetSourceBuffer()->CopyString(val));
}

void LogEvent::SetContent(const std::string& key, const std::string& val) {
    SetContentNoCopy(GetSourceBuffer()->CopyString(key), GetSourceBuffer()->CopyString(val));
}

void LogEvent::SetContent(const StringBuffer& key, const StringView& val) {
    SetContentNoCopy(key, GetSourceBuffer()->CopyString(val));
}

void LogEvent::SetContentNoCopy(const StringBuffer& key, const StringBuffer& val) {
    SetContentNoCopy(StringView(key.data, key.size), StringView(val.data, val.size));
}

const StringView& LogEvent::GetContent(const StringView& key) const {
    auto it = contents.find(key);
    if (it != contents.end()) {
        return it->second;
    }
    return gEmptyStringView;
}

bool LogEvent::HasContent(const StringView& key) const {
    return contents.find(key) != contents.end();
}

void LogEvent::SetContentNoCopy(const StringView& key, const StringView& val) {
    contents[key] = val;
}

void LogEvent::DelContent(const StringView& key) {
    contents.erase(key);
}

uint64_t LogEvent::EventsSizeBytes() {
    // TODO
    return 0;
}

#ifdef APSARA_UNIT_TEST_MAIN
Json::Value LogEvent::ToJson() const {
    Json::Value root;
    root["type"] = static_cast<int>(GetType());
    root["timestamp"] = GetTimestamp();
    root["timestampNanosecond"] = GetTimestampNanosecond();
    if (!GetContents().empty()) {
        Json::Value contents;
        for (const auto& content : this->GetContents()) {
            contents[content.first.to_string()] = content.second.to_string();
        }
        root["contents"] = std::move(contents);
    }
    return root;
}

bool LogEvent::FromJson(const Json::Value& root) {
    if (root.isMember("timestampNanosecond")) {
        SetTimestamp(root["timestamp"].asInt64(), root["timestampNanosecond"].asInt64());
    } else {
        SetTimestamp(root["timestamp"].asInt64());
    }
    if (root.isMember("contents")) {
        Json::Value contents = root["contents"];
        for (const auto& key : contents.getMemberNames()) {
            SetContent(key, contents[key].asString());
        }
    }
    return true;
}
#endif

} // namespace logtail

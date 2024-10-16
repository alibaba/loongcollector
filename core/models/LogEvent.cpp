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

using namespace std;

namespace logtail {

LogEvent::LogEvent(PipelineEventGroup* ptr) : PipelineEvent(Type::LOG, ptr) {
}

unique_ptr<PipelineEvent> LogEvent::Copy() const {
    return make_unique<LogEvent>(*this);
}

StringView LogEvent::GetContent(StringView key) const {
    auto it = mIndex.find(key);
    if (it != mIndex.end()) {
        return mContents[it->second].first.second;
    }
    return gEmptyStringView;
}

bool LogEvent::HasContent(StringView key) const {
    return mIndex.find(key) != mIndex.end();
}

void LogEvent::SetContent(StringView key, StringView val) {
    SetContentNoCopy(GetSourceBuffer()->CopyString(key), GetSourceBuffer()->CopyString(val));
}

void LogEvent::SetContent(const string& key, const string& val) {
    SetContentNoCopy(GetSourceBuffer()->CopyString(key), GetSourceBuffer()->CopyString(val));
}

void LogEvent::SetContent(const StringBuffer& key, StringView val) {
    SetContentNoCopy(key, GetSourceBuffer()->CopyString(val));
}

void LogEvent::SetContentNoCopy(const StringBuffer& key, const StringBuffer& val) {
    SetContentNoCopy(StringView(key.data, key.size), StringView(val.data, val.size));
}

void LogEvent::SetContentNoCopy(StringView key, StringView val) {
    auto it = mIndex.find(key);
    if (it != mIndex.end()) {
        auto& field = mContents[it->second].first;
        mAllocatedContentSize += key.size() + val.size() - field.first.size() - field.second.size();
        field = make_pair(key, val);
    } else {
        mAllocatedContentSize += key.size() + val.size();
        mContents.emplace_back(make_pair(key, val), true);
        mIndex[key] = mContents.size() - 1;
    }
}

void LogEvent::DelContent(StringView key) {
    auto it = mIndex.find(key);
    if (it != mIndex.end()) {
        auto& field = mContents[it->second].first;
        mAllocatedContentSize -= field.first.size() + field.second.size();
        mContents[it->second].second = false;
        mIndex.erase(it);
    }
}

LogEvent::ContentIterator LogEvent::FindContent(StringView key) {
    auto it = mIndex.find(key);
    if (it != mIndex.end()) {
        return ContentIterator(mContents.begin() + it->second, mContents);
    }
    return ContentIterator(mContents.end(), mContents);
}

LogEvent::ConstContentIterator LogEvent::FindContent(StringView key) const {
    auto it = mIndex.find(key);
    if (it != mIndex.end()) {
        return ConstContentIterator(mContents.begin() + it->second, mContents);
    }
    return ConstContentIterator(mContents.end(), mContents);
}

LogEvent::ContentIterator LogEvent::begin() {
    auto it = mContents.begin();
    while (it != mContents.end() && !it->second) {
        ++it;
    }
    return ContentIterator(it, mContents);
}

LogEvent::ContentIterator LogEvent::end() {
    mContents.end() == mContents.end();
    return ContentIterator(mContents.end(), mContents);
}

LogEvent::ConstContentIterator LogEvent::begin() const {
    return cbegin();
}

LogEvent::ConstContentIterator LogEvent::end() const {
    return cend();
}

LogEvent::ConstContentIterator LogEvent::cbegin() const {
    auto it = mContents.cbegin();
    while (it != mContents.cend() && !it->second) {
        ++it;
    }
    return ConstContentIterator(it, mContents);
}

LogEvent::ConstContentIterator LogEvent::cend() const {
    return ConstContentIterator(mContents.cend(), mContents);
}

void LogEvent::AppendContentNoCopy(StringView key, StringView val) {
    mAllocatedContentSize += key.size() + val.size();
    mContents.emplace_back(make_pair(key, val), true);
    mIndex[key] = mContents.size() - 1;
}

size_t LogEvent::DataSize() const {
    return PipelineEvent::DataSize() + sizeof(decltype(mContents)) + mAllocatedContentSize;
}

#ifdef APSARA_UNIT_TEST_MAIN
Json::Value LogEvent::ToJson(bool enableEventMeta) const {
    Json::Value root;
    root["type"] = static_cast<int>(GetType());
    root["timestamp"] = GetTimestamp();
    if (GetTimestampNanosecond()) {
        root["timestampNanosecond"] = static_cast<int32_t>(GetTimestampNanosecond().value());
    }
    if (enableEventMeta) {
        root["fileOffset"] = GetPosition().first;
        root["rawSize"] = GetPosition().second;
    }
    if (!Empty()) {
        Json::Value contents;
        for (const auto& content : *this) {
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
    if (root.isMember("fileOffset") && root.isMember("rawSize")) {
        SetPosition(root["fileOffset"].asUInt64(), root["rawSize"].asUInt64());
    }
    if (root.isMember("contents")) {
        Json::Value contents = root["contents"];
        for (const auto& key : contents.getMemberNames()) {
            // 单测需要，每个key需要独立的内存空间
            SetContent(GetSourceBuffer()->CopyString(key), contents[key].asString());
        }
    }
    return true;
}
#endif

} // namespace logtail

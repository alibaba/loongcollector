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

#include "models/PipelineEventGroup.h"

#include <sstream>

#include "logger/Logger.h"

namespace logtail {

void PipelineEventGroup::AddEvent(const PipelineEventPtr& event) {
    mEvents.emplace_back(event);
}

void PipelineEventGroup::AddEvent(std::unique_ptr<PipelineEvent>&& event) {
    mEvents.emplace_back(std::move(event));
}

void PipelineEventGroup::SetMetadata(const StringView& key, const StringView& val) {
    SetMetadataNoCopy(mSourceBuffer->CopyString(key), mSourceBuffer->CopyString(val));
}

void PipelineEventGroup::SetMetadata(const std::string& key, const std::string& val) {
    SetMetadataNoCopy(mSourceBuffer->CopyString(key), mSourceBuffer->CopyString(val));
}

void PipelineEventGroup::SetMetadataNoCopy(const StringBuffer& key, const StringBuffer& val) {
    SetMetadataNoCopy(StringView(key.data, key.size), StringView(val.data, val.size));
}

bool PipelineEventGroup::HasMetadata(const StringView& key) const {
    return mGroup.metadata.find(key) != mGroup.metadata.end();
}
void PipelineEventGroup::SetMetadataNoCopy(const StringView& key, const StringView& val) {
    mGroup.metadata[key] = val;
}

const StringView& PipelineEventGroup::GetMetadata(const StringView& key) const {
    auto it = mGroup.metadata.find(key);
    if (it != mGroup.metadata.end()) {
        return it->second;
    }
    return gEmptyStringView;
}

void PipelineEventGroup::DelMetadata(const StringView& key) {
    mGroup.metadata.erase(key);
}

void PipelineEventGroup::SetTag(const StringView& key, const StringView& val) {
    SetTagNoCopy(mSourceBuffer->CopyString(key), mSourceBuffer->CopyString(val));
}

void PipelineEventGroup::SetTag(const std::string& key, const std::string& val) {
    SetTagNoCopy(mSourceBuffer->CopyString(key), mSourceBuffer->CopyString(val));
}

void PipelineEventGroup::SetTagNoCopy(const StringBuffer& key, const StringBuffer& val) {
    SetTagNoCopy(StringView(key.data, key.size), StringView(val.data, val.size));
}

bool PipelineEventGroup::HasTag(const StringView& key) const {
    return mGroup.tags.find(key) != mGroup.tags.end();
}

void PipelineEventGroup::SetTagNoCopy(const StringView& key, const StringView& val) {
    mGroup.tags[key] = val;
}

const StringView& PipelineEventGroup::GetTag(const StringView& key) const {
    auto it = mGroup.tags.find(key);
    if (it != mGroup.tags.end()) {
        return it->second;
    }
    return gEmptyStringView;
}

void PipelineEventGroup::DelTag(const StringView& key) {
    mGroup.tags.erase(key);
}

Json::Value PipelineEventGroup::ToJson() const {
    Json::Value root;
    const auto& groupInfo = this->GetGroupInfo();
    if (!groupInfo.metadata.empty()) {
        Json::Value metadata;
        for (const auto& meta : groupInfo.metadata) {
            metadata[meta.first.to_string()] = meta.second.to_string();
        }
        root["metadata"] = metadata;
    }
    if (!groupInfo.tags.empty()) {
        Json::Value tags;
        for (const auto& tag : groupInfo.tags) {
            tags[tag.first.to_string()] = tag.second.to_string();
        }
        root["tags"] = tags;
    }
    if (!this->GetEvents().empty()) {
        Json::Value events;
        for (const auto& event : this->GetEvents()) {
            events.append(event->ToJson());
        }
        root["events"] = std::move(events);
    }
    return root;
}

bool PipelineEventGroup::FromJson(const Json::Value& root) {
    if (root.isMember("metadata")) {
        Json::Value metadata = root["metadata"];
        for (const auto& key : metadata.getMemberNames()) {
            SetMetadata(key, metadata[key].asString());
        }
    }
    if (root.isMember("tags")) {
        Json::Value tags = root["tags"];
        for (const auto& key : tags.getMemberNames()) {
            SetTag(key, tags[key].asString());
        }
    }
    if (root.isMember("events")) {
        Json::Value events = root["events"];
        for (const auto& event : events) {
            PipelineEventPtr eventPtr;
            if (event["type"].asInt() == LOG_EVENT_TYPE) {
                eventPtr = LogEvent::CreateEvent(GetSourceBuffer());
            } else if (event["type"].asInt() == METRIC_EVENT_TYPE) {
                eventPtr = MetricEvent::CreateEvent(GetSourceBuffer());
            } else {
                eventPtr = SpanEvent::CreateEvent(GetSourceBuffer());
            }
            eventPtr->FromJson(event);
            AddEvent(eventPtr);
        }
    }
    return true;
}

std::string PipelineEventGroup::ToJsonString() const {
    Json::Value root = ToJson();
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    std::ostringstream oss;
    writer->write(root, &oss);
    return oss.str();
}

bool PipelineEventGroup::FromJsonString(const std::string& inJson) {
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string errs;
    Json::Value root;
    if (!reader->parse(inJson.data(), inJson.data() + inJson.size(), &root, &errs)) {
        LOG_ERROR(sLogger, ("build PipelineEventGroup FromJsonString error", errs));
        return false;
    }
    return FromJson(root);
}

} // namespace logtail
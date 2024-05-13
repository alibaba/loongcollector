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

#include "processor/inner/ProcessorParseContainerLogNative.h"

#include <sstream>

#include "logger/Logger.h"

namespace logtail {

PipelineEventGroup::PipelineEventGroup(PipelineEventGroup&& rhs) noexcept
    : mMetadata(std::move(rhs.mMetadata)),
      mTags(std::move(rhs.mTags)),
      mEvents(std::move(rhs.mEvents)),
      mSourceBuffer(std::move(rhs.mSourceBuffer)) {
    for (auto& item : mEvents) {
        item->ResetPipelineEventGroup(this);
    }
}

PipelineEventGroup& PipelineEventGroup::operator=(PipelineEventGroup&& rhs) noexcept {
    if (this != &rhs) {
        mMetadata = std::move(rhs.mMetadata);
        mTags = std::move(rhs.mTags);
        mEvents = std::move(rhs.mEvents);
        mSourceBuffer = std::move(rhs.mSourceBuffer);
        for (auto& item : mEvents) {
            item->ResetPipelineEventGroup(this);
        }
    }
    return *this;
}

std::unique_ptr<LogEvent> PipelineEventGroup::CreateLogEvent() {
    return std::unique_ptr<LogEvent>(new LogEvent(this));
}

std::unique_ptr<MetricEvent> PipelineEventGroup::CreateMetricEvent() {
    return std::unique_ptr<MetricEvent>(new MetricEvent(this));
}

std::unique_ptr<SpanEvent> PipelineEventGroup::CreateSpanEvent() {
    return std::unique_ptr<SpanEvent>(new SpanEvent(this));
}

LogEvent* PipelineEventGroup::AddLogEvent() {
    LogEvent* e = new LogEvent(this);
    mEvents.emplace_back(e);
    return e;
}

MetricEvent* PipelineEventGroup::AddMetricEvent() {
    MetricEvent* e = new MetricEvent(this);
    mEvents.emplace_back(e);
    return e;
}

SpanEvent* PipelineEventGroup::AddSpanEvent() {
    SpanEvent* e = new SpanEvent(this);
    mEvents.emplace_back(e);
    return e;
}

void PipelineEventGroup::SetMetadata(EventGroupMetaKey key, StringView val) {
    SetMetadataNoCopy(key, mSourceBuffer->CopyString(val));
}

void PipelineEventGroup::SetMetadata(EventGroupMetaKey key, const std::string& val) {
    SetMetadataNoCopy(key, mSourceBuffer->CopyString(val));
}

void PipelineEventGroup::SetMetadataNoCopy(EventGroupMetaKey key, const StringBuffer& val) {
    SetMetadataNoCopy(key, StringView(val.data, val.size));
}

bool PipelineEventGroup::HasMetadata(EventGroupMetaKey key) const {
    return mMetadata.find(key) != mMetadata.end();
}
void PipelineEventGroup::SetMetadataNoCopy(EventGroupMetaKey key, StringView val) {
    mMetadata[key] = val;
}

StringView PipelineEventGroup::GetMetadata(EventGroupMetaKey key) const {
    auto it = mMetadata.find(key);
    if (it != mMetadata.end()) {
        return it->second;
    }
    return gEmptyStringView;
}

void PipelineEventGroup::DelMetadata(EventGroupMetaKey key) {
    mMetadata.erase(key);
}

void PipelineEventGroup::SetTag(StringView key, StringView val) {
    SetTagNoCopy(mSourceBuffer->CopyString(key), mSourceBuffer->CopyString(val));
}

void PipelineEventGroup::SetTag(const std::string& key, const std::string& val) {
    SetTagNoCopy(mSourceBuffer->CopyString(key), mSourceBuffer->CopyString(val));
}

void PipelineEventGroup::SetTag(const StringBuffer& key, StringView val) {
    SetTagNoCopy(key, mSourceBuffer->CopyString(val));
}

void PipelineEventGroup::SetTagNoCopy(const StringBuffer& key, const StringBuffer& val) {
    SetTagNoCopy(StringView(key.data, key.size), StringView(val.data, val.size));
}

bool PipelineEventGroup::HasTag(StringView key) const {
    return mTags.find(key) != mTags.end();
}

void PipelineEventGroup::SetTagNoCopy(StringView key, StringView val) {
    mTags[key] = val;
}

StringView PipelineEventGroup::GetTag(StringView key) const {
    auto it = mTags.find(key);
    if (it != mTags.end()) {
        return it->second;
    }
    return gEmptyStringView;
}

void PipelineEventGroup::DelTag(StringView key) {
    mTags.erase(key);
}

uint64_t PipelineEventGroup::EventGroupSizeBytes() {
    // TODO
    return 0;
}

#ifdef APSARA_UNIT_TEST_MAIN
// const std::string EVENT_GROUP_META_AGENT_TAG = "agent.tag";
// const std::string EVENT_GROUP_META_HOST_IP = "host.ip";
// const std::string EVENT_GROUP_META_HOST_NAME = "host.name";
// const std::string EVENT_GROUP_META_LOG_TOPIC = "log.topic";
const std::string EVENT_GROUP_META_LOG_FILE_PATH = "log.file.path";
const std::string EVENT_GROUP_META_LOG_FILE_PATH_RESOLVED = "log.file.path_resolved";
const std::string EVENT_GROUP_META_LOG_FILE_INODE = "log.file.inode";
// const std::string EVENT_GROUP_META_LOG_FILE_OFFSET = "log.file.offset";
// const std::string EVENT_GROUP_META_LOG_FILE_LENGTH = "log.file.length";
const std::string EVENT_GROUP_META_CONTAINER_TYPE = "container.type";
const std::string EVENT_GROUP_META_HAS_PART_LOG = "has.part.log";

const std::string EVENT_GROUP_META_K8S_CLUSTER_ID = "k8s.cluster.id";
const std::string EVENT_GROUP_META_K8S_NODE_NAME = "k8s.node.name";
const std::string EVENT_GROUP_META_K8S_NODE_IP = "k8s.node.ip";
const std::string EVENT_GROUP_META_K8S_NAMESPACE = "k8s.namespace.name";
const std::string EVENT_GROUP_META_K8S_POD_UID = "k8s.pod.uid";
const std::string EVENT_GROUP_META_K8S_POD_NAME = "k8s.pod.name";
const std::string EVENT_GROUP_META_CONTAINER_NAME = "container.name";
const std::string EVENT_GROUP_META_CONTAINER_IP = "container.ip";
const std::string EVENT_GROUP_META_CONTAINER_IMAGE_NAME = "container.image.name";
const std::string EVENT_GROUP_META_CONTAINER_IMAGE_ID = "container.image.id";

const std::string EVENT_GROUP_META_CONTAINERD_TEXT = "containerd_text";
const std::string EVENT_GROUP_META_DOCKER_JSON_FILE = "docker_json-file";
const std::string EVENT_GROUP_META_SOURCE_ID = "source.id";
const std::string EVENT_GROUP_META_TOPIC = "topic";

const std::string& EventGroupMetaKeyToString(EventGroupMetaKey key) {
    switch (key) {
        case EventGroupMetaKey::LOG_FILE_PATH:
            return EVENT_GROUP_META_LOG_FILE_PATH;
        case EventGroupMetaKey::LOG_FILE_PATH_RESOLVED:
            return EVENT_GROUP_META_LOG_FILE_PATH_RESOLVED;
        case EventGroupMetaKey::LOG_FILE_INODE:
            return EVENT_GROUP_META_LOG_FILE_INODE;
        case EventGroupMetaKey::SOURCE_ID:
            return EVENT_GROUP_META_SOURCE_ID;
        case EventGroupMetaKey::TOPIC:
            return EVENT_GROUP_META_TOPIC;
        case EventGroupMetaKey::LOG_FORMAT:
            return EVENT_GROUP_META_CONTAINER_TYPE;
        case EventGroupMetaKey::HAS_PART_LOG:
            return EVENT_GROUP_META_HAS_PART_LOG;
        default:
            static std::string sEmpty = "unknown";
            return sEmpty;
    }
}

const std::string EventGroupMetaValueToString(std::string value) {
    if (value == ProcessorParseContainerLogNative::CONTAINERD_TEXT) {
        return EVENT_GROUP_META_CONTAINERD_TEXT;
    } else if (value == ProcessorParseContainerLogNative::DOCKER_JSON_FILE) {
        return EVENT_GROUP_META_DOCKER_JSON_FILE;
    }
    return value;
}

EventGroupMetaKey StringToEventGroupMetaKey(const std::string& key) {
    static std::unordered_map<std::string, EventGroupMetaKey> sStringToEnum{
        {EVENT_GROUP_META_LOG_FILE_PATH, EventGroupMetaKey::LOG_FILE_PATH},
        {EVENT_GROUP_META_LOG_FILE_PATH_RESOLVED, EventGroupMetaKey::LOG_FILE_PATH_RESOLVED},
        {EVENT_GROUP_META_LOG_FILE_INODE, EventGroupMetaKey::LOG_FILE_INODE},
        {EVENT_GROUP_META_SOURCE_ID, EventGroupMetaKey::SOURCE_ID},
        {EVENT_GROUP_META_TOPIC, EventGroupMetaKey::TOPIC},
           {EVENT_GROUP_META_HAS_PART_LOG, EventGroupMetaKey::HAS_PART_LOG}};
    auto it = sStringToEnum.find(key);
    if (it != sStringToEnum.end()) {
        return it->second;
    }
    return EventGroupMetaKey::UNKNOWN;
}

Json::Value PipelineEventGroup::ToJson(bool enableEventMeta) const {
    Json::Value root;
    if (!mMetadata.empty()) {
        Json::Value metadata;
        for (const auto& meta : mMetadata) {
            metadata[EventGroupMetaKeyToString(meta.first)] = EventGroupMetaValueToString(meta.second.to_string());
        }
        root["metadata"] = metadata;
    }
    if (!mTags.empty()) {
        Json::Value tags;
        for (const auto& tag : mTags) {
            tags[tag.first.to_string()] = tag.second.to_string();
        }
        root["tags"] = tags;
    }
    if (!this->GetEvents().empty()) {
        Json::Value events;
        for (const auto& event : this->GetEvents()) {
            events.append(event->ToJson(enableEventMeta));
        }
        root["events"] = std::move(events);
    }
    return root;
}

bool PipelineEventGroup::FromJson(const Json::Value& root) {
    if (root.isMember("metadata")) {
        Json::Value metadata = root["metadata"];
        for (const auto& key : metadata.getMemberNames()) {
            SetMetadata(StringToEventGroupMetaKey(key), metadata[key].asString());
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
            if (event["type"].asInt() == static_cast<int>(PipelineEvent::Type::LOG)) {
                AddLogEvent()->FromJson(event);
            } else if (event["type"].asInt() == static_cast<int>(PipelineEvent::Type::METRIC)) {
                AddMetricEvent()->FromJson(event);
            } else {
                AddSpanEvent()->FromJson(event);
            }
        }
    }
    return true;
}

std::string PipelineEventGroup::ToJsonString(bool enableEventMeta) const {
    Json::Value root = ToJson(enableEventMeta);
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
#endif

} // namespace logtail

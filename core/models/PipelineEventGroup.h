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

#pragma once

#include <memory>
#include <string>

#include "common/Constants.h"
#include "models/PipelineEventPtr.h"
#include "common/memory/SourceBuffer.h"

namespace logtail {

// referrences
// https://opentelemetry.io/docs/specs/otel/logs/data-model-appendix/#elastic-common-schema
// https://github.com/open-telemetry/semantic-conventions/blob/main/docs/resource/README.md
// https://github.com/open-telemetry/semantic-conventions/blob/main/docs/general/logs.md
// https://github.com/open-telemetry/semantic-conventions/blob/main/docs/resource/k8s.md
// https://github.com/open-telemetry/semantic-conventions/blob/main/docs/resource/container.md
enum class EventGroupMetaKey {
    UNKNOWN,
    AGENT_TAG,
    HOST_IP,
    HOST_NAME,
    LOG_TOPIC,
    LOG_FILE_PATH,
    LOG_FILE_PATH_RESOLVED,
    LOG_FILE_INODE,
    LOG_READ_OFFSET,
    LOG_READ_LENGTH,
    LOG_FORMAT,
    HAS_PART_LOG,

    K8S_CLUSTER_ID,
    K8S_NODE_NAME,
    K8S_NODE_IP,
    K8S_NAMESPACE,
    K8S_POD_UID,
    K8S_POD_NAME,
    CONTAINER_NAME,
    CONTAINER_IP,
    CONTAINER_IMAGE_NAME,
    CONTAINER_IMAGE_ID
};

using GroupMetadata = std::map<EventGroupMetaKey, StringView>;
using GroupTags = std::map<StringView, StringView>;

// DeepCopy is required if we want to support no-linear topology
// We cannot just use default copy constructor as it won't deep copy PipelineEvent pointed in Events vector.
using EventsContainer = std::vector<PipelineEventPtr>;

class PipelineEventGroup {
public:
    PipelineEventGroup(std::shared_ptr<SourceBuffer> sourceBuffer) : mSourceBuffer(sourceBuffer) {}
    PipelineEventGroup(const PipelineEventGroup&) = delete;
    PipelineEventGroup& operator=(const PipelineEventGroup&) = delete;
    PipelineEventGroup(PipelineEventGroup&&) noexcept;
    PipelineEventGroup& operator=(PipelineEventGroup&&) noexcept;

    std::unique_ptr<LogEvent> CreateLogEvent();
    std::unique_ptr<MetricEvent> CreateMetricEvent();
    std::unique_ptr<SpanEvent> CreateSpanEvent();

    const EventsContainer& GetEvents() const { return mEvents; }
    EventsContainer& MutableEvents() { return mEvents; }
    LogEvent* AddLogEvent();
    MetricEvent* AddMetricEvent();
    SpanEvent* AddSpanEvent();
    void SwapEvents(EventsContainer& other) { mEvents.swap(other); }
    // void SetSourceBuffer(std::shared_ptr<SourceBuffer> sourceBuffer) { mSourceBuffer = sourceBuffer; }
    std::shared_ptr<SourceBuffer>& GetSourceBuffer() { return mSourceBuffer; }

    void SetMetadata(EventGroupMetaKey key, const StringView& val);
    void SetMetadata(EventGroupMetaKey key, const std::string& val);
    void SetMetadataNoCopy(EventGroupMetaKey key, const StringBuffer& val);
    const StringView& GetMetadata(EventGroupMetaKey key) const;
    const GroupMetadata& GetAllMetadata() const { return mMetadata; };
    bool HasMetadata(EventGroupMetaKey key) const;
    void SetMetadataNoCopy(EventGroupMetaKey key, const StringView& val);
    void DelMetadata(EventGroupMetaKey key);
    GroupMetadata& MutableAllMetadata() { return mMetadata; };
    void SwapAllMetadata(GroupMetadata& other) { mMetadata.swap(other); }
    void SetAllMetadata(const GroupMetadata& other) { mMetadata = other; }

    void SetTag(const StringView& key, const StringView& val);
    void SetTag(const std::string& key, const std::string& val);
    void SetTag(const StringBuffer& key, const StringView& val);
    void SetTagNoCopy(const StringBuffer& key, const StringBuffer& val);
    const StringView& GetTag(const StringView& key) const;
    const GroupTags& GetTags() const { return mTags; };
    bool HasTag(const StringView& key) const;
    void SetTagNoCopy(const StringView& key, const StringView& val);
    void DelTag(const StringView& key);
    GroupTags& MutableTags() { return mTags; };
    void SwapTags(GroupTags& other) { mTags.swap(other); }

    uint64_t EventGroupSizeBytes();

#ifdef APSARA_UNIT_TEST_MAIN
    // for debug and test
    Json::Value ToJson() const;
    bool FromJson(const Json::Value&);
    std::string ToJsonString() const;
    bool FromJsonString(const std::string&);
#endif

private:
    GroupMetadata mMetadata; // Used to generate tag/log. Will not output.
    GroupTags mTags; // custom tags to output
    EventsContainer mEvents;
    std::shared_ptr<SourceBuffer> mSourceBuffer;
};

} // namespace logtail

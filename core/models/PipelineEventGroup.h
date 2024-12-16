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

#include "checkpoint/RangeCheckpoint.h"
#include "constants/Constants.h"
#include "common/memory/SourceBuffer.h"
#include "models/PipelineEventPtr.h"

namespace logtail {
class EventPool;

// referrences
// https://opentelemetry.io/docs/specs/otel/logs/data-model-appendix/#elastic-common-schema
// https://github.com/open-telemetry/semantic-conventions/blob/main/docs/resource/README.md
// https://github.com/open-telemetry/semantic-conventions/blob/main/docs/general/logs.md
// https://github.com/open-telemetry/semantic-conventions/blob/main/docs/resource/k8s.md
// https://github.com/open-telemetry/semantic-conventions/blob/main/docs/resource/container.md
enum class EventGroupMetaKey {
    UNKNOWN,
    LOG_FILE_PATH,
    LOG_FILE_PATH_RESOLVED,
    LOG_FILE_INODE,
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
    CONTAINER_IMAGE_ID,

    PROMETHEUS_SCRAPE_STATE,
    PROMETHEUS_SCRAPE_DURATION,
    PROMETHEUS_SCRAPE_RESPONSE_SIZE,
    PROMETHEUS_SAMPLES_SCRAPED,
    PROMETHEUS_SCRAPE_TIMESTAMP_MILLISEC,
    PROMETHEUS_UP_STATE,

    SOURCE_ID
};

using GroupMetadata = std::map<EventGroupMetaKey, StringView>;
using GroupTags = std::map<StringView, StringView>;

// DeepCopy is required if we want to support no-linear topology
// We cannot just use default copy constructor as it won't deep copy PipelineEvent pointed in Events vector.
using EventsContainer = std::vector<PipelineEventPtr>;

// only movable
class PipelineEventGroup {
public:
    PipelineEventGroup(const std::shared_ptr<SourceBuffer>& sourceBuffer) : mSourceBuffer(sourceBuffer) {}
    ~PipelineEventGroup();
    PipelineEventGroup(PipelineEventGroup&&) noexcept;
    PipelineEventGroup& operator=(PipelineEventGroup&&) noexcept;

    PipelineEventGroup Copy() const;

    std::unique_ptr<LogEvent> CreateLogEvent(bool fromPool = false, EventPool* pool = nullptr);
    std::unique_ptr<MetricEvent> CreateMetricEvent(bool fromPool = false, EventPool* pool = nullptr);
    std::unique_ptr<SpanEvent> CreateSpanEvent(bool fromPool = false, EventPool* pool = nullptr);
    std::unique_ptr<RawEvent> CreateRawEvent(bool fromPool = false, EventPool* pool = nullptr);

    const EventsContainer& GetEvents() const { return mEvents; }
    EventsContainer& MutableEvents() { return mEvents; }
    LogEvent* AddLogEvent(bool fromPool = false, EventPool* pool = nullptr);
    MetricEvent* AddMetricEvent(bool fromPool = false, EventPool* pool = nullptr);
    SpanEvent* AddSpanEvent(bool fromPool = false, EventPool* pool = nullptr);
    RawEvent* AddRawEvent(bool fromPool = false, EventPool* pool = nullptr);
    void SwapEvents(EventsContainer& other) { mEvents.swap(other); }
    void ReserveEvents(size_t size) { mEvents.reserve(size); }

    std::shared_ptr<SourceBuffer>& GetSourceBuffer() { return mSourceBuffer; }

    void SetMetadata(EventGroupMetaKey key, StringView val);
    void SetMetadata(EventGroupMetaKey key, const std::string& val);
    void SetMetadataNoCopy(EventGroupMetaKey key, const StringBuffer& val);
    StringView GetMetadata(EventGroupMetaKey key) const;
    const GroupMetadata& GetAllMetadata() const { return mMetadata; };
    bool HasMetadata(EventGroupMetaKey key) const;
    void SetMetadataNoCopy(EventGroupMetaKey key, StringView val);
    void DelMetadata(EventGroupMetaKey key);
    void SetAllMetadata(const GroupMetadata& other) { mMetadata = other; }

    void SetTag(StringView key, StringView val);
    void SetTag(const std::string& key, const std::string& val);
    void SetTag(const StringBuffer& key, StringView val);
    void SetTagNoCopy(const StringBuffer& key, const StringBuffer& val);
    StringView GetTag(StringView key) const;
    const GroupTags& GetTags() const { return mTags.mInner; };
    SizedMap& GetSizedTags() { return mTags; };
    bool HasTag(StringView key) const;
    void SetTagNoCopy(StringView key, StringView val);
    void DelTag(StringView key);

    size_t GetTagsHash() const;

    void SetExactlyOnceCheckpoint(const RangeCheckpointPtr& checkpoint) { mExactlyOnceCheckpoint = checkpoint; }
    RangeCheckpointPtr& GetExactlyOnceCheckpoint() { return mExactlyOnceCheckpoint; }
    bool IsReplay() const;

    size_t DataSize() const;

#ifdef APSARA_UNIT_TEST_MAIN
    // for debug and test
    Json::Value ToJson(bool enableEventMeta = false) const;
    bool FromJson(const Json::Value&);
    std::string ToJsonString(bool enableEventMeta = false) const;
    bool FromJsonString(const std::string&);
#endif

private:
    GroupMetadata mMetadata; // Used to generate tag/log. Will not output.
    SizedMap mTags; // custom tags to output
    EventsContainer mEvents;
    std::shared_ptr<SourceBuffer> mSourceBuffer;
    RangeCheckpointPtr mExactlyOnceCheckpoint;
};

} // namespace logtail

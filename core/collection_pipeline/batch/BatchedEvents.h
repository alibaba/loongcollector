/*
 * Copyright 2024 iLogtail Authors
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

#include <unordered_set>
#include <vector>

#include "common/StringView.h"
#include "models/PipelineEventGroup.h"

namespace logtail {

struct BatchedEvents {
    EventsContainer mEvents;
    SizedMap mTags;
    std::vector<std::shared_ptr<SourceBuffer>> mSourceBuffers;
    size_t mSizeBytes = 0; // only set on completion
    // for flusher_sls only
    RangeCheckpointPtr mExactlyOnceCheckpoint;
    // threads the file send checkpoint (input_static_file) to the flusher so that the
    // committed file offset is advanced only after the data is successfully sent
    FileSendCheckpointPtr mFileSendCheckpoint;
    StringView mPackIdPrefix;

    BatchedEvents() = default;
    BatchedEvents(const BatchedEvents& other) = delete;
    BatchedEvents& operator=(const BatchedEvents& other) = delete;
    BatchedEvents(BatchedEvents&& other) noexcept
        : mEvents(std::move(other.mEvents)),
          mTags(std::move(other.mTags)),
          mSourceBuffers(std::move(other.mSourceBuffers)),
          mSizeBytes(other.mSizeBytes),
          mExactlyOnceCheckpoint(std::move(other.mExactlyOnceCheckpoint)),
          mFileSendCheckpoint(std::move(other.mFileSendCheckpoint)),
          mPackIdPrefix(other.mPackIdPrefix) {}
    BatchedEvents& operator=(BatchedEvents&&) noexcept = delete;
    ~BatchedEvents();

    // for flusher_sls only
    BatchedEvents(EventsContainer&& events,
                  SizedMap&& tags,
                  std::shared_ptr<SourceBuffer>&& sourceBuffer,
                  StringView packIdPrefix,
                  RangeCheckpointPtr&& eoo);

    void Clear();
};

using BatchedEventsList = std::vector<BatchedEvents>;

} // namespace logtail

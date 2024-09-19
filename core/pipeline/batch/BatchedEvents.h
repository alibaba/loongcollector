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

#include "models/PipelineEventGroup.h"
#include "models/StringView.h"

namespace logtail {

struct BatchedEvents {
    EventsContainer mEvents;
    SizedMap mTags;
    std::vector<std::shared_ptr<SourceBuffer>> mSourceBuffers;
    size_t mSizeBytes = 0; // only set on completion
    // for flusher_sls only
    RangeCheckpointPtr mExactlyOnceCheckpoint;
    StringView mPackIdPrefix;

    BatchedEvents() = default;

    // for flusher_sls only
    BatchedEvents(EventsContainer&& events,
                  SizedMap&& tags,
                  std::shared_ptr<SourceBuffer>&& sourceBuffer,
                  StringView packIdPrefix,
                  RangeCheckpointPtr&& eoo)
        : mEvents(std::move(events)),
          mTags(std::move(tags)),
          mExactlyOnceCheckpoint(std::move(eoo)),
          mPackIdPrefix(packIdPrefix) {
        mSourceBuffers.emplace_back(std::move(sourceBuffer));
        mSizeBytes = sizeof(decltype(mEvents)) + mTags.DataSize();
        for (const auto& item : mEvents) {
            mSizeBytes += item->DataSize();
        }
    }

    void Clear() {
        mEvents.clear();
        mTags.Clear();
        mSourceBuffers.clear();
        mSizeBytes = 0;
        mExactlyOnceCheckpoint.reset();
        mPackIdPrefix = StringView();
    }
};

using BatchedEventsList = std::vector<BatchedEvents>;

} // namespace logtail

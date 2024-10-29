// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "pipeline/batch/BatchedEvents.h"

#include "models/EventPool.h"

using namespace std;

namespace logtail {

template <class T>
void DestroyEvents(vector<PipelineEventPtr>&& events) {
    unordered_map<EventPool*, vector<T*>> eventsPoolMap;
    for (auto& item : events) {
        if (item && item.IsFromEventPool()) {
            item->Reset();
            eventsPoolMap[item.GetEventPool()].emplace_back(static_cast<T*>(item.Release()));
        }
    }
    for (auto& item : eventsPoolMap) {
        if (item.first) {
            item.first->Release(std::move(item.second));
        } else {
            gThreadedEventPool.Release(std::move(item.second));
        }
    }
}

BatchedEvents::~BatchedEvents() {
    if (mEvents.empty() || !mEvents[0]) {
        return;
    }
    switch (mEvents[0]->GetType()) {
        case PipelineEvent::Type::LOG:
            DestroyEvents<LogEvent>(std::move(mEvents));
            break;
        case PipelineEvent::Type::METRIC:
            DestroyEvents<MetricEvent>(std::move(mEvents));
            break;
        case PipelineEvent::Type::SPAN:
            DestroyEvents<SpanEvent>(std::move(mEvents));
            break;
        default:
            break;
    }
}

BatchedEvents::BatchedEvents(EventsContainer&& events,
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

void BatchedEvents::Clear() {
    mEvents.clear();
    mTags.Clear();
    mSourceBuffers.clear();
    mSizeBytes = 0;
    mExactlyOnceCheckpoint.reset();
    mPackIdPrefix = StringView();
}

} // namespace logtail

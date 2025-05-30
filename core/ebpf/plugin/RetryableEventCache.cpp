// Copyright 2025 iLogtail Authors
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

#include "ebpf/plugin/RetryableEventCache.h"

#include <cstddef>

#include <iterator>
#include <mutex>

#include "ebpf/plugin/RetryableEvent.h"

namespace logtail::ebpf {

RetryableEventCache::RetryableEventCache() {
    mEventQueue.reserve(4096);
    mEventProcessing.reserve(4096);
}

size_t RetryableEventCache::Size() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mEventQueue.size();
}

void RetryableEventCache::AddEvent(std::shared_ptr<RetryableEvent> event) {
    std::lock_guard<std::mutex> lock(mMutex);
    mEventQueue.emplace_back(std::move(event));
}

void RetryableEventCache::Clear() {
    std::lock_guard<std::mutex> lock(mMutex);
    mEventQueue.clear();
}

void RetryableEventCache::HandleEvents() {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mEventProcessing.swap(mEventQueue);
    }
    size_t nextRetryItemCount = 0;
    for (auto& item : mEventProcessing) {
        if (item->OnRetry()) {
            continue;
        }
        item->DecrementRetryCount();
        if (item->CanRetry()) {
            mEventProcessing[nextRetryItemCount++] = item;
        } else {
            item->OnDrop();
        }
    }
    if (nextRetryItemCount > 0) {
        mEventProcessing.resize(nextRetryItemCount);
        std::lock_guard<std::mutex> lock(mMutex);
        mEventQueue.insert(mEventQueue.end(),
                           std::make_move_iterator(mEventProcessing.begin()),
                           std::make_move_iterator(mEventProcessing.end()));
    }
    mEventProcessing.clear();
}

void RetryableEventCache::ClearEvents() {
    std::lock_guard<std::mutex> lock(mMutex);
    mEventQueue.clear();
    mEventProcessing.clear();
};

} // namespace logtail::ebpf

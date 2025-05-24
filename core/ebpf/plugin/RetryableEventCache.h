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

#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "ebpf/plugin/RetryableEvent.h"

namespace logtail::ebpf {

/**
 * @class RetryableEvent
 * @brief A base class for events that can be retried.
 *
 * This class provides a framework for handling events that may need to be
 * retried a certain number of times before being considered failed.
 */
class RetryableEventCache {
public:
    RetryableEventCache();
    [[nodiscard]] size_t Size() const;
    void AddEvent(std::shared_ptr<RetryableEvent> event);
    void Clear();
    void HandleEvents();
    void ClearEvents();

private:
    mutable std::mutex mMutex;
    std::vector<std::shared_ptr<RetryableEvent>> mEventQueue;
    std::vector<std::shared_ptr<RetryableEvent>> mEventProcessing;
};
} // namespace logtail::ebpf

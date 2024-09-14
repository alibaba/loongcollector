/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <chrono>
#include <memory>

#include "models/PipelineEventGroup.h"

namespace logtail {

class Pipeline;

struct ProcessQueueItem {
    PipelineEventGroup mEventGroup;
    std::shared_ptr<Pipeline> mPipeline; // not null only during pipeline update
    size_t mInputIndex = 0; // index of the input in the pipeline
    std::chrono::system_clock::time_point mEnqueTime;

    ProcessQueueItem(PipelineEventGroup&& group, size_t index) : mEventGroup(std::move(group)), mInputIndex(index) {}
};

} // namespace logtail

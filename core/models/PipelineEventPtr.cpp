/*
 * Copyright 2025 iLogtail Authors
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

#include "models/PipelineEventPtr.h"

#include "models/EventPool.h"

namespace logtail {

template <typename T>
void PipelineEventPtr::releaseToPool() {
    if (mEventPool) {
        mEventPool->Release(static_cast<T*>(Release()));
    } else {
        gThreadedEventPool.Release(static_cast<T*>(Release()));
    }
}

void PipelineEventPtr::destroy() {
    if (mData && mFromEventPool) {
        mData->Reset();
        switch (mData->GetType()) {
            case PipelineEvent::Type::LOG:
                releaseToPool<LogEvent>();
                break;
            case PipelineEvent::Type::METRIC:
                releaseToPool<MetricEvent>();
                break;
            case PipelineEvent::Type::SPAN:
                releaseToPool<SpanEvent>();
                break;
            case PipelineEvent::Type::RAW:
                releaseToPool<RawEvent>();
                break;
            default:
                break;
        }
    }
}

PipelineEventPtr& PipelineEventPtr::operator=(PipelineEventPtr&& other) noexcept {
    if (this != &other) {
        destroy();
        mData = std::move(other.mData);
        mFromEventPool = other.mFromEventPool;
        mEventPool = other.mEventPool;
    }
    return *this;
}

PipelineEventPtr::~PipelineEventPtr() {
    destroy();
}
} // namespace logtail

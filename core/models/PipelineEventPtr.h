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
#include <typeinfo>

#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/PipelineEvent.h"
#include "models/SpanEvent.h"
#include "models/RawEvent.h"

namespace logtail {
class EventPool;

// only movable
class PipelineEventPtr {
public:
    PipelineEventPtr() = default;
    PipelineEventPtr(PipelineEvent* ptr, bool fromPool, EventPool* pool)
        : mData(std::unique_ptr<PipelineEvent>(ptr)), mFromEventPool(fromPool), mEventPool(pool) {}
    PipelineEventPtr(std::unique_ptr<PipelineEvent>&& ptr, bool fromPool, EventPool* pool)
        : mData(std::move(ptr)), mFromEventPool(fromPool), mEventPool(pool) {}

    template <typename T>
    bool Is() const {
        if (typeid(T) == typeid(LogEvent)) {
            return mData->GetType() == PipelineEvent::Type::LOG;
        }
        if (typeid(T) == typeid(MetricEvent)) {
            return mData->GetType() == PipelineEvent::Type::METRIC;
        }
        if (typeid(T) == typeid(SpanEvent)) {
            return mData->GetType() == PipelineEvent::Type::SPAN;
        }
        if (typeid(T) == typeid(RawEvent)) {
            return mData->GetType() == PipelineEvent::Type::RAW;
        }
        return false;
    }
    template <typename T>
    T& Cast() {
        return *static_cast<T*>(mData.get());
    }
    template <typename T>
    const T& Cast() const {
        return *static_cast<const T*>(mData.get());
    }
    template <typename T>
    T* Get() {
        return Is<T>() ? static_cast<T*>(mData.get()) : nullptr;
    }
    template <typename T>
    const T* Get() const {
        return Is<T>() ? static_cast<const T*>(mData.get()) : nullptr;
    }
    PipelineEvent* Release() { return mData.release(); }

    operator bool() const { return static_cast<bool>(mData); }
    PipelineEvent* operator->() { return mData.operator->(); }
    const PipelineEvent* operator->() const { return mData.operator->(); }

    PipelineEventPtr Copy() const { return PipelineEventPtr(mData->Copy(), mFromEventPool, mEventPool); }
    bool IsFromEventPool() const { return mFromEventPool; }
    EventPool* GetEventPool() const { return mEventPool; }

private:
    std::unique_ptr<PipelineEvent> mData;
    bool mFromEventPool = false;
    EventPool* mEventPool = nullptr; // null means using processor runner threaded pool
};

} // namespace logtail

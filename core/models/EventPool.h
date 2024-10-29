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

#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <vector>

#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/SpanEvent.h"

namespace logtail {
class PipelineEventGroup;

class EventPool {
public:
    EventPool(bool enableLock = true) : mEnableLock(enableLock) {};
    ~EventPool();
    EventPool(const EventPool&) = delete;
    EventPool& operator=(const EventPool&) = delete;

    LogEvent* AcquireLogEvent(PipelineEventGroup* ptr);
    MetricEvent* AcquireMetricEvent(PipelineEventGroup* ptr);
    SpanEvent* AcquireSpanEvent(PipelineEventGroup* ptr);
    void Release(std::vector<LogEvent*>&& obj);
    void Release(std::vector<MetricEvent*>&& obj);
    void Release(std::vector<SpanEvent*>&& obj);
    void CheckGC();

#ifdef APSARA_UNIT_TEST_MAIN
    void Clear();
#endif

private:
    template <class T>
    void TransferPoolIfEmpty(std::vector<T*>& pool, std::vector<T*>& poolBak) {
        std::lock_guard<std::mutex> lock(mPoolMux);
        if (pool.empty()) {
            std::lock_guard<std::mutex> lk(mPoolBakMux);
            pool.swap(poolBak);
        }
    }

    template <class T>
    T* AcquireEventNoLock(PipelineEventGroup* ptr, std::vector<T*>& pool, size_t& minUnusedCnt) {
        if (pool.empty()) {
            return new T(ptr);
        }

        auto obj = pool.back();
        obj->ResetPipelineEventGroup(ptr);
        pool.pop_back();
        minUnusedCnt = std::min(minUnusedCnt, pool.size());
        return obj;
    }

    void DestroyAllEventPool();
    void DestroyAllEventPoolBak();

    bool mEnableLock = true;

    std::mutex mPoolMux;
    std::vector<LogEvent*> mLogEventPool;
    std::vector<MetricEvent*> mMetricEventPool;
    std::vector<SpanEvent*> mSpanEventPool;

    // only meaningful when mEnableLock is true
    std::mutex mPoolBakMux;
    std::vector<LogEvent*> mLogEventPoolBak;
    std::vector<MetricEvent*> mMetricEventPoolBak;
    std::vector<SpanEvent*> mSpanEventPoolBak;

    size_t mMinUnusedLogEventsCnt = std::numeric_limits<size_t>::max();
    size_t mMinUnusedMetricEventsCnt = std::numeric_limits<size_t>::max();
    size_t mMinUnusedSpanEventsCnt = std::numeric_limits<size_t>::max();

    time_t mLastGCTime = 0;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class EventPoolUnittest;
    friend class PipelineEventGroupUnittest;
    friend class BatchedEventsUnittest;
#endif
};

extern thread_local EventPool gThreadedEventPool;

} // namespace logtail

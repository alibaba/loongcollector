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

#include "models/EventPool.h"

#include "common/Flags.h"
#include "logger/Logger.h"

DEFINE_FLAG_INT32(event_pool_gc_interval_secs, "", 60);

using namespace std;

namespace logtail {

EventPool::~EventPool() {
    if (mEnableLock) {
        {
            lock_guard<mutex> lock(mPoolMux);
            DestroyAllEventPool();
        }
        {
            lock_guard<mutex> lockBak(mPoolBakMux);
            DestroyAllEventPoolBak();
        }
    } else {
        DestroyAllEventPool();
    }
}

LogEvent* EventPool::AcquireLogEvent(PipelineEventGroup* ptr) {
    TransferPoolIfEmpty(mLogEventPool, mLogEventPoolBak);

    if (mEnableLock) {
        lock_guard<mutex> lock(mPoolMux);
        return AcquireEventNoLock(ptr, mLogEventPool, mMinUnusedLogEventsCnt);
    }
    return AcquireEventNoLock(ptr, mLogEventPool, mMinUnusedLogEventsCnt);
}

MetricEvent* EventPool::AcquireMetricEvent(PipelineEventGroup* ptr) {
    TransferPoolIfEmpty(mMetricEventPool, mMetricEventPoolBak);

    if (mEnableLock) {
        lock_guard<mutex> lock(mPoolMux);
        return AcquireEventNoLock(ptr, mMetricEventPool, mMinUnusedMetricEventsCnt);
    }
    return AcquireEventNoLock(ptr, mMetricEventPool, mMinUnusedMetricEventsCnt);
}

SpanEvent* EventPool::AcquireSpanEvent(PipelineEventGroup* ptr) {
    TransferPoolIfEmpty(mSpanEventPool, mSpanEventPoolBak);

    if (mEnableLock) {
        lock_guard<mutex> lock(mPoolMux);
        return AcquireEventNoLock(ptr, mSpanEventPool, mMinUnusedSpanEventsCnt);
    }
    return AcquireEventNoLock(ptr, mSpanEventPool, mMinUnusedSpanEventsCnt);
}

void EventPool::Release(vector<LogEvent*>&& obj) {
    if (mEnableLock) {
        lock_guard<mutex> lock(mPoolBakMux);
        mLogEventPoolBak.insert(mLogEventPoolBak.end(), obj.begin(), obj.end());
    } else {
        mLogEventPool.insert(mLogEventPool.end(), obj.begin(), obj.end());
    }
}

void EventPool::Release(vector<MetricEvent*>&& obj) {
    if (mEnableLock) {
        lock_guard<mutex> lock(mPoolBakMux);
        mMetricEventPoolBak.insert(mMetricEventPoolBak.end(), obj.begin(), obj.end());
    } else {
        mMetricEventPool.insert(mMetricEventPool.end(), obj.begin(), obj.end());
    }
}

void EventPool::Release(vector<SpanEvent*>&& obj) {
    if (mEnableLock) {
        lock_guard<mutex> lock(mPoolBakMux);
        mSpanEventPoolBak.insert(mSpanEventPoolBak.end(), obj.begin(), obj.end());
    } else {
        mSpanEventPool.insert(mSpanEventPool.end(), obj.begin(), obj.end());
    }
}

template <class T>
void DoGC(vector<T*>& pool, vector<T*>& poolBak, size_t& minUnusedCnt, mutex* mux) {
    if (minUnusedCnt <= pool.size() || minUnusedCnt == numeric_limits<size_t>::max()) {
        auto sz = minUnusedCnt == numeric_limits<size_t>::max() ? pool.size() : minUnusedCnt;
        for (size_t i = 0; i < sz; ++i) {
            delete pool.back();
            pool.pop_back();
        }
        if (mux) {
            lock_guard<mutex> lock(*mux);
            for (auto& item : poolBak) {
                delete item;
            }
            poolBak.clear();
        }
    } else {
        LOG_ERROR(sLogger,
                  ("unexpected error", "min unused event cnt is greater than pool size")(
                      "min unused cnt", minUnusedCnt)("pool size", pool.size()));
    }
    minUnusedCnt = numeric_limits<size_t>::max();
}

void EventPool::CheckGC() {
    if (time(nullptr) - mLastGCTime > INT32_FLAG(event_pool_gc_interval_secs)) {
        if (mEnableLock) {
            lock_guard<mutex> lock(mPoolMux);
            DoGC(mLogEventPool, mLogEventPoolBak, mMinUnusedLogEventsCnt, &mPoolBakMux);
            DoGC(mMetricEventPool, mMetricEventPoolBak, mMinUnusedMetricEventsCnt, &mPoolBakMux);
            DoGC(mSpanEventPool, mSpanEventPoolBak, mMinUnusedSpanEventsCnt, &mPoolBakMux);
        } else {
            DoGC(mLogEventPool, mLogEventPoolBak, mMinUnusedLogEventsCnt, nullptr);
            DoGC(mMetricEventPool, mMetricEventPoolBak, mMinUnusedMetricEventsCnt, nullptr);
            DoGC(mSpanEventPool, mSpanEventPoolBak, mMinUnusedSpanEventsCnt, nullptr);
        }
        mLastGCTime = time(nullptr);
    }
}

void EventPool::DestroyAllEventPool() {
    for (auto& item : mLogEventPool) {
        delete item;
    }
    for (auto& item : mMetricEventPool) {
        delete item;
    }
    for (auto& item : mSpanEventPool) {
        delete item;
    }
}

void EventPool::DestroyAllEventPoolBak() {
    for (auto& item : mLogEventPoolBak) {
        delete item;
    }
    for (auto& item : mMetricEventPoolBak) {
        delete item;
    }
    for (auto& item : mSpanEventPoolBak) {
        delete item;
    }
}

#ifdef APSARA_UNIT_TEST_MAIN
void EventPool::Clear() {
    {
        lock_guard<mutex> lock(mPoolMux);
        DestroyAllEventPool();
        mLogEventPool.clear();
        mMetricEventPool.clear();
        mSpanEventPool.clear();
        mMinUnusedLogEventsCnt = numeric_limits<size_t>::max();
        mMinUnusedMetricEventsCnt = numeric_limits<size_t>::max();
        mMinUnusedSpanEventsCnt = numeric_limits<size_t>::max();
    }
    {
        lock_guard<mutex> lock(mPoolBakMux);
        DestroyAllEventPoolBak();
        mLogEventPoolBak.clear();
        mMetricEventPoolBak.clear();
        mSpanEventPoolBak.clear();
    }
    mLastGCTime = 0;
}
#endif

thread_local EventPool gThreadedEventPool(false);

} // namespace logtail

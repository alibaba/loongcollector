/*
 * Copyright 2025 iLogtail Authors
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

#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <vector>

#include "monitor/MetricManager.h"

namespace logtail {

struct OTLPGrpcCallContext;

class GrpcSink {
public:
    GrpcSink(const GrpcSink&) = delete;
    GrpcSink& operator=(const GrpcSink&) = delete;

    static GrpcSink* GetInstance();

    virtual bool Init();
    virtual void Stop();
    virtual void AddRequest(std::unique_ptr<OTLPGrpcCallContext>&& ctx);

    int32_t GetInFlightCount() const { return mSendingCnt.load(); }

private:
    GrpcSink() = default;
    ~GrpcSink() = default;

    void Run();
    void DispatchRequests();

    std::future<void> mThreadRes;
    std::atomic_bool mIsFlush{false};

    std::mutex mPendingMutex;
    std::condition_variable mCV;
    std::vector<std::unique_ptr<OTLPGrpcCallContext>> mPendingRequests;

    std::atomic<int32_t> mSendingCnt{0};

    mutable MetricsRecordRef mMetricsRecordRef;
    CounterPtr mInItemsTotal;
    CounterPtr mOutSuccessfulItemsTotal;
    CounterPtr mOutFailedItemsTotal;
    IntGaugePtr mSendingItemsTotal;
    IntGaugePtr mLastRunTime;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class GrpcSinkMock;
#endif
};

} // namespace logtail

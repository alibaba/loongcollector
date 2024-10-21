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

#include <curl/multi.h>

#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>

#include "runner/sink/Sink.h"
#include "runner/sink/http/HttpSinkRequest.h"
#include "monitor/LogtailMetric.h"

namespace logtail {

class HttpSink : public Sink<HttpSinkRequest> {
public:
    HttpSink(const HttpSink&) = delete;
    HttpSink& operator=(const HttpSink&) = delete;

    static HttpSink* GetInstance() {
        static HttpSink instance;
        return &instance;
    }

    bool Init() override;
    void Stop() override;

private:
    HttpSink() = default;
    ~HttpSink() = default;

    void Run();
    bool AddRequestToClient(std::unique_ptr<HttpSinkRequest>&& request);
    void DoRun();
    void HandleCompletedRequests();

    CURLM* mClient = nullptr;

    std::future<void> mThreadRes;
    std::atomic_bool mIsFlush = false;

    mutable MetricsRecordRef mMetricsRecordRef;
    CounterPtr mInItemsTotal;
    CounterPtr mOutSuccessfulItemsTotal;
    CounterPtr mOutFailedItemsTotal;
    CounterPtr mSuccessfulItemTotalResponseTimeMs;
    CounterPtr mFailedItemTotalResponseTimeMs;
    IntGaugePtr mSendingItemsTotal;
    IntGaugePtr mSendConcurrency;
    IntGaugePtr mLastRunTime;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class FlusherRunnerUnittest;
#endif
};

} // namespace logtail

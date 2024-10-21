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

#include <atomic>
#include <mutex>
#include <cstdint>
#include <string>
#include <chrono>

#include "monitor/metric_constants/MetricConstants.h"

namespace logtail {

class ConcurrencyLimiter {
public:
    ConcurrencyLimiter(uint32_t maxConcurrency, uint32_t maxRetryIntervalSecs = 3600, 
        uint32_t minRetryIntervalSecs = 30, double retryIntervalUpRatio = 1.5, double concurrencyDownRatio = 0.5) : 
        mMaxConcurrency(maxConcurrency), mCurrenctConcurrency(maxConcurrency),
        mMaxRetryIntervalSecs(maxRetryIntervalSecs), mMinRetryIntervalSecs(minRetryIntervalSecs), 
        mRetryIntervalSecs(minRetryIntervalSecs), mRetryIntervalUpRatio(retryIntervalUpRatio), mConcurrencyDownRatio(concurrencyDownRatio) {}

    bool IsValidToPop();
    void PostPop();
    void OnSendDone();

    void OnSuccess();
    void OnFail();

    static std::string GetLimiterMetricName(const std::string& limiter) {
        if (limiter == "region") {
            return  METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_REGION_LIMITER_TIMES_TOTAL;
        } else if (limiter == "project") {
            return  METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_PROJECT_LIMITER_TIMES_TOTAL;
        } else if (limiter == "logstore") {
            return  METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_LOGSTORE_LIMITER_TIMES_TOTAL;
        } 
        return limiter;
    }

#ifdef APSARA_UNIT_TEST_MAIN

    uint32_t GetCurrentLimit() const;
    uint32_t GetCurrentInterval() const;
    void SetCurrentLimit(uint32_t limit);
    void SetInSendingCount(uint32_t count);
    uint32_t GetInSendingCount() const;

#endif

private:
    std::atomic_uint32_t mInSendingCnt = 0U;

    uint32_t mMaxConcurrency = 0;

    mutable std::mutex mLimiterMux;
    uint32_t mCurrenctConcurrency = 0;

    uint32_t mMaxRetryIntervalSecs = 0;
    uint32_t mMinRetryIntervalSecs = 0;

    uint32_t mRetryIntervalSecs = 0;

    double mRetryIntervalUpRatio = 0.0;
    double mConcurrencyDownRatio = 0.0;

    std::chrono::system_clock::time_point mLastCheckTime;
};

} // namespace logtail

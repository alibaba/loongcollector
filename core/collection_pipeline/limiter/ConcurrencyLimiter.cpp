// Copyright 2024 iLogtail Authors
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

#include "collection_pipeline/limiter/ConcurrencyLimiter.h"

#include "common/StringTools.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail {
#ifdef APSARA_UNIT_TEST_MAIN
uint32_t ConcurrencyLimiter::GetCurrentLimit() const {
    lock_guard<mutex> lock(mLimiterMux);
    return mCurrentConcurrency;
}

void ConcurrencyLimiter::SetCurrentLimit(uint32_t limit) {
    lock_guard<mutex> lock(mLimiterMux);
    mCurrentConcurrency = limit;
}

void ConcurrencyLimiter::SetInSendingCount(uint32_t count) {
    mInSendingCnt.store(count);
}
uint32_t ConcurrencyLimiter::GetInSendingCount() const {
    return mInSendingCnt.load();
}

uint32_t ConcurrencyLimiter::GetStatisticThreshold() const {
    return CONCURRENCY_STATISTIC_THRESHOLD;
}

#endif

bool ConcurrencyLimiter::IsValidToPop() {
    lock_guard<mutex> lock(mLimiterMux);
    if (mCurrentConcurrency > mInSendingCnt.load()) {
        return true;
    }
    return false;
}

void ConcurrencyLimiter::PostPop() {
    ++mInSendingCnt;
}

void ConcurrencyLimiter::OnSendDone() {
    --mInSendingCnt;
}

void ConcurrencyLimiter::OnSuccess(std::chrono::system_clock::time_point currentTime) {
    AdjustConcurrency(true, currentTime);
}

void ConcurrencyLimiter::OnFail(std::chrono::system_clock::time_point currentTime) {
    AdjustConcurrency(false, currentTime);
}

void ConcurrencyLimiter::Increase() {
    lock_guard<mutex> lock(mLimiterMux);
    if (mCurrentConcurrency != mMaxConcurrency) {
        ++mCurrentConcurrency;
        if (mCurrentConcurrency == mMaxConcurrency) {
            LOG_DEBUG(sLogger,
                      ("increase send concurrency to maximum, type", mDescription)("concurrency", mCurrentConcurrency));
        } else {
            LOG_DEBUG(sLogger,
                      ("increase send concurrency, type",
                       mDescription)("from", mCurrentConcurrency - 1)("to", mCurrentConcurrency));
        }
    }
}

void ConcurrencyLimiter::Decrease(double fallBackRatio) {
    lock_guard<mutex> lock(mLimiterMux);
    if (mCurrentConcurrency != mMinConcurrency) {
        auto old = mCurrentConcurrency;
        mCurrentConcurrency = std::max(static_cast<uint32_t>(mCurrentConcurrency * fallBackRatio), mMinConcurrency);
        LOG_DEBUG(sLogger, ("decrease send concurrency, type", mDescription)("from", old)("to", mCurrentConcurrency));
    } else {
        if (mMinConcurrency == 0) {
            mCurrentConcurrency = 1;
            LOG_INFO(sLogger, ("decrease send concurrency to min, type", mDescription)("to", mCurrentConcurrency));
        }
    }
}


void ConcurrencyLimiter::AdjustConcurrency(bool success, std::chrono::system_clock::time_point currentTime) {
    uint32_t failPercentage = 0;
    bool finishStatistics = false;
    {
        lock_guard<mutex> lock(mStatisticsMux);
        mStatisticsTotal++;
        if (!success) {
            mStatisticsFailTotal++;
        }
        if (mLastStatisticsTime == std::chrono::system_clock::time_point()) {
            mLastStatisticsTime = currentTime;
        }
        if (mStatisticsTotal == CONCURRENCY_STATISTIC_THRESHOLD
            || chrono::duration_cast<chrono::seconds>(currentTime - mLastStatisticsTime).count()
                > CONCURRENCY_STATISTIC_INTERVAL_THRESHOLD_SECONDS) {
            failPercentage = mStatisticsFailTotal * 100 / mStatisticsTotal;
            LOG_DEBUG(sLogger,
                      ("AdjustConcurrency", mDescription)("mStatisticsFailTotal",
                                                          mStatisticsFailTotal)("mStatisticsTotal", mStatisticsTotal));
            mStatisticsTotal = 0;
            mStatisticsFailTotal = 0;
            mLastStatisticsTime = currentTime;
            finishStatistics = true;
        }
    }
    if (finishStatistics) {
        if (failPercentage == 0) {
            // 成功
            Increase();
        } else if (failPercentage <= NO_FALL_BACK_FAIL_PERCENTAGE) {
            // 不调整
        } else if (failPercentage <= SLOW_FALL_BACK_FAIL_PERCENTAGE) {
            // 慢回退
            Decrease(mConcurrencySlowFallBackRatio);
        } else {
            // 快速回退
            Decrease(mConcurrencyFastFallBackRatio);
        }
    }
}


} // namespace logtail

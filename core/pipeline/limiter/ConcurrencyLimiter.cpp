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

#include "pipeline/limiter/ConcurrencyLimiter.h"

#include "common/StringTools.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail {

#ifdef APSARA_UNIT_TEST_MAIN
uint32_t ConcurrencyLimiter::GetCurrentLimit() const {
    lock_guard<mutex> lock(mLimiterMux);
    return mCurrenctConcurrency;
}

uint32_t ConcurrencyLimiter::GetCurrentInterval() const {
    lock_guard<mutex> lock(mLimiterMux);
    return mRetryIntervalSecs;
}
void ConcurrencyLimiter::SetCurrentLimit(uint32_t limit) {
    lock_guard<mutex> lock(mLimiterMux);
    mCurrenctConcurrency = limit;
}

void ConcurrencyLimiter::SetInSendingCount(uint32_t count) {
    mInSendingCnt.store(count);
}
uint32_t ConcurrencyLimiter::GetInSendingCount() const {
    return mInSendingCnt.load();
}
#endif

bool ConcurrencyLimiter::IsValidToPop() {
    lock_guard<mutex> lock(mLimiterMux);
    if (mCurrenctConcurrency == 0) {
        auto curTime = std::chrono::system_clock::now();
        if (chrono::duration_cast<chrono::seconds>(curTime - mLastCheckTime).count() > mRetryIntervalSecs) {
            mLastCheckTime = curTime;
            return true;
        } else {
            return false;
        }
    }
    if (mCurrenctConcurrency > mInSendingCnt.load()) {
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

void ConcurrencyLimiter::OnSuccess() {
    lock_guard<mutex> lock(mLimiterMux);
    if (mCurrenctConcurrency <= 0) {
        mRetryIntervalSecs = mMinRetryIntervalSecs;
        LOG_INFO(sLogger, ("reset send retry interval, type", mDescription));
    }
    if (mCurrenctConcurrency != mMaxConcurrency) {
        ++mCurrenctConcurrency;
        if (mCurrenctConcurrency == mMaxConcurrency) {
            LOG_INFO(sLogger,
                     ("increase send concurrency to maximum, type", mDescription)("concurrency", mCurrenctConcurrency));
        } else {
            LOG_DEBUG(sLogger,
                      ("increase send concurrency, type",
                       mDescription)("from", mCurrenctConcurrency - 1)("to", mCurrenctConcurrency));
        }
    }
}

void ConcurrencyLimiter::OnFail() {
    lock_guard<mutex> lock(mLimiterMux);
    if (mCurrenctConcurrency != 0) {
        auto old = mCurrenctConcurrency;
        mCurrenctConcurrency = static_cast<uint32_t>(mCurrenctConcurrency * mConcurrencyDownRatio);
        LOG_INFO(sLogger, ("decrease send concurrency, type", mDescription)("from", old)("to", mCurrenctConcurrency));
    } else {
        if (mRetryIntervalSecs != mMaxRetryIntervalSecs) {
            auto old = mRetryIntervalSecs;
            mRetryIntervalSecs
                = min(mMaxRetryIntervalSecs, static_cast<uint32_t>(mRetryIntervalSecs * mRetryIntervalUpRatio));
            LOG_INFO(sLogger,
                     ("increase send retry interval, type",
                      mDescription)("from", ToString(old) + "s")("to", ToString(mRetryIntervalSecs) + "s"));
        }
    }
}

} // namespace logtail

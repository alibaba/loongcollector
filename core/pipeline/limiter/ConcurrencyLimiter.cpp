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
uint32_t ConcurrencyLimiter::GetInSendingCount() const { return mInSendingCnt.load(); }

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
    }    
    if (mCurrenctConcurrency != mMaxConcurrency) {
        ++mCurrenctConcurrency;
    }
}

void ConcurrencyLimiter::OnFail() {
    lock_guard<mutex> lock(mLimiterMux);
    if (mCurrenctConcurrency != 0) {
        mCurrenctConcurrency = static_cast<uint32_t>(mCurrenctConcurrency * mConcurrencyDownRatio);
    } else {
        if (mRetryIntervalSecs != mMaxRetryIntervalSecs) {
            mRetryIntervalSecs = min(mMaxRetryIntervalSecs, static_cast<uint32_t>(mRetryIntervalSecs * mRetryIntervalUpRatio));
        }
    }
}

} // namespace logtail

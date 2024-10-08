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
#include "app_config/AppConfig.h"
#include "common/TimeUtil.h"

using namespace std;

namespace logtail {

uint32_t ConcurrencyLimiter::GetCurrentLimit() const { 
    lock_guard<mutex> lock(mCurrenctConcurrencyMux);
    return mCurrenctConcurrency; 
}

uint32_t ConcurrencyLimiter::GetCurrentInterval() const { 
    lock_guard<mutex> lock(mIntervalMux);
    return mRetryIntervalSecs; 
}

#ifdef APSARA_UNIT_TEST_MAIN
void ConcurrencyLimiter::SetCurrentLimit(uint32_t limit) { 
    lock_guard<mutex> lock(mCurrenctConcurrencyMux);
    mCurrenctConcurrency = limit;
}

void ConcurrencyLimiter::SetInSendingCount(uint32_t count) {
    mInSendingCnt.store(count);
}
uint32_t ConcurrencyLimiter::GetInSendingCount() const { return mInSendingCnt.load(); }

#endif


bool ConcurrencyLimiter::IsValidToPop() {
    if (mLastCheckTime.time_since_epoch().count()  == 0) {
        mLastCheckTime = std::chrono::system_clock::now();
    }
    if (GetCurrentLimit() <= mMinConcurrency) {
        std::chrono::system_clock::time_point curTime = std::chrono::system_clock::now();
        if (chrono::duration_cast<chrono::seconds>(curTime - mLastCheckTime).count() > GetCurrentInterval()) {
            mLastCheckTime = curTime;
            return true;
        } else {
            return false;
        }
    }
    if (GetCurrentLimit() > mInSendingCnt.load()) {
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
    {
        lock_guard<mutex> lock(mCurrenctConcurrencyMux);        
        if (mCurrenctConcurrency != mMaxConcurrency) {
            ++mCurrenctConcurrency;
        }
    }
    {
        // 只要成功，interval就设置成最小值
        lock_guard<mutex> lock(mIntervalMux);
        mRetryIntervalSecs = mMinRetryIntervalSecs;
    }
}

void ConcurrencyLimiter::OnFail(time_t curTime) {
    {
        lock_guard<mutex> lock(mCurrenctConcurrencyMux);
        if (mCurrenctConcurrency != mMinConcurrency) {
            mCurrenctConcurrency = max(mMinConcurrency, static_cast<uint32_t>(mCurrenctConcurrency * mDownRatio));
        }
    }
    {
        lock_guard<mutex> lock(mIntervalMux);
        if (GetCurrentLimit() <= mMinConcurrency) {
            if (mRetryIntervalSecs != mMaxRetryIntervalSecs) {
                mRetryIntervalSecs = min(mMaxRetryIntervalSecs, static_cast<uint32_t>(mRetryIntervalSecs * mUpRatio));
            }
        }
    }
}

} // namespace logtail

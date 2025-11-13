// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "collection_pipeline/limiter/ConcurrencyLimiter.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class ConcurrencyLimiterUnittest : public testing::Test {
public:
    void TestLimiter() const;
    void TestTimeFallback() const;
    void TestNoTimeFallback() const;
};

void ConcurrencyLimiterUnittest::TestLimiter() const {
    auto curSystemTime = chrono::system_clock::now();
    int maxConcurrency = 80;
    int minConcurrency = 20;

    shared_ptr<ConcurrencyLimiter> sConcurrencyLimiter
        = make_shared<ConcurrencyLimiter>("", maxConcurrency, minConcurrency);
    // fastFallBack
    APSARA_TEST_EQUAL(true, sConcurrencyLimiter->IsValidToPop());
    for (uint32_t i = 0; i < sConcurrencyLimiter->GetStatisticThreshold(); i++) {
        sConcurrencyLimiter->PostPop();
        APSARA_TEST_EQUAL(1U, sConcurrencyLimiter->GetInSendingCount());
        curSystemTime = chrono::system_clock::now();
        sConcurrencyLimiter->OnFail(curSystemTime);
        sConcurrencyLimiter->OnSendDone();
    }
    APSARA_TEST_EQUAL(40U, sConcurrencyLimiter->GetCurrentLimit());
    APSARA_TEST_EQUAL(0U, sConcurrencyLimiter->GetInSendingCount());

    // success one time
    APSARA_TEST_EQUAL(true, sConcurrencyLimiter->IsValidToPop());
    for (uint32_t i = 0; i < sConcurrencyLimiter->GetStatisticThreshold(); i++) {
        APSARA_TEST_EQUAL(true, sConcurrencyLimiter->IsValidToPop());
        sConcurrencyLimiter->PostPop();
    }
    APSARA_TEST_EQUAL(10U, sConcurrencyLimiter->GetInSendingCount());
    for (uint32_t i = 0; i < sConcurrencyLimiter->GetStatisticThreshold(); i++) {
        curSystemTime = chrono::system_clock::now();
        sConcurrencyLimiter->OnSuccess(curSystemTime);
        sConcurrencyLimiter->OnSendDone();
    }

    APSARA_TEST_EQUAL(0U, sConcurrencyLimiter->GetInSendingCount());
    APSARA_TEST_EQUAL(41U, sConcurrencyLimiter->GetCurrentLimit());

    // slowFallBack
    for (uint32_t i = 0; i < sConcurrencyLimiter->GetStatisticThreshold() - 2; i++) {
        sConcurrencyLimiter->PostPop();
        curSystemTime = chrono::system_clock::now();
        sConcurrencyLimiter->OnSuccess(curSystemTime);
        sConcurrencyLimiter->OnSendDone();
    }
    for (int i = 0; i < 2; i++) {
        sConcurrencyLimiter->PostPop();
        curSystemTime = chrono::system_clock::now();
        sConcurrencyLimiter->OnFail(curSystemTime);
        sConcurrencyLimiter->OnSendDone();
    }
    uint32_t expect = 41 * 0.8;
    APSARA_TEST_EQUAL(0U, sConcurrencyLimiter->GetInSendingCount());
    APSARA_TEST_EQUAL(expect, sConcurrencyLimiter->GetCurrentLimit());

    // no FallBack
    for (uint32_t i = 0; i < sConcurrencyLimiter->GetStatisticThreshold() - 1; i++) {
        sConcurrencyLimiter->PostPop();
        curSystemTime = chrono::system_clock::now();
        sConcurrencyLimiter->OnSuccess(curSystemTime);
        sConcurrencyLimiter->OnSendDone();
    }
    for (int i = 0; i < 1; i++) {
        sConcurrencyLimiter->PostPop();
        curSystemTime = chrono::system_clock::now();
        sConcurrencyLimiter->OnFail(curSystemTime);
        sConcurrencyLimiter->OnSendDone();
    }
    APSARA_TEST_EQUAL(0U, sConcurrencyLimiter->GetInSendingCount());
    APSARA_TEST_EQUAL(expect, sConcurrencyLimiter->GetCurrentLimit());

    // test FallBack to minConcurrency
    for (int i = 0; i < 10; i++) {
        for (uint32_t j = 0; j < sConcurrencyLimiter->GetStatisticThreshold(); j++) {
            sConcurrencyLimiter->PostPop();
            curSystemTime = chrono::system_clock::now();
            sConcurrencyLimiter->OnFail(curSystemTime);
            sConcurrencyLimiter->OnSendDone();
        }
    }
    APSARA_TEST_EQUAL(0U, sConcurrencyLimiter->GetInSendingCount());
    APSARA_TEST_EQUAL(minConcurrency, sConcurrencyLimiter->GetCurrentLimit());

    // test limit by concurrency
    for (int i = 0; i < minConcurrency; i++) {
        APSARA_TEST_EQUAL(true, sConcurrencyLimiter->IsValidToPop());
        sConcurrencyLimiter->PostPop();
    }
    APSARA_TEST_EQUAL(false, sConcurrencyLimiter->IsValidToPop());
    for (int i = 0; i < minConcurrency; i++) {
        sConcurrencyLimiter->OnSendDone();
    }

    // test time exceed interval; 8 success, 1 fail, and last one timeout
    sConcurrencyLimiter->SetCurrentLimit(40);
    for (uint32_t i = 0; i < sConcurrencyLimiter->GetStatisticThreshold() - 3; i++) {
        sConcurrencyLimiter->PostPop();
        curSystemTime = chrono::system_clock::now();
        sConcurrencyLimiter->OnSuccess(curSystemTime);
        sConcurrencyLimiter->OnSendDone();
    }
    for (int i = 0; i < 1; i++) {
        sConcurrencyLimiter->PostPop();
        curSystemTime = chrono::system_clock::now();
        sConcurrencyLimiter->OnFail(curSystemTime);
        sConcurrencyLimiter->OnSendDone();
    }
    sleep(4);
    for (int i = 0; i < 1; i++) {
        sConcurrencyLimiter->PostPop();
        curSystemTime = chrono::system_clock::now();
        sConcurrencyLimiter->OnSuccess(curSystemTime);
        sConcurrencyLimiter->OnSendDone();
    }
    expect = 40 * 0.8;
    APSARA_TEST_EQUAL(0U, sConcurrencyLimiter->GetInSendingCount());
    APSARA_TEST_EQUAL(expect, sConcurrencyLimiter->GetCurrentLimit());
}

void ConcurrencyLimiterUnittest::TestTimeFallback() const {
    auto curSystemTime = chrono::system_clock::now();
    int maxConcurrency = 80;
    int minConcurrency = 1;

    // Create limiter with time fallback enabled (duration > 0 enables it)
    shared_ptr<ConcurrencyLimiter> limiter
        = make_shared<ConcurrencyLimiter>("test_time_fallback", maxConcurrency, minConcurrency, 3000);

    // Test 1: Decrease to minimum should enter time fallback state
    for (int i = 0; i < 10; i++) {
        for (uint32_t j = 0; j < limiter->GetStatisticThreshold(); j++) {
            limiter->PostPop();
            curSystemTime = chrono::system_clock::now();
            limiter->OnFail(curSystemTime);
            limiter->OnSendDone();
        }
    }
    APSARA_TEST_EQUAL(minConcurrency, limiter->GetCurrentLimit());
    APSARA_TEST_TRUE(limiter->IsInTimeFallback());

    // Test 2: During fallback period (< 3s), IsValidToPop should return false
    APSARA_TEST_FALSE(limiter->IsValidToPop());
    sleep(1);
    APSARA_TEST_FALSE(limiter->IsValidToPop());
    sleep(1);
    APSARA_TEST_FALSE(limiter->IsValidToPop());

    // Test 3: After 3 seconds, IsValidToPop should return true (allow one request)
    sleep(2);
    APSARA_TEST_EQUAL(0U, limiter->GetInSendingCount());
    APSARA_TEST_TRUE(limiter->IsValidToPop());
    limiter->PostPop();
    APSARA_TEST_EQUAL(1U, limiter->GetInSendingCount());
    APSARA_TEST_TRUE(limiter->IsInTimeFallback());

    // Test 4: After allowing one request, timer resets, should block again for 3s
    APSARA_TEST_FALSE(limiter->IsValidToPop());
    sleep(2);
    APSARA_TEST_FALSE(limiter->IsValidToPop());

    // Test 5: OnSuccess should clear time fallback state
    curSystemTime = chrono::system_clock::now();
    limiter->OnSuccess(curSystemTime);
    limiter->OnSendDone();
    APSARA_TEST_FALSE(limiter->IsInTimeFallback());

    // Test 6: After clearing fallback, should work normally
    APSARA_TEST_TRUE(limiter->IsValidToPop());
    limiter->PostPop();
    APSARA_TEST_EQUAL(1U, limiter->GetInSendingCount());
    limiter->OnSendDone();

    // Test 7: Test multiple fallback cycles (continuous failure scenario)
    // Trigger fallback state by continuous failures when at minimum concurrency
    limiter->SetCurrentLimit(minConcurrency);
    for (uint32_t i = 0; i < limiter->GetStatisticThreshold(); i++) {
        limiter->PostPop();
        curSystemTime = chrono::system_clock::now();
        limiter->OnFail(curSystemTime);
        limiter->OnSendDone();
    }
    APSARA_TEST_TRUE(limiter->IsInTimeFallback());

    // First cycle: wait 3s, allow one request, reset timer
    sleep(3);
    APSARA_TEST_TRUE(limiter->IsValidToPop());
    limiter->PostPop();
    limiter->OnSendDone();

    // Should block again immediately after the request
    APSARA_TEST_FALSE(limiter->IsValidToPop());

    // Second cycle: wait another 3s, allow one request
    sleep(3);
    APSARA_TEST_TRUE(limiter->IsValidToPop());
    limiter->PostPop();
    limiter->OnSendDone();

    // Still in fallback state
    APSARA_TEST_TRUE(limiter->IsInTimeFallback());

    // Test 8: Success should immediately exit fallback
    curSystemTime = chrono::system_clock::now();
    limiter->OnSuccess(curSystemTime);
    APSARA_TEST_FALSE(limiter->IsInTimeFallback());
}

void ConcurrencyLimiterUnittest::TestNoTimeFallback() const {
    auto curSystemTime = chrono::system_clock::now();
    int maxConcurrency = 80;
    int minConcurrency = 1;

    // Create limiter with time fallback disabled (duration = 0 disables it)
    shared_ptr<ConcurrencyLimiter> limiter
        = make_shared<ConcurrencyLimiter>("test_no_time_fallback", maxConcurrency, minConcurrency, 0);

    // Test 1: Decrease to minimum concurrency through continuous failures
    for (int i = 0; i < 10; i++) {
        for (uint32_t j = 0; j < limiter->GetStatisticThreshold(); j++) {
            limiter->PostPop();
            curSystemTime = chrono::system_clock::now();
            limiter->OnFail(curSystemTime);
            limiter->OnSendDone();
        }
    }
    APSARA_TEST_EQUAL(minConcurrency, limiter->GetCurrentLimit());

    // Test 2: Should NOT enter time fallback state when time fallback is disabled
    APSARA_TEST_FALSE(limiter->IsInTimeFallback());

    // Test 3: IsValidToPop should work normally (no time-based blocking)
    // At minConcurrency=1, should allow up to 1 concurrent request
    APSARA_TEST_EQUAL(0U, limiter->GetInSendingCount());
    APSARA_TEST_TRUE(limiter->IsValidToPop());
    limiter->PostPop();
    APSARA_TEST_EQUAL(1U, limiter->GetInSendingCount());

    // Test 4: Should block when reaching concurrency limit (not by time)
    APSARA_TEST_FALSE(limiter->IsValidToPop());

    // Test 5: After OnSendDone, should immediately allow next request (no time delay)
    limiter->OnSendDone();
    APSARA_TEST_EQUAL(0U, limiter->GetInSendingCount());
    APSARA_TEST_TRUE(limiter->IsValidToPop());

    // Test 6: Continue to fail, should remain at minConcurrency without entering fallback
    limiter->PostPop();
    for (uint32_t i = 0; i < limiter->GetStatisticThreshold(); i++) {
        curSystemTime = chrono::system_clock::now();
        limiter->OnFail(curSystemTime);
    }
    limiter->OnSendDone();

    APSARA_TEST_EQUAL(minConcurrency, limiter->GetCurrentLimit());
    APSARA_TEST_FALSE(limiter->IsInTimeFallback());

    // Test 7: Can still pop immediately without time-based restrictions
    APSARA_TEST_TRUE(limiter->IsValidToPop());
    limiter->PostPop();
    APSARA_TEST_EQUAL(1U, limiter->GetInSendingCount());
    limiter->OnSendDone();

    // Test 8: OnSuccess should increase concurrency normally
    for (uint32_t i = 0; i < limiter->GetStatisticThreshold(); i++) {
        limiter->PostPop();
        curSystemTime = chrono::system_clock::now();
        limiter->OnSuccess(curSystemTime);
        limiter->OnSendDone();
    }
    APSARA_TEST_EQUAL(minConcurrency + 1, limiter->GetCurrentLimit());
    APSARA_TEST_FALSE(limiter->IsInTimeFallback());
}

UNIT_TEST_CASE(ConcurrencyLimiterUnittest, TestLimiter)
UNIT_TEST_CASE(ConcurrencyLimiterUnittest, TestTimeFallback)
UNIT_TEST_CASE(ConcurrencyLimiterUnittest, TestNoTimeFallback)

} // namespace logtail

UNIT_TEST_MAIN

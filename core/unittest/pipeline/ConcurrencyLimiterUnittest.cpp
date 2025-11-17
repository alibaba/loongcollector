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
    void TestExponentialBackoffWithMaxDuration() const;
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
    limiter->OnSendDone(); // Complete the request

    // Test 4: After allowing one request, timer resets with exponential backoff
    // Next wait time should be 3s * 2 = 6s (exponential backoff)
    APSARA_TEST_FALSE(limiter->IsValidToPop());
    sleep(3);
    APSARA_TEST_FALSE(limiter->IsValidToPop()); // Still < 6s
    sleep(3);
    APSARA_TEST_TRUE(limiter->IsValidToPop()); // Now >= 6s

    // Test 5: OnSuccess should clear time fallback state immediately
    limiter->PostPop();
    curSystemTime = chrono::system_clock::now();
    limiter->OnSuccess(curSystemTime);
    limiter->OnSendDone();
    APSARA_TEST_FALSE(limiter->IsInTimeFallback());

    // Test 6: After clearing fallback, should work normally
    APSARA_TEST_TRUE(limiter->IsValidToPop());
    limiter->PostPop();
    APSARA_TEST_EQUAL(1U, limiter->GetInSendingCount());
    limiter->OnSendDone();

    // Test 7: Test exponential backoff in multiple retry cycles
    // Trigger fallback state by continuous failures when at minimum concurrency
    limiter->SetCurrentLimit(minConcurrency);
    for (uint32_t i = 0; i < limiter->GetStatisticThreshold(); i++) {
        limiter->PostPop();
        curSystemTime = chrono::system_clock::now();
        limiter->OnFail(curSystemTime);
        limiter->OnSendDone();
    }
    APSARA_TEST_TRUE(limiter->IsInTimeFallback());

    // First retry: wait 3s, allow one request, backoff time increases to 6s
    sleep(3);
    APSARA_TEST_TRUE(limiter->IsValidToPop());
    limiter->PostPop();
    APSARA_TEST_TRUE(limiter->IsInTimeFallback());
    limiter->OnSendDone(); // Complete the request

    // Should block for 6s now (exponential backoff: 3s * 2)
    APSARA_TEST_FALSE(limiter->IsValidToPop());
    sleep(3);
    APSARA_TEST_FALSE(limiter->IsValidToPop()); // Still < 6s
    sleep(3);
    APSARA_TEST_TRUE(limiter->IsValidToPop()); // Now >= 6s
    limiter->PostPop();
    APSARA_TEST_TRUE(limiter->IsInTimeFallback());
    limiter->OnSendDone(); // Complete the request

    // Still in fallback state, next wait would be 12s (6s * 2)
    APSARA_TEST_TRUE(limiter->IsInTimeFallback());

    // Test 8: Success should immediately exit fallback and reset backoff duration
    // Need to actually send a request that succeeds
    limiter->PostPop();
    curSystemTime = chrono::system_clock::now();
    limiter->OnSuccess(curSystemTime);
    limiter->OnSendDone();
    APSARA_TEST_FALSE(limiter->IsInTimeFallback());

    // Test 9: After exiting fallback, if enter again, should start from initial duration (3s)
    limiter->SetCurrentLimit(minConcurrency);
    for (uint32_t i = 0; i < limiter->GetStatisticThreshold(); i++) {
        limiter->PostPop();
        curSystemTime = chrono::system_clock::now();
        limiter->OnFail(curSystemTime);
        limiter->OnSendDone();
    }
    APSARA_TEST_TRUE(limiter->IsInTimeFallback());
    sleep(3);
    APSARA_TEST_TRUE(limiter->IsValidToPop()); // Should work after 3s (not 12s from previous)
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

void ConcurrencyLimiterUnittest::TestExponentialBackoffWithMaxDuration() const {
    auto curSystemTime = chrono::system_clock::now();
    int maxConcurrency = 80;
    int minConcurrency = 1;

    // Create limiter with small initial duration (1s) to test exponential growth faster
    // backoff multiplier = 2.0, max duration = 10s (for faster testing)
    shared_ptr<ConcurrencyLimiter> limiter = make_shared<ConcurrencyLimiter>(
        "test_exponential_backoff", maxConcurrency, minConcurrency, 1000, 0.5, 0.8, 2.0, 10000);

    // Trigger time fallback
    for (int i = 0; i < 10; i++) {
        for (uint32_t j = 0; j < limiter->GetStatisticThreshold(); j++) {
            limiter->PostPop();
            curSystemTime = chrono::system_clock::now();
            limiter->OnFail(curSystemTime);
            limiter->OnSendDone();
        }
    }
    APSARA_TEST_TRUE(limiter->IsInTimeFallback());

    // Test exponential backoff: 1s -> 2s -> 4s -> 8s -> 10s (capped at max)
    // Retry 1: wait 1s
    sleep(1);
    APSARA_TEST_TRUE(limiter->IsValidToPop());
    limiter->PostPop();
    limiter->OnSendDone();

    // Retry 2: wait 2s (1s * 2)
    sleep(1);
    APSARA_TEST_FALSE(limiter->IsValidToPop());
    sleep(1);
    APSARA_TEST_TRUE(limiter->IsValidToPop());
    limiter->PostPop();
    limiter->OnSendDone();

    // Retry 3: wait 4s (2s * 2)
    sleep(2);
    APSARA_TEST_FALSE(limiter->IsValidToPop());
    sleep(2);
    APSARA_TEST_TRUE(limiter->IsValidToPop());
    limiter->PostPop();
    limiter->OnSendDone();

    // Retry 4: wait 8s (4s * 2)
    sleep(4);
    APSARA_TEST_FALSE(limiter->IsValidToPop());
    sleep(4);
    APSARA_TEST_TRUE(limiter->IsValidToPop());
    limiter->PostPop();
    limiter->OnSendDone();

    // Retry 5: should be capped at max 10s, not 16s (8s * 2)
    sleep(8);
    APSARA_TEST_FALSE(limiter->IsValidToPop());
    sleep(2);
    APSARA_TEST_TRUE(limiter->IsValidToPop()); // After 10s total
    limiter->PostPop();
    limiter->OnSendDone();

    // Retry 6: should still be at max 10s
    sleep(10);
    APSARA_TEST_TRUE(limiter->IsValidToPop());

    // Test that OnSuccess resets backoff to initial value
    limiter->PostPop();
    curSystemTime = chrono::system_clock::now();
    limiter->OnSuccess(curSystemTime);
    limiter->OnSendDone();
    APSARA_TEST_FALSE(limiter->IsInTimeFallback());

    // Re-enter fallback and verify it starts from 1s again
    limiter->SetCurrentLimit(minConcurrency);
    for (uint32_t i = 0; i < limiter->GetStatisticThreshold(); i++) {
        limiter->PostPop();
        curSystemTime = chrono::system_clock::now();
        limiter->OnFail(curSystemTime);
        limiter->OnSendDone();
    }
    APSARA_TEST_TRUE(limiter->IsInTimeFallback());
    sleep(1);
    APSARA_TEST_TRUE(limiter->IsValidToPop()); // Should work after 1s (reset to initial)
}

UNIT_TEST_CASE(ConcurrencyLimiterUnittest, TestLimiter)
UNIT_TEST_CASE(ConcurrencyLimiterUnittest, TestTimeFallback)
UNIT_TEST_CASE(ConcurrencyLimiterUnittest, TestNoTimeFallback)
UNIT_TEST_CASE(ConcurrencyLimiterUnittest, TestExponentialBackoffWithMaxDuration)

} // namespace logtail

UNIT_TEST_MAIN

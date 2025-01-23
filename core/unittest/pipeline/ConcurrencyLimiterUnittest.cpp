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

UNIT_TEST_CASE(ConcurrencyLimiterUnittest, TestLimiter)

} // namespace logtail

UNIT_TEST_MAIN

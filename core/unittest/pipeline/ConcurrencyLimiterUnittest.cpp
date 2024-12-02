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

#include "pipeline/limiter/ConcurrencyLimiter.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class ConcurrencyLimiterUnittest : public testing::Test {
public:
    void TestLimiter() const;
};

void ConcurrencyLimiterUnittest::TestLimiter() const {
    shared_ptr<ConcurrencyLimiter> sConcurrencyLimiter = make_shared<ConcurrencyLimiter>("", 80);
    // comcurrency = 10, count = 0
    APSARA_TEST_EQUAL(true, sConcurrencyLimiter->IsValidToPop());
    sConcurrencyLimiter->PostPop();
    APSARA_TEST_EQUAL(1U, sConcurrencyLimiter->GetInSendingCount());
    sConcurrencyLimiter->OnFail();
    sConcurrencyLimiter->OnSendDone();
    APSARA_TEST_EQUAL(40U, sConcurrencyLimiter->GetCurrentLimit());
    APSARA_TEST_EQUAL(0U, sConcurrencyLimiter->GetInSendingCount());
    APSARA_TEST_EQUAL(30U, sConcurrencyLimiter->GetCurrentInterval());

    // count = 10, comcurrency = 10
    APSARA_TEST_EQUAL(true, sConcurrencyLimiter->IsValidToPop());
    int num = 10;
    for (int i = 0; i < num; i++) {
        APSARA_TEST_EQUAL(true, sConcurrencyLimiter->IsValidToPop());
        sConcurrencyLimiter->PostPop();
    }
    APSARA_TEST_EQUAL(10U, sConcurrencyLimiter->GetInSendingCount());
    for (int i = 0; i < num; i++) {
        sConcurrencyLimiter->OnSuccess();
        sConcurrencyLimiter->OnSendDone();
    }
    APSARA_TEST_EQUAL(0U, sConcurrencyLimiter->GetInSendingCount());
    APSARA_TEST_EQUAL(50U, sConcurrencyLimiter->GetCurrentLimit());
    APSARA_TEST_EQUAL(30U, sConcurrencyLimiter->GetCurrentInterval());

    // limit = 50/2/2/2/2/2/2/2 = 25/2/2/2/2/2/2 = 3/2/2/2 = 1/2/2 = 0 
    // interval = 30 * 1.5  = 45
    num = 7;
    for (int i = 0; i < num; i++) {
        APSARA_TEST_EQUAL(true, sConcurrencyLimiter->IsValidToPop());
        sConcurrencyLimiter->PostPop();
    }
    APSARA_TEST_EQUAL(7U, sConcurrencyLimiter->GetInSendingCount());
    for (int i = 0; i < num; i++) {
        sConcurrencyLimiter->OnFail();
        sConcurrencyLimiter->OnSendDone();
    }
    APSARA_TEST_EQUAL(0U, sConcurrencyLimiter->GetInSendingCount());
    APSARA_TEST_EQUAL(0U, sConcurrencyLimiter->GetCurrentLimit());
    APSARA_TEST_EQUAL(45U, sConcurrencyLimiter->GetCurrentInterval());

    num = 3;
    for (int i = 0; i < num; i++) {
        if (i == 0) {
            APSARA_TEST_EQUAL(true, sConcurrencyLimiter->IsValidToPop());
        } else {
            APSARA_TEST_EQUAL(false, sConcurrencyLimiter->IsValidToPop());
        }
    }

    sConcurrencyLimiter->PostPop();
    sConcurrencyLimiter->OnSuccess();
    sConcurrencyLimiter->OnSendDone();

    APSARA_TEST_EQUAL(0U, sConcurrencyLimiter->GetInSendingCount());
    APSARA_TEST_EQUAL(1U, sConcurrencyLimiter->GetCurrentLimit());
    APSARA_TEST_EQUAL(30U, sConcurrencyLimiter->GetCurrentInterval());
}

UNIT_TEST_CASE(ConcurrencyLimiterUnittest, TestLimiter)

} // namespace logtail

UNIT_TEST_MAIN

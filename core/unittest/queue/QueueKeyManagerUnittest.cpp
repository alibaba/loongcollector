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

#include "pipeline/queue/QueueKeyManager.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class QueueKeyManagerUnittest : public testing::Test {
public:
    void TestGetKey();
    void TestHasKey();
    void TestRemoveKey();
    void TestGetName();

protected:
    void TearDown() override { QueueKeyManager::GetInstance()->Clear(); }
};

void QueueKeyManagerUnittest::TestGetKey() {
    APSARA_TEST_EQUAL(0, QueueKeyManager::GetInstance()->GetKey("key1"));
    APSARA_TEST_EQUAL(1, QueueKeyManager::GetInstance()->GetKey("key2"));
    APSARA_TEST_EQUAL(0, QueueKeyManager::GetInstance()->GetKey("key1"));
}

void QueueKeyManagerUnittest::TestHasKey() {
    QueueKeyManager::GetInstance()->GetKey("key1");

    APSARA_TEST_TRUE(QueueKeyManager::GetInstance()->HasKey("key1"));
    APSARA_TEST_FALSE(QueueKeyManager::GetInstance()->HasKey("key2"));
}

void QueueKeyManagerUnittest::TestRemoveKey() {
    QueueKeyManager::GetInstance()->GetKey("key1");

    APSARA_TEST_TRUE(QueueKeyManager::GetInstance()->RemoveKey(0));
    APSARA_TEST_FALSE(QueueKeyManager::GetInstance()->RemoveKey(1));
    APSARA_TEST_EQUAL("", QueueKeyManager::GetInstance()->GetName(0));
    APSARA_TEST_EQUAL(1, QueueKeyManager::GetInstance()->GetKey("key1"));
}

void QueueKeyManagerUnittest::TestGetName() {
    QueueKeyManager::GetInstance()->GetKey("key1");

    APSARA_TEST_EQUAL("key1", QueueKeyManager::GetInstance()->GetName(0));
    APSARA_TEST_EQUAL("", QueueKeyManager::GetInstance()->GetName(1));
}

UNIT_TEST_CASE(QueueKeyManagerUnittest, TestGetKey)
UNIT_TEST_CASE(QueueKeyManagerUnittest, TestHasKey)
UNIT_TEST_CASE(QueueKeyManagerUnittest, TestRemoveKey)
UNIT_TEST_CASE(QueueKeyManagerUnittest, TestGetName)

} // namespace logtail

UNIT_TEST_MAIN

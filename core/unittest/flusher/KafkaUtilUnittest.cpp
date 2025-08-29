/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef APSARA_UNIT_TEST_MAIN
#define APSARA_UNIT_TEST_MAIN
#endif

#include <cassert>

#include <string>
#include <vector>

#include "plugin/flusher/kafka/KafkaUtil.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class KafkaUtilUnittest : public ::testing::Test {
public:
    void TestBrokersToStringEmpty();
    void TestBrokersToStringSingle();
    void TestBrokersToStringMultiple();
};

void KafkaUtilUnittest::TestBrokersToStringEmpty() {
    vector<string> emptyBrokers;
    string result = KafkaUtil::BrokersToString(emptyBrokers);
    APSARA_TEST_EQUAL("", result);
}

void KafkaUtilUnittest::TestBrokersToStringSingle() {
    vector<string> singleBroker = {"localhost:9092"};
    string result = KafkaUtil::BrokersToString(singleBroker);
    APSARA_TEST_EQUAL("localhost:9092", result);
}

void KafkaUtilUnittest::TestBrokersToStringMultiple() {
    vector<string> multipleBrokers = {"broker1:9092", "broker2:9092", "broker3:9092"};
    string result = KafkaUtil::BrokersToString(multipleBrokers);
    APSARA_TEST_EQUAL("broker1:9092,broker2:9092,broker3:9092", result);
}

UNIT_TEST_CASE(KafkaUtilUnittest, TestBrokersToStringEmpty)
UNIT_TEST_CASE(KafkaUtilUnittest, TestBrokersToStringSingle)
UNIT_TEST_CASE(KafkaUtilUnittest, TestBrokersToStringMultiple)

} // namespace logtail

UNIT_TEST_MAIN

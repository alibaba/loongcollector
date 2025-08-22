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

#include <librdkafka/rdkafka.h>

#include "plugin/flusher/kafka/KafkaUtil.h"
#include "unittest/Unittest.h"

namespace logtail {

class KafkaUtilUnittest : public ::testing::Test {
public:
    void TestBrokersToString();
    void TestGetErrorString();
    void TestIsRetriableError();
    void TestIsNetworkError();
    void TestIsAuthError();
    void TestCalculateBackoffDelay();
};

void KafkaUtilUnittest::TestBrokersToString() {
    std::vector<std::string> brokers = {"host1:9092", "host2:9092", "host3:9092"};
    APSARA_TEST_EQUAL("host1:9092,host2:9092,host3:9092", KafkaUtil::BrokersToString(brokers));

    std::vector<std::string> singleBroker = {"host1:9092"};
    APSARA_TEST_EQUAL("host1:9092", KafkaUtil::BrokersToString(singleBroker));

    std::vector<std::string> emptyBrokers;
    APSARA_TEST_EQUAL("", KafkaUtil::BrokersToString(emptyBrokers));
}

void KafkaUtilUnittest::TestGetErrorString() {
    APSARA_TEST_EQUAL("Success", KafkaUtil::GetErrorString(RD_KAFKA_RESP_ERR_NO_ERROR));
    APSARA_TEST_EQUAL("Broker: Not leader for partition",
                      KafkaUtil::GetErrorString(RD_KAFKA_RESP_ERR_NOT_LEADER_FOR_PARTITION));
}

void KafkaUtilUnittest::TestIsRetriableError() {
    APSARA_TEST_TRUE(KafkaUtil::IsRetriableError(RD_KAFKA_RESP_ERR_LEADER_NOT_AVAILABLE));
    APSARA_TEST_TRUE(KafkaUtil::IsRetriableError(RD_KAFKA_RESP_ERR_NETWORK_EXCEPTION));
    APSARA_TEST_FALSE(KafkaUtil::IsRetriableError(RD_KAFKA_RESP_ERR__AUTHENTICATION));
    APSARA_TEST_FALSE(KafkaUtil::IsRetriableError(RD_KAFKA_RESP_ERR_MSG_SIZE_TOO_LARGE));
}

void KafkaUtilUnittest::TestIsNetworkError() {
    APSARA_TEST_TRUE(KafkaUtil::IsNetworkError(RD_KAFKA_RESP_ERR__TRANSPORT));
    APSARA_TEST_TRUE(KafkaUtil::IsNetworkError(RD_KAFKA_RESP_ERR__ALL_BROKERS_DOWN));
    APSARA_TEST_FALSE(KafkaUtil::IsNetworkError(RD_KAFKA_RESP_ERR_LEADER_NOT_AVAILABLE));
}

void KafkaUtilUnittest::TestIsAuthError() {
    APSARA_TEST_TRUE(KafkaUtil::IsAuthError(RD_KAFKA_RESP_ERR__AUTHENTICATION));
    APSARA_TEST_TRUE(KafkaUtil::IsAuthError(RD_KAFKA_RESP_ERR_TOPIC_AUTHORIZATION_FAILED));
    APSARA_TEST_FALSE(KafkaUtil::IsAuthError(RD_KAFKA_RESP_ERR__TRANSPORT));
}

void KafkaUtilUnittest::TestCalculateBackoffDelay() {
    APSARA_TEST_EQUAL(50, KafkaUtil::CalculateBackoffDelay(0, 50));
    APSARA_TEST_EQUAL(100, KafkaUtil::CalculateBackoffDelay(1, 50));
    APSARA_TEST_EQUAL(200, KafkaUtil::CalculateBackoffDelay(2, 50));
    APSARA_TEST_EQUAL(51200, KafkaUtil::CalculateBackoffDelay(10, 50));
    APSARA_TEST_EQUAL(51200, KafkaUtil::CalculateBackoffDelay(11, 50));
}

UNIT_TEST_CASE(KafkaUtilUnittest, TestBrokersToString)
UNIT_TEST_CASE(KafkaUtilUnittest, TestGetErrorString)
UNIT_TEST_CASE(KafkaUtilUnittest, TestIsRetriableError)
UNIT_TEST_CASE(KafkaUtilUnittest, TestIsNetworkError)
UNIT_TEST_CASE(KafkaUtilUnittest, TestIsAuthError)
UNIT_TEST_CASE(KafkaUtilUnittest, TestCalculateBackoffDelay)

} // namespace logtail

UNIT_TEST_MAIN

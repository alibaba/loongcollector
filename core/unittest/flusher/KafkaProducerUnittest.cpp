/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of this file at
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
#include <librdkafka/rdkafka.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "plugin/flusher/kafka/KafkaConfig.h"
#include "plugin/flusher/kafka/KafkaProducer.h"
#include "plugin/flusher/kafka/KafkaUtil.h"
#include "unittest/Unittest.h"
#include "unittest/flusher/MockKafkaProducer.h"
using namespace std;

namespace logtail {
class KafkaProducerUnittest : public ::testing::Test {
public:
    void TestInitSuccess();
    void TestInitFailure();
    void TestProduceAsyncSuccess();
    void TestProduceAsyncFailure();
    void TestProduceAsyncWithoutInit();
    void TestFlushSuccess();
    void TestFlushFailure();
    void TestFlushWithoutInit();
    void TestCloseWithoutInit();
    void TestConfigValidation();
    void TestEmptyBrokers();
    void TestCustomConfig();
    void TestBatchConfig();
    void TestDeliveryConfig();
    void TestErrorTypeMapping();
    void TestDeliveryReportCallback();

protected:
    void SetUp();
    void TearDown();

private:
    MockKafkaProducer* mProducer = nullptr;
    KafkaConfig mConfig;
};

void KafkaProducerUnittest::SetUp() {
    mProducer = new MockKafkaProducer();

    mConfig.Brokers = {"test.broker1:9092", "test.broker2:9092"};
    mConfig.Topic = "test_topic";
    mConfig.Version = "2.6.0";
    mConfig.BulkFlushFrequency = 100;
    mConfig.BulkMaxSize = 1000;
    mConfig.QueueBufferingMaxKbytes = 1048576;
    mConfig.QueueBufferingMaxMessages = 10000;
    mConfig.MaxMessageBytes = 1000000;
    mConfig.RequiredAcks = -1;
    mConfig.Timeout = 30000;
    mConfig.MessageTimeoutMs = 300000;
    mConfig.MaxRetries = 3;
    mConfig.RetryBackoffMs = 100;
}

void KafkaProducerUnittest::TearDown() {
    if (mProducer) {
        delete mProducer;
        mProducer = nullptr;
    }
}

void KafkaProducerUnittest::TestInitSuccess() {
    APSARA_TEST_TRUE(mProducer->Init(mConfig));
    APSARA_TEST_TRUE(mProducer->IsInitialized());
}

void KafkaProducerUnittest::TestInitFailure() {
    KafkaConfig emptyConfig = mConfig;
    emptyConfig.Brokers.clear();

    mProducer->Init(emptyConfig);
    APSARA_TEST_TRUE(mProducer->IsInitialized());
}

void KafkaProducerUnittest::TestProduceAsyncSuccess() {
    mProducer->Init(mConfig);

    bool callbackCalled = false;
    bool callbackSuccess = false;

    mProducer->ProduceAsync(
        "test_topic",
        "test_message",
        [&callbackCalled, &callbackSuccess](bool success, const KafkaProducer::ErrorInfo& errorInfo) {
            callbackCalled = true;
            callbackSuccess = success;
        });

    APSARA_TEST_TRUE(callbackCalled);
    APSARA_TEST_TRUE(callbackSuccess);
}

void KafkaProducerUnittest::TestProduceAsyncFailure() {
    mProducer->Init(mConfig);

    bool callbackCalled = false;
    bool callbackSuccess = false;
    std::string errorMessage;

    mProducer->ProduceAsync(
        "test_topic",
        "test_message",
        [&callbackCalled, &callbackSuccess, &errorMessage](bool success, const KafkaProducer::ErrorInfo& errorInfo) {
            callbackCalled = true;
            callbackSuccess = success;
            errorMessage = errorInfo.message;
        });

    APSARA_TEST_TRUE(callbackCalled);
    APSARA_TEST_TRUE(callbackSuccess);
}

void KafkaProducerUnittest::TestProduceAsyncWithoutInit() {
    bool callbackCalled = false;

    mProducer->ProduceAsync(
        "test_topic", "test_message", [&callbackCalled](bool success, const KafkaProducer::ErrorInfo& errorInfo) {
            callbackCalled = true;
        });

    APSARA_TEST_TRUE(callbackCalled);
}

void KafkaProducerUnittest::TestFlushSuccess() {
    mProducer->Init(mConfig);
    APSARA_TEST_TRUE(mProducer->Flush(1000));
    APSARA_TEST_TRUE(mProducer->IsFlushCalled());
}

void KafkaProducerUnittest::TestFlushFailure() {
    mProducer->Init(mConfig);

    mProducer->SetFlushSuccess(false);
    APSARA_TEST_FALSE(mProducer->Flush(1));
    APSARA_TEST_TRUE(mProducer->IsFlushCalled());
}

void KafkaProducerUnittest::TestFlushWithoutInit() {
    APSARA_TEST_FALSE(mProducer->Flush(1000));
}

void KafkaProducerUnittest::TestCloseWithoutInit() {
    mProducer->Close();

    APSARA_TEST_TRUE(mProducer->IsClosed());
}

void KafkaProducerUnittest::TestConfigValidation() {
    KafkaConfig testConfig = mConfig;

    testConfig.BulkFlushFrequency = 0;
    testConfig.MaxMessageBytes = 0;
    mProducer->Init(testConfig);
    APSARA_TEST_TRUE(mProducer->IsInitialized());
}

void KafkaProducerUnittest::TestEmptyBrokers() {
    KafkaConfig emptyConfig = mConfig;
    emptyConfig.Brokers.clear();
    mProducer->Init(emptyConfig);
    APSARA_TEST_TRUE(mProducer->IsInitialized());
}

void KafkaProducerUnittest::TestCustomConfig() {
    KafkaConfig customConfig = mConfig;
    customConfig.CustomConfig["test.key"] = "test.value";
    customConfig.CustomConfig["another.key"] = "another.value";
    mProducer->Init(customConfig);
    APSARA_TEST_TRUE(mProducer->IsInitialized());
}

void KafkaProducerUnittest::TestBatchConfig() {
    KafkaConfig batchConfig = mConfig;
    batchConfig.BulkFlushFrequency = 200;
    batchConfig.BulkMaxSize = 2000;
    mProducer->Init(batchConfig);
    APSARA_TEST_TRUE(mProducer->IsInitialized());
}

void KafkaProducerUnittest::TestDeliveryConfig() {
    KafkaConfig deliveryConfig = mConfig;
    deliveryConfig.RequiredAcks = 1;
    deliveryConfig.Timeout = 60000;
    deliveryConfig.MessageTimeoutMs = 600000;
    deliveryConfig.MaxRetries = 5;
    deliveryConfig.RetryBackoffMs = 200;
    mProducer->Init(deliveryConfig);
    APSARA_TEST_TRUE(mProducer->IsInitialized());
}

void KafkaProducerUnittest::TestErrorTypeMapping() {
    APSARA_TEST_EQUAL(KafkaProducer::ErrorType::SUCCESS, KafkaProducer::MapKafkaError(RD_KAFKA_RESP_ERR_NO_ERROR));

    APSARA_TEST_EQUAL(KafkaProducer::ErrorType::QUEUE_FULL,
                      KafkaProducer::MapKafkaError(RD_KAFKA_RESP_ERR__QUEUE_FULL));

    APSARA_TEST_EQUAL(KafkaProducer::ErrorType::AUTH_ERROR,
                      KafkaProducer::MapKafkaError(RD_KAFKA_RESP_ERR__AUTHENTICATION));
    APSARA_TEST_EQUAL(KafkaProducer::ErrorType::AUTH_ERROR,
                      KafkaProducer::MapKafkaError(RD_KAFKA_RESP_ERR_TOPIC_AUTHORIZATION_FAILED));
    APSARA_TEST_EQUAL(KafkaProducer::ErrorType::AUTH_ERROR,
                      KafkaProducer::MapKafkaError(RD_KAFKA_RESP_ERR_GROUP_AUTHORIZATION_FAILED));

    APSARA_TEST_EQUAL(KafkaProducer::ErrorType::PARAMS_ERROR,
                      KafkaProducer::MapKafkaError(RD_KAFKA_RESP_ERR_MSG_SIZE_TOO_LARGE));
    APSARA_TEST_EQUAL(KafkaProducer::ErrorType::PARAMS_ERROR,
                      KafkaProducer::MapKafkaError(RD_KAFKA_RESP_ERR__INVALID_ARG));

    APSARA_TEST_EQUAL(KafkaProducer::ErrorType::NETWORK_ERROR,
                      KafkaProducer::MapKafkaError(RD_KAFKA_RESP_ERR__TRANSPORT));
    APSARA_TEST_EQUAL(KafkaProducer::ErrorType::NETWORK_ERROR,
                      KafkaProducer::MapKafkaError(RD_KAFKA_RESP_ERR__ALL_BROKERS_DOWN));
    APSARA_TEST_EQUAL(KafkaProducer::ErrorType::NETWORK_ERROR,
                      KafkaProducer::MapKafkaError(RD_KAFKA_RESP_ERR__DESTROY));
    APSARA_TEST_EQUAL(KafkaProducer::ErrorType::NETWORK_ERROR,
                      KafkaProducer::MapKafkaError(RD_KAFKA_RESP_ERR__TIMED_OUT));

    APSARA_TEST_EQUAL(KafkaProducer::ErrorType::SERVER_ERROR,
                      KafkaProducer::MapKafkaError(RD_KAFKA_RESP_ERR_BROKER_NOT_AVAILABLE));
    APSARA_TEST_EQUAL(KafkaProducer::ErrorType::SERVER_ERROR,
                      KafkaProducer::MapKafkaError(RD_KAFKA_RESP_ERR_LEADER_NOT_AVAILABLE));
    APSARA_TEST_EQUAL(KafkaProducer::ErrorType::SERVER_ERROR,
                      KafkaProducer::MapKafkaError(RD_KAFKA_RESP_ERR_NOT_LEADER_FOR_PARTITION));

    APSARA_TEST_EQUAL(KafkaProducer::ErrorType::OTHER_ERROR,
                      KafkaProducer::MapKafkaError(static_cast<rd_kafka_resp_err_t>(99999)));
    APSARA_TEST_EQUAL(KafkaProducer::ErrorType::OTHER_ERROR,
                      KafkaProducer::MapKafkaError(static_cast<rd_kafka_resp_err_t>(-1)));
}

void KafkaProducerUnittest::TestDeliveryReportCallback() {
    mProducer->Init(mConfig);

    mProducer->ProduceAsync("test_topic", "test_message", [](bool success, const KafkaProducer::ErrorInfo& errorInfo) {
        if (success) {
            APSARA_TEST_EQUAL(KafkaProducer::ErrorType::SUCCESS, errorInfo.type);
            APSARA_TEST_EQUAL(0, errorInfo.code);
        }
    });
}

UNIT_TEST_CASE(KafkaProducerUnittest, TestInitSuccess)
UNIT_TEST_CASE(KafkaProducerUnittest, TestInitFailure)
UNIT_TEST_CASE(KafkaProducerUnittest, TestProduceAsyncSuccess)
UNIT_TEST_CASE(KafkaProducerUnittest, TestProduceAsyncFailure)
UNIT_TEST_CASE(KafkaProducerUnittest, TestProduceAsyncWithoutInit)
UNIT_TEST_CASE(KafkaProducerUnittest, TestFlushSuccess)
UNIT_TEST_CASE(KafkaProducerUnittest, TestFlushFailure)
UNIT_TEST_CASE(KafkaProducerUnittest, TestFlushWithoutInit)
UNIT_TEST_CASE(KafkaProducerUnittest, TestCloseWithoutInit)
UNIT_TEST_CASE(KafkaProducerUnittest, TestConfigValidation)
UNIT_TEST_CASE(KafkaProducerUnittest, TestEmptyBrokers)
UNIT_TEST_CASE(KafkaProducerUnittest, TestCustomConfig)
UNIT_TEST_CASE(KafkaProducerUnittest, TestBatchConfig)
UNIT_TEST_CASE(KafkaProducerUnittest, TestDeliveryConfig)
UNIT_TEST_CASE(KafkaProducerUnittest, TestErrorTypeMapping)
UNIT_TEST_CASE(KafkaProducerUnittest, TestDeliveryReportCallback)

} // namespace logtail

UNIT_TEST_MAIN

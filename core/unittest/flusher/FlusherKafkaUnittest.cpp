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

#define APSARA_UNIT_TEST_MAIN

#include <librdkafka/rdkafka.h>

#include <memory>
#include <string>
#include <vector>

#include "json/json.h"

#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/queue/QueueKeyManager.h"
#include "collection_pipeline/queue/SenderQueueManager.h"
#include "common/JsonUtil.h"
#include "common/memory/SourceBuffer.h"
#include "models/LogEvent.h"
#include "models/PipelineEventGroup.h"
#include "plugin/flusher/kafka/FlusherKafka.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class KafkaProducerMock {
public:
    static KafkaProducerMock& GetInstance() {
        static KafkaProducerMock instance;
        return instance;
    }

    struct MockState {
        rd_kafka_resp_err_t produceResult = RD_KAFKA_RESP_ERR_NO_ERROR;
        rd_kafka_resp_err_t flushResult = RD_KAFKA_RESP_ERR_NO_ERROR;
        int produceCalled = 0;
        int flushCalled = 0;
        int pollCalled = 0;
        int configSetCalled = 0;
        bool producerCreated = false;
        bool threadStarted = false;
        bool threadJoined = false;
        std::string topicName = "test_topic";
        std::string clientId;
        std::vector<std::string> brokers;
        std::map<std::string, std::string> configSettings;
        std::vector<std::pair<std::string, std::string>> produceHistory;
    };

    MockState& State() { return mState; }
    void Reset() { mState = MockState(); }

private:
    KafkaProducerMock() = default;
    MockState mState;
};


typedef void (*rd_kafka_delivery_report_cb_t)(rd_kafka_t* rk, const rd_kafka_message_t* rkmessage, void* opaque);


extern "C" {

rd_kafka_t* rd_kafka_new(rd_kafka_type_t type, rd_kafka_conf_t* conf, char* errstr, size_t errstr_size) {
    KafkaProducerMock::GetInstance().State().producerCreated = true;
    return reinterpret_cast<rd_kafka_t*>(0x1);
}

int rd_kafka_produce(rd_kafka_topic_t* rkt,
                     int32_t partition,
                     int msgflags,
                     void* payload,
                     size_t len,
                     const void* key,
                     size_t keylen,
                     void* msg_opaque) {
    auto& state = KafkaProducerMock::GetInstance().State();
    ++state.produceCalled;
    if (payload && len > 0) {
        std::string data(static_cast<char*>(payload), len);
        std::string keyStr(key ? static_cast<const char*>(key) : "", keylen);
        state.produceHistory.emplace_back(keyStr, data);
    }

    if (state.produceResult == RD_KAFKA_RESP_ERR_NO_ERROR) {
        return 0;
    } else {
        errno = state.produceResult;
        return -1;
    }
}

int rd_kafka_poll(rd_kafka_t* rk, int timeout_ms) {
    ++KafkaProducerMock::GetInstance().State().pollCalled;
    return 0;
}

rd_kafka_resp_err_t rd_kafka_flush(rd_kafka_t* rk, int timeout_ms) {
    ++KafkaProducerMock::GetInstance().State().flushCalled;
    return KafkaProducerMock::GetInstance().State().flushResult;
}

void rd_kafka_destroy(rd_kafka_t* rk) {
    KafkaProducerMock::GetInstance().State().producerCreated = false;
}

rd_kafka_resp_err_t rd_kafka_last_error() {
    return KafkaProducerMock::GetInstance().State().produceResult;
}

const char* rd_kafka_err2str(rd_kafka_resp_err_t err) {
    switch (err) {
        case RD_KAFKA_RESP_ERR_NO_ERROR:
            return "Success";
        case RD_KAFKA_RESP_ERR__QUEUE_FULL:
            return "Queue full";
        case RD_KAFKA_RESP_ERR__TRANSPORT:
            return "Transport failure";
        case RD_KAFKA_RESP_ERR_MSG_SIZE_TOO_LARGE:
            return "Message size too large";
        default:
            return "Unknown error";
    }
}

rd_kafka_topic_t* rd_kafka_topic_new(rd_kafka_t* rk, const char* topic, rd_kafka_topic_conf_t* conf) {
    KafkaProducerMock::GetInstance().State().topicName = topic ? topic : "";
    return reinterpret_cast<rd_kafka_topic_t*>(0x2);
}

void rd_kafka_topic_destroy(rd_kafka_topic_t* rkt) {
}

const char* rd_kafka_topic_name(const rd_kafka_topic_t* rkt) {
    return KafkaProducerMock::GetInstance().State().topicName.c_str();
}

rd_kafka_conf_t* rd_kafka_conf_new() {
    return reinterpret_cast<rd_kafka_conf_t*>(0x3);
}

rd_kafka_topic_conf_t* rd_kafka_topic_conf_new() {
    return reinterpret_cast<rd_kafka_topic_conf_t*>(0x4);
}

rd_kafka_conf_res_t
rd_kafka_conf_set(rd_kafka_conf_t* conf, const char* name, const char* value, char* errstr, size_t errstr_size) {
    auto& state = KafkaProducerMock::GetInstance().State();
    ++state.configSetCalled;
    if (name && value) {
        state.configSettings[name] = value;
    }
    return RD_KAFKA_CONF_OK;
}

void rd_kafka_conf_set_dr_msg_cb(rd_kafka_conf_t* conf, rd_kafka_delivery_report_cb_t dr_msg_cb) {
}
void rd_kafka_conf_set_opaque(rd_kafka_conf_t* conf, void* opaque) {
}
void rd_kafka_conf_destroy(rd_kafka_conf_t* conf) {
}
void rd_kafka_topic_conf_destroy(rd_kafka_topic_conf_t* topic_conf) {
}
}

class FlusherKafkaUnittest : public testing::Test {
public:
    void OnSuccessfulInit();
    void OnFailedInit();
    void OnPipelineUpdate();
    void TestSend();
    void TestFlush();
    void TestFlushAll();
    void TestConfigValidation();
    void TestSerializeFormat();

protected:
    void SetUp() override {
        ctx.SetConfigName("test_config");
        ctx.SetPipeline(pipeline);
        KafkaProducerMock::GetInstance().Reset();
    }

    void TearDown() override {
        if (QueueKeyManager::GetInstance()) {
            QueueKeyManager::GetInstance()->Clear();
        }
        if (SenderQueueManager::GetInstance()) {
            SenderQueueManager::GetInstance()->Clear();
        }
    }

    PipelineEventGroup GenerateSimpleGroup() {
        PipelineEventGroup group(make_shared<SourceBuffer>());
        group.SetMetadata(EventGroupMetaKey::SOURCE_ID, StringView("source-id"));
        auto logEvent = group.AddLogEvent();
        logEvent->SetTimestamp(1234567890);
        logEvent->SetContent(StringView("content_key"), StringView("content_value"));
        return group;
    }

    bool InitWithConfig(FlusherKafka& flusher, const std::string& configStr) {
        Json::Value configJson, optionalGoPipeline;
        std::string errorMsg;
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        flusher.SetContext(ctx);
        flusher.CreateMetricsRecordRef(FlusherKafka::sName, "1");
        bool result = flusher.Init(configJson, optionalGoPipeline);
        flusher.CommitMetricsRecordRef();
        return result;
    }

private:
    CollectionPipeline pipeline;
    CollectionPipelineContext ctx;
};

void FlusherKafkaUnittest::OnSuccessfulInit() {
    {
        FlusherKafka flusher;
        string configStr = R"({
            "Type": "flusher_kafka",
            "Brokers": ["localhost:9092"],
            "Topic": "test_topic"
        })";

        APSARA_TEST_TRUE(InitWithConfig(flusher, configStr));
        APSARA_TEST_EQUAL("test_topic", flusher.mTopic);
        APSARA_TEST_EQUAL(1UL, flusher.mBrokers.size());
        APSARA_TEST_EQUAL("localhost:9092", flusher.mBrokers[0]);
        APSARA_TEST_TRUE(flusher.mSerializer != nullptr);
    }


    {
        FlusherKafka flusher;
        string configStr = R"({
            "Type": "flusher_kafka",
            "Brokers": ["kafka1:9092", "kafka2:9092"],
            "Topic": "my_topic",
            "ClientID": "my_client",
            "TimeoutMs": 5000,
            "Retries": 3,
            "BatchNumMessages": 100,
            "LingerMs": 10
        })";

        APSARA_TEST_TRUE(InitWithConfig(flusher, configStr));
        APSARA_TEST_EQUAL("my_topic", flusher.mTopic);
        APSARA_TEST_EQUAL(2UL, flusher.mBrokers.size());
        APSARA_TEST_EQUAL("kafka1:9092", flusher.mBrokers[0]);
        APSARA_TEST_EQUAL("kafka2:9092", flusher.mBrokers[1]);
        APSARA_TEST_EQUAL("my_client", flusher.mClientID);
        APSARA_TEST_EQUAL(5000U, flusher.mTimeoutMs);
        APSARA_TEST_EQUAL(3U, flusher.mRetries);
        APSARA_TEST_EQUAL(100U, flusher.mBatchNumMessages);
        APSARA_TEST_EQUAL(10U, flusher.mLingerMs);
    }
}

void FlusherKafkaUnittest::OnFailedInit() {
    {
        FlusherKafka flusher;
        string configStr = R"({
            "Type": "flusher_kafka",
            "Topic": "test_topic"
        })";
        APSARA_TEST_FALSE(InitWithConfig(flusher, configStr));
    }


    {
        FlusherKafka flusher;
        string configStr = R"({
            "Type": "flusher_kafka",
            "Brokers": ["localhost:9092"]
        })";
        APSARA_TEST_FALSE(InitWithConfig(flusher, configStr));
    }


    {
        FlusherKafka flusher;
        string configStr = R"({
            "Type": "flusher_kafka",
            "Brokers": "invalid",
            "Topic": "test_topic"
        })";
        APSARA_TEST_FALSE(InitWithConfig(flusher, configStr));
    }


    {
        FlusherKafka flusher;
        string configStr = R"({
            "Type": "flusher_kafka",
            "Brokers": ["localhost:9092"],
            "Topic": 123
        })";
        APSARA_TEST_FALSE(InitWithConfig(flusher, configStr));
    }
}

void FlusherKafkaUnittest::OnPipelineUpdate() {
    CollectionPipelineContext ctx1;
    ctx1.SetConfigName("test_config_1");

    FlusherKafka flusher1;
    string configStr = R"({
        "Type": "flusher_kafka",
        "Brokers": ["localhost:9092"],
        "Topic": "test_topic"
    })";

    APSARA_TEST_TRUE(InitWithConfig(flusher1, configStr));
    APSARA_TEST_TRUE(flusher1.Start());


    CollectionPipelineContext ctx2;
    ctx2.SetConfigName("test_config_2");

    FlusherKafka flusher2;
    string configStr2 = R"({
        "Type": "flusher_kafka",
        "Brokers": ["localhost:9093"],
        "Topic": "test_topic2"
    })";

    APSARA_TEST_TRUE(InitWithConfig(flusher2, configStr2));
    APSARA_TEST_TRUE(flusher2.Start());

    APSARA_TEST_TRUE(flusher1.Stop(false));
    APSARA_TEST_TRUE(flusher2.Stop(true));
}

void FlusherKafkaUnittest::TestSend() {
    auto& mock = KafkaProducerMock::GetInstance();


    {
        mock.State().produceResult = RD_KAFKA_RESP_ERR_NO_ERROR;
        FlusherKafka flusher;
        string configStr = R"({
            "Type": "flusher_kafka",
            "Brokers": ["localhost:9092"],
            "Topic": "test_topic"
        })";

        APSARA_TEST_TRUE(InitWithConfig(flusher, configStr));
        APSARA_TEST_TRUE(flusher.Start());

        PipelineEventGroup group = GenerateSimpleGroup();
        APSARA_TEST_TRUE(flusher.Send(std::move(group)));
        APSARA_TEST_EQUAL(1, mock.State().produceCalled);
        APSARA_TEST_EQUAL(1, flusher.mSendCnt->GetValue());

        APSARA_TEST_TRUE(flusher.Stop(true));
    }

    mock.Reset();


    {
        mock.State().produceResult = RD_KAFKA_RESP_ERR_MSG_SIZE_TOO_LARGE;
        FlusherKafka flusher;
        string configStr = R"({
            "Type": "flusher_kafka",
            "Brokers": ["localhost:9092"],
            "Topic": "test_topic"
        })";

        APSARA_TEST_TRUE(InitWithConfig(flusher, configStr));
        APSARA_TEST_TRUE(flusher.Start());

        PipelineEventGroup group = GenerateSimpleGroup();
        APSARA_TEST_FALSE(flusher.Send(std::move(group)));
        APSARA_TEST_EQUAL(1, mock.State().produceCalled);
        APSARA_TEST_EQUAL(1, flusher.mParamsErrorCnt->GetValue());

        APSARA_TEST_TRUE(flusher.Stop(true));
    }
}

void FlusherKafkaUnittest::TestFlush() {
    auto& mock = KafkaProducerMock::GetInstance();


    {
        mock.State().flushResult = RD_KAFKA_RESP_ERR_NO_ERROR;
        FlusherKafka flusher;
        string configStr = R"({
            "Type": "flusher_kafka",
            "Brokers": ["localhost:9092"],
            "Topic": "test_topic"
        })";

        APSARA_TEST_TRUE(InitWithConfig(flusher, configStr));
        APSARA_TEST_TRUE(flusher.Start());

        APSARA_TEST_TRUE(flusher.Flush(0));
        APSARA_TEST_EQUAL(1, mock.State().flushCalled);

        APSARA_TEST_TRUE(flusher.Stop(true));
    }

    mock.Reset();


    {
        mock.State().flushResult = RD_KAFKA_RESP_ERR__TRANSPORT;
        FlusherKafka flusher;
        string configStr = R"({
            "Type": "flusher_kafka",
            "Brokers": ["localhost:9092"],
            "Topic": "test_topic"
        })";

        APSARA_TEST_TRUE(InitWithConfig(flusher, configStr));
        APSARA_TEST_TRUE(flusher.Start());

        APSARA_TEST_FALSE(flusher.Flush(0));
        APSARA_TEST_EQUAL(1, mock.State().flushCalled);

        APSARA_TEST_TRUE(flusher.Stop(true));
    }
}

void FlusherKafkaUnittest::TestFlushAll() {
    auto& mock = KafkaProducerMock::GetInstance();

    mock.State().flushResult = RD_KAFKA_RESP_ERR_NO_ERROR;
    FlusherKafka flusher;
    string configStr = R"({
        "Type": "flusher_kafka",
        "Brokers": ["localhost:9092"],
        "Topic": "test_topic"
    })";

    APSARA_TEST_TRUE(InitWithConfig(flusher, configStr));
    APSARA_TEST_TRUE(flusher.Start());

    APSARA_TEST_TRUE(flusher.FlushAll());
    APSARA_TEST_EQUAL(1, mock.State().flushCalled);

    APSARA_TEST_TRUE(flusher.Stop(true));
}

void FlusherKafkaUnittest::TestConfigValidation() {
    auto& mock = KafkaProducerMock::GetInstance();


    {
        FlusherKafka flusher;
        string configStr = R"({
            "Type": "flusher_kafka",
            "Brokers": ["localhost:9092"],
            "Topic": "test_topic",
            "BatchNumMessages": 100,
            "LingerMs": 50
        })";

        APSARA_TEST_TRUE(InitWithConfig(flusher, configStr));
        APSARA_TEST_EQUAL(100U, flusher.mBatchNumMessages);
        APSARA_TEST_EQUAL(50U, flusher.mLingerMs);


        APSARA_TEST_EQUAL("100", mock.State().configSettings["batch.num.messages"]);
        APSARA_TEST_EQUAL("50", mock.State().configSettings["linger.ms"]);
    }


    {
        FlusherKafka flusher;
        string configStr = R"({
            "Type": "flusher_kafka",
            "Brokers": ["localhost:9092"],
            "Topic": "test_topic",
            "ClientID": "test_client"
        })";

        APSARA_TEST_TRUE(InitWithConfig(flusher, configStr));
        APSARA_TEST_EQUAL("test_client", flusher.mClientID);
        APSARA_TEST_EQUAL("test_client", mock.State().configSettings["client.id"]);
    }
}

void FlusherKafkaUnittest::TestSerializeFormat() {
    FlusherKafka flusher;
    string configStr = R"({
        "Type": "flusher_kafka",
        "Brokers": ["localhost:9092"],
        "Topic": "test_topic"
    })";

    APSARA_TEST_TRUE(InitWithConfig(flusher, configStr));
    APSARA_TEST_TRUE(flusher.Start());


    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetMetadata(EventGroupMetaKey::SOURCE_ID, StringView("test-source"));
    group.SetTag(LOG_RESERVED_KEY_SOURCE, "192.168.1.1");
    group.SetTag(LOG_RESERVED_KEY_MACHINE_UUID, "test-uuid");
    group.SetTag(LOG_RESERVED_KEY_TOPIC, "test-topic");

    auto logEvent = group.AddLogEvent();
    logEvent->SetTimestamp(1234567890);
    logEvent->SetContent(StringView("key1"), StringView("value1"));
    logEvent->SetContent(StringView("key2"), StringView("value2"));

    APSARA_TEST_TRUE(flusher.Send(std::move(group)));


    auto& mock = KafkaProducerMock::GetInstance();
    APSARA_TEST_EQUAL(1, mock.State().produceHistory.size());

    const auto& [key, value] = mock.State().produceHistory[0];
    APSARA_TEST_TRUE(!value.empty());

    APSARA_TEST_TRUE(flusher.Stop(true));
}

UNIT_TEST_CASE(FlusherKafkaUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(FlusherKafkaUnittest, OnFailedInit)
UNIT_TEST_CASE(FlusherKafkaUnittest, OnPipelineUpdate)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSend)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestFlush)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestFlushAll)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestConfigValidation)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSerializeFormat)

} // namespace logtail

UNIT_TEST_MAIN

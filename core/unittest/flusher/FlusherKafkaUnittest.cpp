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
#include <librdkafka/rdkafka_mock.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/memory/SourceBuffer.h"
#include "models/LogEvent.h"
#include "models/PipelineEventGroup.h"
#include "plugin/flusher/kafka/FlusherKafka.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

Json::Value CreateKafkaTestConfig(const std::string& topic) {
    Json::Value config;
    config["Brokers"] = Json::Value(Json::arrayValue);
    config["Brokers"].append("test.mock.brokers");
    config["Topic"] = topic;
    config["Kafka"] = Json::Value(Json::objectValue);
    config["Kafka"]["test.mock.num.brokers"] = "3";
    return config;
}

class FlusherKafkaUnittest : public ::testing::Test {
public:
    void TestInitSuccess();
    void TestInitMissingBrokers();
    void TestInitMissingTopic();
    void TestSendSuccess();
    void TestStartStop();

protected:
    void SetUp();
    void TearDown();

private:
    FlusherKafka* mFlusher = nullptr;
    CollectionPipelineContext* mContext = nullptr;
    string mTopic = "test_topic";

    bool EnsureTopicMetadataReady(const std::string& topic, int timeoutMs = 10000);
    bool WaitForProducerReady(int timeoutMs = 5000);
    bool WaitForMessageDelivery(uint64_t expectedSendDoneCount, int timeoutMs = 5000);
    bool WaitForFlushComplete(int timeoutMs = 10000);

    template <typename T>
    bool WaitForMetricValue(const T& metric, uint64_t expectedValue, int timeoutMs = 5000, int checkIntervalMs = 50);
};

void FlusherKafkaUnittest::SetUp() {
    mContext = new CollectionPipelineContext();
    mContext->SetConfigName("test_config");

    mFlusher = new FlusherKafka();
    mFlusher->SetContext(*mContext);

    mFlusher->CreateMetricsRecordRef(FlusherKafka::sName, "1");
}

void FlusherKafkaUnittest::TearDown() {
    if (mFlusher) {
        if (mFlusher->mIsRunning.load()) {
            WaitForFlushComplete(5000);


            mFlusher->Stop(true);
        }


        mFlusher->CommitMetricsRecordRef();
        delete mFlusher;
        mFlusher = nullptr;
    }
    if (mContext) {
        delete mContext;
        mContext = nullptr;
    }
}


bool FlusherKafkaUnittest::EnsureTopicMetadataReady(const std::string& topic, int timeoutMs) {
    rd_kafka_t* rk = mFlusher->mProducer;
    rd_kafka_mock_cluster_t* mockCluster = rd_kafka_handle_mock_cluster(rk);

    if (!mockCluster) {
        return false;
    }

    rd_kafka_mock_topic_create(mockCluster, topic.c_str(), 1, 1);

    const struct rd_kafka_metadata* metadata = nullptr;
    rd_kafka_topic_t* rkt = rd_kafka_topic_new(rk, topic.c_str(), nullptr);
    if (!rkt) {
        return false;
    }

    auto startTime = std::chrono::steady_clock::now();
    bool topicReady = false;

    while (
        !topicReady
        && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count()
            < timeoutMs) {
        rd_kafka_resp_err_t err = rd_kafka_metadata(rk, 0, rkt, &metadata, 1000);
        if (err == RD_KAFKA_RESP_ERR_NO_ERROR && metadata) {
            for (int i = 0; i < metadata->topic_cnt; i++) {
                const rd_kafka_metadata_topic_t* topicMeta = &metadata->topics[i];
                if (strcmp(topicMeta->topic, topic.c_str()) == 0 && topicMeta->partition_cnt > 0
                    && topicMeta->err == RD_KAFKA_RESP_ERR_NO_ERROR) {
                    topicReady = true;
                    break;
                }
            }
            rd_kafka_metadata_destroy(metadata);
            metadata = nullptr;
        }

        if (!topicReady) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    rd_kafka_topic_destroy(rkt);
    return topicReady;
}


bool FlusherKafkaUnittest::WaitForProducerReady(int timeoutMs) {
    auto startTime = std::chrono::steady_clock::now();

    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count()
           < timeoutMs) {
        rd_kafka_poll(mFlusher->mProducer, 10);

        const struct rd_kafka_metadata* metadata;
        rd_kafka_resp_err_t err = rd_kafka_metadata(mFlusher->mProducer, 1, nullptr, &metadata, 1000);
        if (err == RD_KAFKA_RESP_ERR_NO_ERROR && metadata && metadata->broker_cnt > 0) {
            rd_kafka_metadata_destroy(metadata);
            return true;
        }
        if (metadata) {
            rd_kafka_metadata_destroy(metadata);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}


template <typename T>
bool FlusherKafkaUnittest::WaitForMetricValue(const T& metric,
                                              uint64_t expectedValue,
                                              int timeoutMs,
                                              int checkIntervalMs) {
    auto startTime = std::chrono::steady_clock::now();

    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count()
           < timeoutMs) {
        rd_kafka_poll(mFlusher->mProducer, 10);

        if (metric->GetValue() == expectedValue) {
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(checkIntervalMs));
    }

    return false;
}


bool FlusherKafkaUnittest::WaitForMessageDelivery(uint64_t expectedSendDoneCount, int timeoutMs) {
    return WaitForMetricValue(mFlusher->mSendDoneCnt, expectedSendDoneCount, timeoutMs);
}


bool FlusherKafkaUnittest::WaitForFlushComplete(int timeoutMs) {
    auto startTime = std::chrono::steady_clock::now();

    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count()
           < timeoutMs) {
        rd_kafka_poll(mFlusher->mProducer, 10);

        if (mFlusher->FlushAll()) {
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

void FlusherKafkaUnittest::TestInitSuccess() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);

    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
}

void FlusherKafkaUnittest::TestInitMissingBrokers() {
    Json::Value config;
    Json::Value optionalGoPipeline;
    config["Topic"] = mTopic;
    APSARA_TEST_FALSE(mFlusher->Init(config, optionalGoPipeline));
}

void FlusherKafkaUnittest::TestInitMissingTopic() {
    Json::Value config;
    Json::Value optionalGoPipeline;
    config["Brokers"] = Json::Value(Json::arrayValue);
    config["Brokers"].append("dummy:9092");
    APSARA_TEST_FALSE(mFlusher->Init(config, optionalGoPipeline));
}

void FlusherKafkaUnittest::TestSendSuccess() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);

    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
    APSARA_TEST_TRUE(EnsureTopicMetadataReady(mTopic));
    APSARA_TEST_TRUE(mFlusher->Start());
    APSARA_TEST_TRUE(WaitForProducerReady());

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup group(sourceBuffer);
    auto* event = group.AddLogEvent();
    event->SetContent(StringView("key"), StringView("value"));

    APSARA_TEST_TRUE(mFlusher->Send(std::move(group)));
    APSARA_TEST_TRUE(WaitForMessageDelivery(1));
    APSARA_TEST_TRUE(WaitForFlushComplete());
    APSARA_TEST_EQUAL(1, mFlusher->mSendCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mSendDoneCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mSuccessCnt->GetValue());
}

void FlusherKafkaUnittest::TestStartStop() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);

    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
    APSARA_TEST_TRUE(mFlusher->Start());
    APSARA_TEST_TRUE(mFlusher->mIsRunning.load());
    APSARA_TEST_TRUE(mFlusher->Stop(true));
    APSARA_TEST_FALSE(mFlusher->mIsRunning.load());
}

UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitSuccess)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitMissingBrokers)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitMissingTopic)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSendSuccess)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestStartStop)

} // namespace logtail

UNIT_TEST_MAIN

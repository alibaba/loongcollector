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

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/serializer/JsonSerializer.h"
#include "common/memory/SourceBuffer.h"
#include "models/LogEvent.h"
#include "models/PipelineEventGroup.h"
#include "plugin/flusher/kafka/FlusherKafka.h"
#include "plugin/flusher/kafka/KafkaConfig.h"
#include "plugin/flusher/kafka/KafkaProducer.h"
#include "unittest/Unittest.h"


using namespace std;

namespace logtail {

struct ProduceRequest {
    std::string Topic;
    std::string Value;
    KafkaProducer::Callback Callback;
};

class MockKafkaProducer : public KafkaProducer {
public:
    MockKafkaProducer() = default;
    ~MockKafkaProducer() override = default;

    bool Init(const KafkaConfig& config) override {
        mInitialized = true;
        mConfig = config;
        return mInitSuccess;
    }

    void ProduceAsync(const std::string& topic, std::string&& value, Callback callback) override {
        ProduceRequest request{topic, std::move(value), std::move(callback)};
        mRequests.emplace_back(std::move(request));

        if (mAutoComplete) {
            CompleteLastRequest();
        }
    }

    bool Flush(int timeoutMs) override {
        mFlushCalled = true;
        return mFlushSuccess;
    }

    void Close() override { mClosed = true; }

    void SetInitSuccess(bool success) { mInitSuccess = success; }
    void SetFlushSuccess(bool success) { mFlushSuccess = success; }
    void SetAutoComplete(bool autoComplete) { mAutoComplete = autoComplete; }
    void CompleteLastRequest(bool success = true, const std::string& errorMsg = "") {
        KafkaProducer::ErrorInfo errorInfo;
        errorInfo.type = success ? KafkaProducer::ErrorType::SUCCESS : KafkaProducer::ErrorType::OTHER_ERROR;
        errorInfo.message = errorMsg;
        errorInfo.code = success ? 0 : -1;
        CompleteLastRequest(success, errorInfo);
    }

    void CompleteLastRequest(bool success, const KafkaProducer::ErrorInfo& errorInfo) {
        if (!mRequests.empty()) {
            auto& last = mRequests.back();
            last.Callback(success, errorInfo);
            mCompletedRequests.emplace_back(std::move(last));
            mRequests.pop_back();
        }
    }

    void CompleteAllRequests(bool success = true, const std::string& errorMsg = "") {
        KafkaProducer::ErrorInfo errorInfo;
        errorInfo.type = success ? KafkaProducer::ErrorType::SUCCESS : KafkaProducer::ErrorType::OTHER_ERROR;
        errorInfo.message = errorMsg;
        errorInfo.code = success ? 0 : -1;
        CompleteAllRequests(success, errorInfo);
    }

    void CompleteAllRequests(bool success, const KafkaProducer::ErrorInfo& errorInfo) {
        for (auto& request : mRequests) {
            request.Callback(success, errorInfo);
            mCompletedRequests.emplace_back(std::move(request));
        }
        mRequests.clear();
    }

    bool IsInitialized() const { return mInitialized; }
    bool IsClosed() const { return mClosed; }
    bool IsFlushCalled() const { return mFlushCalled; }
    const KafkaConfig& GetConfig() const { return mConfig; }
    const std::vector<ProduceRequest>& GetRequests() const { return mRequests; }
    const std::vector<ProduceRequest>& GetCompletedRequests() const { return mCompletedRequests; }
    size_t GetRequestCount() const { return mRequests.size() + mCompletedRequests.size(); }

private:
    bool mInitialized = false;
    bool mClosed = false;
    bool mFlushCalled = false;
    bool mInitSuccess = true;
    bool mFlushSuccess = true;
    bool mAutoComplete = true;

    KafkaConfig mConfig;
    std::vector<ProduceRequest> mRequests;
    std::vector<ProduceRequest> mCompletedRequests;
};

class MockEventGroupSerializer : public EventGroupSerializer {
public:
    MockEventGroupSerializer(Flusher* flusher) : EventGroupSerializer(flusher), mShouldFail(false) {}

    bool Serialize(BatchedEvents&& group, std::string& res, std::string& errorMsg) override {
        if (mShouldFail) {
            errorMsg = "mock serialization error";
            return false;
        }
        res = "serialized_data";
        return true;
    }

    void SetShouldFail(bool fail) { mShouldFail = fail; }

private:
    bool mShouldFail;
};


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
    void TestSendFailure();
    void TestStartStop();
    void TestFlush();
    void TestInitProducerFailure();
    void TestSendNetworkError();
    void TestSendAuthError();
    void TestSendServerError();
    void TestSendParamsError();
    void TestSendQueueFullError();
    void TestFlushFailure();
    void TestInitWithFullConfig();
    void TestSendOnUnstarted();
    void TestSendSerializationFailure();

protected:
    void SetUp();
    void TearDown();

private:
    FlusherKafka* mFlusher = nullptr;
    CollectionPipelineContext* mContext = nullptr;

    MockKafkaProducer* mMockProducer = nullptr;
    string mTopic = "test_topic";
};

void FlusherKafkaUnittest::SetUp() {
    mContext = new CollectionPipelineContext();
    mContext->SetConfigName("test_config");

    mFlusher = new FlusherKafka();
    auto mockProducer = std::make_unique<MockKafkaProducer>();
    mMockProducer = mockProducer.get();

    mFlusher->SetProducerForTest(std::move(mockProducer));
    mFlusher->SetContext(*mContext);
    mFlusher->CreateMetricsRecordRef(FlusherKafka::sName, "1");
}

void FlusherKafkaUnittest::TearDown() {
    if (mFlusher) {
        mFlusher->Stop(true);
        mFlusher->CommitMetricsRecordRef();
        delete mFlusher;
        mFlusher = nullptr;
    }
    if (mContext) {
        delete mContext;
        mContext = nullptr;
    }
}

void FlusherKafkaUnittest::TestInitSuccess() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);

    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
    APSARA_TEST_EQUAL(mTopic, mFlusher->mKafkaConfig.Topic);
    APSARA_TEST_EQUAL(1, mFlusher->mKafkaConfig.Brokers.size());
    APSARA_TEST_EQUAL("test.mock.brokers", mFlusher->mKafkaConfig.Brokers[0]);
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
    APSARA_TEST_TRUE(mFlusher->Start());

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup group(sourceBuffer);
    auto* event = group.AddLogEvent();
    event->SetContent(StringView("key"), StringView("value"));

    APSARA_TEST_TRUE(mFlusher->Send(std::move(group)));

    APSARA_TEST_EQUAL(1, mFlusher->mSendCnt->GetValue());
}

void FlusherKafkaUnittest::TestSendFailure() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);
    mFlusher->Init(config, optionalGoPipeline);
    mFlusher->Start();

    PipelineEventGroup group(std::make_shared<SourceBuffer>());
    auto* event = group.AddLogEvent();
    event->SetContent(StringView("key"), StringView("value"));


    mMockProducer->SetAutoComplete(false);
    mFlusher->Send(std::move(group));
    mMockProducer->CompleteLastRequest(false, {KafkaProducer::ErrorType::OTHER_ERROR, "mock general error", -1});


    APSARA_TEST_EQUAL(1, mFlusher->mSendCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mSendDoneCnt->GetValue());
    APSARA_TEST_EQUAL(0, mFlusher->mSuccessCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mOtherErrorCnt->GetValue());
}

void FlusherKafkaUnittest::TestStartStop() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);

    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
    APSARA_TEST_TRUE(mFlusher->Start());
    APSARA_TEST_TRUE(mFlusher->Stop(true));
}

void FlusherKafkaUnittest::TestFlush() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);

    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
    APSARA_TEST_TRUE(mFlusher->Flush(0));
    APSARA_TEST_TRUE(mFlusher->FlushAll());
}

void FlusherKafkaUnittest::TestInitProducerFailure() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);
    mMockProducer->SetInitSuccess(false);

    APSARA_TEST_FALSE(mFlusher->Init(config, optionalGoPipeline));
}

void FlusherKafkaUnittest::TestSendNetworkError() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);
    mFlusher->Init(config, optionalGoPipeline);
    mFlusher->Start();

    PipelineEventGroup group(std::make_shared<SourceBuffer>());
    auto* event = group.AddLogEvent();
    event->SetContent(StringView("key"), StringView("value"));

    mMockProducer->SetAutoComplete(false);
    mFlusher->Send(std::move(group));
    mMockProducer->CompleteLastRequest(false, {KafkaProducer::ErrorType::NETWORK_ERROR, "mock network error", 0});

    APSARA_TEST_EQUAL(1, mFlusher->mSendCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mSendDoneCnt->GetValue());
    APSARA_TEST_EQUAL(0, mFlusher->mSuccessCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mNetworkErrorCnt->GetValue());
}

void FlusherKafkaUnittest::TestSendAuthError() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);
    mFlusher->Init(config, optionalGoPipeline);
    mFlusher->Start();

    PipelineEventGroup group(std::make_shared<SourceBuffer>());
    auto* event = group.AddLogEvent();
    event->SetContent(StringView("key"), StringView("value"));

    mMockProducer->SetAutoComplete(false);
    mFlusher->Send(std::move(group));
    mMockProducer->CompleteLastRequest(false, {KafkaProducer::ErrorType::AUTH_ERROR, "mock auth error", 0});

    APSARA_TEST_EQUAL(1, mFlusher->mSendCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mSendDoneCnt->GetValue());
    APSARA_TEST_EQUAL(0, mFlusher->mSuccessCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mUnauthErrorCnt->GetValue());
}

void FlusherKafkaUnittest::TestSendServerError() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);
    mFlusher->Init(config, optionalGoPipeline);
    mFlusher->Start();

    PipelineEventGroup group(std::make_shared<SourceBuffer>());
    auto* event = group.AddLogEvent();
    event->SetContent(StringView("key"), StringView("value"));

    mMockProducer->SetAutoComplete(false);
    mFlusher->Send(std::move(group));
    mMockProducer->CompleteLastRequest(false, {KafkaProducer::ErrorType::SERVER_ERROR, "mock server error", 0});

    APSARA_TEST_EQUAL(1, mFlusher->mSendCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mSendDoneCnt->GetValue());
    APSARA_TEST_EQUAL(0, mFlusher->mSuccessCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mServerErrorCnt->GetValue());
}

void FlusherKafkaUnittest::TestSendParamsError() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);
    mFlusher->Init(config, optionalGoPipeline);
    mFlusher->Start();

    PipelineEventGroup group(std::make_shared<SourceBuffer>());
    auto* event = group.AddLogEvent();
    event->SetContent(StringView("key"), StringView("value"));

    mMockProducer->SetAutoComplete(false);
    mFlusher->Send(std::move(group));
    mMockProducer->CompleteLastRequest(false, {KafkaProducer::ErrorType::PARAMS_ERROR, "mock params error", 0});

    APSARA_TEST_EQUAL(1, mFlusher->mSendCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mSendDoneCnt->GetValue());
    APSARA_TEST_EQUAL(0, mFlusher->mSuccessCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mParamsErrorCnt->GetValue());
}

void FlusherKafkaUnittest::TestSendQueueFullError() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);
    mFlusher->Init(config, optionalGoPipeline);
    mFlusher->Start();

    PipelineEventGroup group(std::make_shared<SourceBuffer>());
    auto* event = group.AddLogEvent();
    event->SetContent(StringView("key"), StringView("value"));

    mMockProducer->SetAutoComplete(false);
    mFlusher->Send(std::move(group));
    mMockProducer->CompleteLastRequest(false, {KafkaProducer::ErrorType::QUEUE_FULL, "mock queue full error", 0});

    APSARA_TEST_EQUAL(1, mFlusher->mSendCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mSendDoneCnt->GetValue());
    APSARA_TEST_EQUAL(0, mFlusher->mSuccessCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mDiscardCnt->GetValue());
}

void FlusherKafkaUnittest::TestFlushFailure() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);
    mFlusher->Init(config, optionalGoPipeline);
    mMockProducer->SetFlushSuccess(false);

    APSARA_TEST_FALSE(mFlusher->Flush(0));
    APSARA_TEST_TRUE(mMockProducer->IsFlushCalled());
}

void FlusherKafkaUnittest::TestInitWithFullConfig() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);
    config["Producer"]["BatchSize"] = 2000000;
    config["Producer"]["LingerMs"] = 10;
    config["Delivery"]["Acks"] = "all";
    config["Delivery"]["RetryBackoffMs"] = 2000;

    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
    APSARA_TEST_EQUAL(2000000, mFlusher->mKafkaConfig.Producer.BatchSize);
    APSARA_TEST_EQUAL(10, mFlusher->mKafkaConfig.Producer.LingerMs);
    APSARA_TEST_EQUAL("all", mFlusher->mKafkaConfig.Delivery.Acks);
    APSARA_TEST_EQUAL(2000, mFlusher->mKafkaConfig.Delivery.RetryBackoffMs);
}

void FlusherKafkaUnittest::TestSendSerializationFailure() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);
    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));

    auto mockSerializer = std::make_unique<MockEventGroupSerializer>(mFlusher);
    mockSerializer->SetShouldFail(true);
    mFlusher->SetSerializerForTest(std::move(mockSerializer));

    PipelineEventGroup group(std::make_shared<SourceBuffer>());
    auto* event = group.AddLogEvent();
    event->SetContent(StringView("key"), StringView("value"));

    APSARA_TEST_FALSE(mFlusher->Send(std::move(group)));
    APSARA_TEST_EQUAL(1, mFlusher->mDiscardCnt->GetValue());
}

UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitSuccess)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitMissingBrokers)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitMissingTopic)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSendSuccess)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSendFailure)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestStartStop)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestFlush)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitProducerFailure)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSendNetworkError)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSendAuthError)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSendServerError)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSendParamsError)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSendQueueFullError)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestFlushFailure)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitWithFullConfig)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSendSerializationFailure)

} // namespace logtail

UNIT_TEST_MAIN

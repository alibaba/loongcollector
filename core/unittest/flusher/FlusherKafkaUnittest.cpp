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
#include <set>
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
#include "unittest/flusher/MockKafkaProducer.h"


using namespace std;

namespace logtail {

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
    config["Version"] = "2.6.0";
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
    void TestInitMissingKafkaVersion();
    void TestInitWithFullConfig();
    void TestSendOnUnstarted();
    void TestSendSerializationFailure();
    void TestDynamicTopic_Success();
    void TestDynamicTopic_FallbackToStatic();
    void TestDynamicTopic_FromTags();
    void TestPartitionerHashConfigValidation();
    void TestPartitionerHashKeySend();
    void TestHeadersConfigured_SendWithHeaders();
    void TestInitWithTLSMinimal();
    void TestInitWithTLSFullPaths();
    void TestPartitionerHashKeyInvalidPrefix();
    void TestUnknownPartitionerType();
    void TestGeneratePartitionKey_NotHash();
    void TestGeneratePartitionKey_ShortKeyAndJoinAndNonLog();
    void TestInitTLSCertKeyMismatch();
    void TestInitWithKerberosMinimal();
    void TestInitWithKerberosAndTLS();
    void TestInitWithKerberosFull();
    void TestInitWithCompression();
    void TestInitWithCompressionAndLevel();

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
    config["Version"] = "2.6.0";
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

void FlusherKafkaUnittest::TestInitMissingKafkaVersion() {
    Json::Value optionalGoPipeline;
    Json::Value config;
    config["Brokers"] = Json::Value(Json::arrayValue);
    config["Brokers"].append("dummy:9092");
    config["Topic"] = mTopic;
    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
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
    config["BulkFlushFrequency"] = 10;
    config["RequiredAcks"] = -1;
    config["RetryBackoffMs"] = 2000;

    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
    APSARA_TEST_EQUAL(10, mFlusher->mKafkaConfig.BulkFlushFrequency);
    APSARA_TEST_EQUAL(-1, mFlusher->mKafkaConfig.RequiredAcks);
    APSARA_TEST_EQUAL(2000, mFlusher->mKafkaConfig.RetryBackoffMs);
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

void FlusherKafkaUnittest::TestDynamicTopic_Success() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig("test_%{content.application}");
    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
    APSARA_TEST_TRUE(mFlusher->Start());

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup group(sourceBuffer);
    auto* event = group.AddLogEvent();
    event->SetContent(StringView("application"), StringView("user_behavior_log"));

    APSARA_TEST_TRUE(mFlusher->Send(std::move(group)));
    const auto& completed = mMockProducer->GetCompletedRequests();
    APSARA_TEST_EQUAL(1, completed.size());
    APSARA_TEST_EQUAL(std::string("test_user_behavior_log"), completed.back().Topic);
    APSARA_TEST_EQUAL(1, mFlusher->mSendCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mSendDoneCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mSuccessCnt->GetValue());
}

void FlusherKafkaUnittest::TestDynamicTopic_FallbackToStatic() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig("test_%{content.application}");
    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
    APSARA_TEST_TRUE(mFlusher->Start());

    PipelineEventGroup group(std::make_shared<SourceBuffer>());
    auto* event = group.AddLogEvent();
    event->SetContent(StringView("key"), StringView("value"));

    APSARA_TEST_TRUE(mFlusher->Send(std::move(group)));
    const auto& completed = mMockProducer->GetCompletedRequests();
    APSARA_TEST_EQUAL(1, completed.size());
    APSARA_TEST_EQUAL(std::string("test_%{content.application}"), completed.back().Topic);
    APSARA_TEST_EQUAL(1, mFlusher->mSendCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mSendDoneCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mSuccessCnt->GetValue());
}

void FlusherKafkaUnittest::TestDynamicTopic_FromTags() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig("logs_%{tag.namespace}");
    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
    APSARA_TEST_TRUE(mFlusher->Start());

    PipelineEventGroup group(std::make_shared<SourceBuffer>());
    auto* event = group.AddLogEvent();
    event->SetContent(StringView("key"), StringView("value"));
    group.SetTag(StringView("namespace"), StringView("nginx_access_log"));

    APSARA_TEST_TRUE(mFlusher->Send(std::move(group)));
    const auto& completed = mMockProducer->GetCompletedRequests();
    APSARA_TEST_EQUAL(1, completed.size());
    APSARA_TEST_EQUAL(std::string("logs_nginx_access_log"), completed.back().Topic);
    APSARA_TEST_EQUAL(1, mFlusher->mSendCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mSendDoneCnt->GetValue());
    APSARA_TEST_EQUAL(1, mFlusher->mSuccessCnt->GetValue());
}

void FlusherKafkaUnittest::TestPartitionerHashConfigValidation() {
    Json::Value optionalGoPipeline;
    Json::Value config;
    config["Brokers"] = Json::Value(Json::arrayValue);
    config["Brokers"].append("dummy:9092");
    config["Topic"] = mTopic;
    config["Version"] = "2.6.0";
    config["PartitionerType"] = "hash";
    APSARA_TEST_FALSE(mFlusher->Init(config, optionalGoPipeline));
}

void FlusherKafkaUnittest::TestPartitionerHashKeySend() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);
    config["PartitionerType"] = "hash";
    Json::Value hashKeys(Json::arrayValue);
    hashKeys.append("content.application");
    config["HashKeys"] = hashKeys;

    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
    APSARA_TEST_TRUE(mFlusher->Start());

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup group(sourceBuffer);
    auto* e1 = group.AddLogEvent();
    e1->SetContent(StringView("application"), StringView("serviceA"));
    auto* e2 = group.AddLogEvent();
    e2->SetContent(StringView("application"), StringView("serviceB"));

    APSARA_TEST_TRUE(mFlusher->Send(std::move(group)));
    APSARA_TEST_EQUAL(2, mFlusher->mSendCnt->GetValue());

    const auto& reqs = mMockProducer->GetCompletedRequests();
    APSARA_TEST_EQUAL(2U, reqs.size());
    std::set<std::string> keys;
    for (const auto& r : reqs) {
        keys.insert(r.Key);
    }
    APSARA_TEST_TRUE(keys.find("serviceA") != keys.end());
    APSARA_TEST_TRUE(keys.find("serviceB") != keys.end());
}
void FlusherKafkaUnittest::TestHeadersConfigured_SendWithHeaders() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);
    Json::Value headers(Json::arrayValue);
    {
        Json::Value h(Json::objectValue);
        h["key"] = "hk1";
        h["value"] = "hv1";
        headers.append(h);
    }
    {
        Json::Value h(Json::objectValue);
        h["key"] = "hk2";
        h["value"] = "hv2";
        headers.append(h);
    }
    config["Headers"] = headers;

    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
    APSARA_TEST_TRUE(mFlusher->Start());

    PipelineEventGroup group(std::make_shared<SourceBuffer>());
    auto* event = group.AddLogEvent();
    event->SetContent(StringView("k"), StringView("v"));

    APSARA_TEST_TRUE(mFlusher->Send(std::move(group)));

    const auto& completed = mMockProducer->GetCompletedRequests();
    APSARA_TEST_EQUAL(1U, completed.size());
    const auto& hdrs = completed.back().Headers;
    APSARA_TEST_EQUAL(2U, hdrs.size());
    APSARA_TEST_EQUAL(std::string("hk1"), hdrs[0].first);
    APSARA_TEST_EQUAL(std::string("hv1"), hdrs[0].second);
    APSARA_TEST_EQUAL(std::string("hk2"), hdrs[1].first);
    APSARA_TEST_EQUAL(std::string("hv2"), hdrs[1].second);
}
void FlusherKafkaUnittest::TestInitWithTLSMinimal() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig("tls-test");
    config["Authentication"]["TLS"]["Enabled"] = true;

    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
}

void FlusherKafkaUnittest::TestInitWithTLSFullPaths() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig("tls-test");
    config["Authentication"]["TLS"]["Enabled"] = true;
    config["Authentication"]["TLS"]["CAFile"] = "/tmp/does-not-need-to-exist.ca";
    config["Authentication"]["TLS"]["CertFile"] = "/tmp/does-not-need-to-exist.crt";
    config["Authentication"]["TLS"]["KeyFile"] = "/tmp/does-not-need-to-exist.key";
    config["Authentication"]["TLS"]["KeyPassword"] = "secret";

    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
}

void FlusherKafkaUnittest::TestInitWithKerberosMinimal() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig("krb-test");
    config["Authentication"]["Kerberos"]["Enabled"] = true;

    APSARA_TEST_FALSE(mFlusher->Init(config, optionalGoPipeline));
}

void FlusherKafkaUnittest::TestInitWithKerberosAndTLS() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig("krb-test");
    config["Authentication"]["Kerberos"]["Enabled"] = true;
    config["Authentication"]["TLS"]["Enabled"] = true;
    config["Authentication"]["TLS"]["CAFile"] = "/tmp/ca.pem";

    APSARA_TEST_FALSE(mFlusher->Init(config, optionalGoPipeline));
}

void FlusherKafkaUnittest::TestInitWithKerberosFull() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig("krb-test");
    config["Authentication"]["Kerberos"]["Enabled"] = true;
    config["Authentication"]["Kerberos"]["Mechanism"] = "GSSAPI";
    config["Authentication"]["Kerberos"]["ServiceName"] = "kafka";
    config["Authentication"]["Kerberos"]["Principal"] = "kafka/test@EXAMPLE.COM";
    config["Authentication"]["Kerberos"]["Keytab"] = "/tmp/test.keytab";
    config["Authentication"]["Kerberos"]["KinitCmd"] = "kinit -k -t %{sasl.kerberos.keytab} %{sasl.kerberos.principal}";

    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
}

void FlusherKafkaUnittest::TestPartitionerHashKeyInvalidPrefix() {
    Json::Value optionalGoPipeline;
    Json::Value config;
    config["Brokers"] = Json::Value(Json::arrayValue);
    config["Brokers"].append("dummy:9092");
    config["Topic"] = mTopic;
    config["Version"] = "2.6.0";
    config["PartitionerType"] = "hash";
    Json::Value keys(Json::arrayValue);
    keys.append("app"); // 非 content. 前缀，触发参数错误分支
    config["HashKeys"] = keys;

    APSARA_TEST_FALSE(mFlusher->Init(config, optionalGoPipeline));
}

void FlusherKafkaUnittest::TestUnknownPartitionerType() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);
    config["PartitionerType"] = "unknown_partitioner";

    APSARA_TEST_FALSE(mFlusher->Init(config, optionalGoPipeline));
}

void FlusherKafkaUnittest::TestGeneratePartitionKey_NotHash() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);
    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));

    PipelineEventGroup group(std::make_shared<SourceBuffer>());
    auto* event = group.AddLogEvent();
    event->SetContent(StringView("k"), StringView("v"));
    const auto& ev = group.GetEvents()[0];

    APSARA_TEST_EQUAL(std::string(), mFlusher->GeneratePartitionKey(ev));
}

void FlusherKafkaUnittest::TestInitTLSCertKeyMismatch() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);
    config["Authentication"]["TLS"]["Enabled"] = true;
    config["Authentication"]["TLS"]["CertFile"] = "/tmp/dummy.crt";

    APSARA_TEST_FALSE(mFlusher->Init(config, optionalGoPipeline));
}

void FlusherKafkaUnittest::TestGeneratePartitionKey_ShortKeyAndJoinAndNonLog() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);
    config["PartitionerType"] = "hash";
    Json::Value keys(Json::arrayValue);
    keys.append("content.user");
    keys.append("content.");
    keys.append("content.app");
    config["HashKeys"] = keys;

    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));

    PipelineEventGroup g(std::make_shared<SourceBuffer>());
    auto* e = g.AddLogEvent();
    e->SetContent(StringView("user"), StringView("alice"));
    e->SetContent(StringView("app"), StringView("svc"));
    const auto& ev = g.GetEvents()[0];
    auto k = mFlusher->GeneratePartitionKey(ev);
    APSARA_TEST_EQUAL(std::string("alice###svc"), k);

    PipelineEventGroup g2(std::make_shared<SourceBuffer>());
    g2.AddMetricEvent();
    const auto& ev2 = g2.GetEvents()[0];
    auto k2 = mFlusher->GeneratePartitionKey(ev2);
    APSARA_TEST_EQUAL(std::string(""), k2);
}

void FlusherKafkaUnittest::TestInitWithCompression() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);
    config["Compression"] = "gzip";
    config["CompressionLevel"] = -1;

    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
    APSARA_TEST_EQUAL(std::string("gzip"), mFlusher->mKafkaConfig.Compression);
    APSARA_TEST_EQUAL(-1, mFlusher->mKafkaConfig.CompressionLevel);
}

void FlusherKafkaUnittest::TestInitWithCompressionAndLevel() {
    Json::Value optionalGoPipeline;
    Json::Value config = CreateKafkaTestConfig(mTopic);
    config["Compression"] = "lz4";
    config["CompressionLevel"] = 2;

    APSARA_TEST_TRUE(mFlusher->Init(config, optionalGoPipeline));
    APSARA_TEST_EQUAL(std::string("lz4"), mFlusher->mKafkaConfig.Compression);
    APSARA_TEST_EQUAL(2, mFlusher->mKafkaConfig.CompressionLevel);
}

UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitSuccess)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitMissingBrokers)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitMissingTopic)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSendSuccess)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSendFailure)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestStartStop)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestFlush)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitProducerFailure)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitMissingKafkaVersion)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSendNetworkError)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSendAuthError)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSendServerError)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSendParamsError)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSendQueueFullError)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestFlushFailure)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitWithFullConfig)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestSendSerializationFailure)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestDynamicTopic_Success)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestDynamicTopic_FallbackToStatic)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestDynamicTopic_FromTags)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestPartitionerHashConfigValidation)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestPartitionerHashKeySend)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestHeadersConfigured_SendWithHeaders)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestPartitionerHashKeyInvalidPrefix)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitWithTLSMinimal)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitWithTLSFullPaths)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitTLSCertKeyMismatch)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestUnknownPartitionerType)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestGeneratePartitionKey_NotHash)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestGeneratePartitionKey_ShortKeyAndJoinAndNonLog)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitWithKerberosMinimal)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitWithKerberosAndTLS)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitWithKerberosFull)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitWithCompression)
UNIT_TEST_CASE(FlusherKafkaUnittest, TestInitWithCompressionAndLevel)

} // namespace logtail

UNIT_TEST_MAIN

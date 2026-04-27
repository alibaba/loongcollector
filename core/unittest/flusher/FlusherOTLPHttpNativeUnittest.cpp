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

#include <json/value.h>

#include <memory>
#include <string>
#include <thread>

#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/http/HttpResponse.h"
#include "common/memory/SourceBuffer.h"
#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/MetricValue.h"
#include "models/PipelineEventGroup.h"
#include "models/SpanEvent.h"
#include "plugin/flusher/opentelemetry/FlusherOTLPHttpNative.h"
#include "runner/sink/http/HttpSinkRequest.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class FlusherOTLPHttpNativeUnittest : public testing::Test {
public:
    void SetUp() override {
        ctx.SetConfigName("test_config");
        ctx.SetPipeline(pipeline);
    }

    void TearDown() override {}

    void TestInit_ValidConfig();
    void TestInit_MissingUrl();
    void TestInit_WithTLSAndHeaders();
    void TestBuildRequestLogs();
    void TestBuildRequestMetrics();
    void TestBuildRequestTraces();
    void TestBuildRequest_EmptyData();
    void TestOnSendDone_Success();
    void TestOnSendDone_Failure();
    void TestFlusherName();
    void TestInit_ProtobufFormat();
    void TestBuildRequest_ProtobufContentType();
    void TestSerialize_ProtobufBody();

protected:
    CollectionPipeline pipeline;
    CollectionPipelineContext ctx;

    std::unique_ptr<FlusherOTLPHttpNative> CreateAndInitFlusher(const Json::Value& config) {
        auto flusher = std::make_unique<FlusherOTLPHttpNative>();
        flusher->SetContext(ctx);
        flusher->CreateMetricsRecordRef(FlusherOTLPHttpNative::sName, "1");

        Json::Value optionalGoPipeline;
        if (!flusher->Init(config, optionalGoPipeline)) {
            return nullptr;
        }
        flusher->CommitMetricsRecordRef();
        return flusher;
    }

    PipelineEventGroup MakeLogGroup() {
        auto group = PipelineEventGroup(std::make_shared<SourceBuffer>());
        auto* logEvent = group.AddLogEvent(true);
        logEvent->SetTimestamp(1748313840, 259486017);
        logEvent->SetContent(std::string("content"), std::string("Test log message"));
        group.SetTag(std::string("service.name"), std::string("unittest"));
        return group;
    }

    PipelineEventGroup MakeMetricGroup() {
        auto group = PipelineEventGroup(std::make_shared<SourceBuffer>());
        auto* metricEvent = group.AddMetricEvent(true);
        metricEvent->SetName("test.metric.value");
        auto* pt = metricEvent->MutableValue<UntypedSingleValue>();
        pt->mValue = 42.5;
        group.SetTag(std::string("service.name"), std::string("unittest"));
        return group;
    }

    PipelineEventGroup MakeSpanGroup() {
        auto group = PipelineEventGroup(std::make_shared<SourceBuffer>());
        auto* spanEvent = group.AddSpanEvent(true);
        spanEvent->SetTraceId("unittest1234567890abcdef12345678");
        spanEvent->SetSpanId("unittest12345678");
        spanEvent->SetParentSpanId("parent12345678901234");
        spanEvent->SetName("/test/api");
        spanEvent->SetKind(SpanEvent::Kind::Server);
        spanEvent->SetStatus(SpanEvent::StatusCode::Ok);
        spanEvent->SetStartTimeNs(1748313840259486017ULL);
        spanEvent->SetEndTimeNs(1748313840259765375ULL);
        spanEvent->SetTag(std::string("service.name"), std::string("unittest"));
        return group;
    }
};

void FlusherOTLPHttpNativeUnittest::TestInit_ValidConfig() {
    Json::Value config;
    config["Url"] = "http://localhost:4318/v1/logs";

    auto flusher = CreateAndInitFlusher(config);
    APSARA_TEST_TRUE(flusher != nullptr);
    APSARA_TEST_STREQ(flusher->GetUrl().c_str(), "http://localhost:4318/v1/logs");
}

void FlusherOTLPHttpNativeUnittest::TestInit_MissingUrl() {
    auto flusher = std::make_unique<FlusherOTLPHttpNative>();
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherOTLPHttpNative::sName, "1");

    Json::Value config;
    Json::Value optionalGoPipeline;

    APSARA_TEST_FALSE(flusher->Init(config, optionalGoPipeline));
}

void FlusherOTLPHttpNativeUnittest::TestInit_WithTLSAndHeaders() {
    Json::Value config;
    config["Url"] = "https://otel.example.com/v1/traces";
    config["EnableTLS"] = true;
    Json::Value headers;
    headers["Authorization"] = "Bearer test-token";
    headers["X-Custom-Header"] = "custom-value";
    config["Headers"] = headers;

    auto flusher = CreateAndInitFlusher(config);
    APSARA_TEST_TRUE(flusher != nullptr);
}

void FlusherOTLPHttpNativeUnittest::TestBuildRequestLogs() {
    Json::Value config;
    config["Url"] = "http://localhost:4318/v1/logs";
    config["Format"] = "json";
    Json::Value headers;
    headers["X-Test"] = "value";
    config["Headers"] = headers;

    auto flusher = CreateAndInitFlusher(config);
    APSARA_TEST_TRUE(flusher != nullptr);

    // Build a test SenderQueueItem with serialized OTLP JSON data
    std::string testData
        = R"({"resource_logs":[{"resource":{"attributes":[{"key":"service.name","value":{"string_value":"unittest"}}]},"scope_logs":[{"log_records":[{"time_unix_nano":"1748313840259486017","body":{"string_value":"Test log message"}}]}]}]})";
    SenderQueueItem testItem(std::string(testData), testData.size(), flusher.get(), 0, RawDataType::EVENT_GROUP);

    std::unique_ptr<HttpSinkRequest> req;
    bool keepItem = false;
    std::string errMsg;
    APSARA_TEST_TRUE(flusher->BuildRequest(&testItem, req, &keepItem, &errMsg));
    APSARA_TEST_TRUE(req != nullptr);
    APSARA_TEST_TRUE(keepItem);

    // Verify request properties
    APSARA_TEST_STREQ(req->mMethod.c_str(), "POST");
    APSARA_TEST_STREQ(req->mHost.c_str(), "localhost");
    APSARA_TEST_EQUAL(req->mPort, 4318);
    APSARA_TEST_STREQ(req->mUrl.c_str(), "/v1/logs");

    // Verify headers
    auto it = req->mHeader.find("Content-Type");
    APSARA_TEST_TRUE(it != req->mHeader.end());
    APSARA_TEST_STREQ(it->second.c_str(), "application/json");

    it = req->mHeader.find("X-Test");
    APSARA_TEST_TRUE(it != req->mHeader.end());
    APSARA_TEST_STREQ(it->second.c_str(), "value");

    // Verify body contains OTLP JSON structure
    APSARA_TEST_TRUE(req->mBody.find("resource_logs") != std::string::npos);
    APSARA_TEST_TRUE(req->mBody.find("Test log message") != std::string::npos);
}

void FlusherOTLPHttpNativeUnittest::TestBuildRequestMetrics() {
    Json::Value config;
    config["Url"] = "http://localhost:4318/v1/metrics";

    auto flusher = CreateAndInitFlusher(config);
    APSARA_TEST_TRUE(flusher != nullptr);

    std::string testData
        = R"({"resource_metrics":[{"resource":{"attributes":[{"key":"service.name","value":{"string_value":"unittest"}}]},"scope_metrics":[{"metrics":[{"name":"test.metric.value","gauge":{"data_points":[{"as_double":42.5}]}}]}]}]})";
    SenderQueueItem testItem(std::string(testData), testData.size(), flusher.get(), 0, RawDataType::EVENT_GROUP);

    std::unique_ptr<HttpSinkRequest> req;
    bool keepItem = false;
    std::string errMsg;
    APSARA_TEST_TRUE(flusher->BuildRequest(&testItem, req, &keepItem, &errMsg));
    APSARA_TEST_TRUE(req != nullptr);

    APSARA_TEST_STREQ(req->mHost.c_str(), "localhost");
    APSARA_TEST_EQUAL(req->mPort, 4318);
    APSARA_TEST_STREQ(req->mUrl.c_str(), "/v1/metrics");
    APSARA_TEST_TRUE(req->mBody.find("resource_metrics") != std::string::npos);
    APSARA_TEST_TRUE(req->mBody.find("test.metric.value") != std::string::npos);
    APSARA_TEST_TRUE(req->mBody.find("42.5") != std::string::npos);
}

void FlusherOTLPHttpNativeUnittest::TestBuildRequestTraces() {
    Json::Value config;
    config["Url"] = "http://localhost:4318/v1/traces";

    auto flusher = CreateAndInitFlusher(config);
    APSARA_TEST_TRUE(flusher != nullptr);

    std::string testData
        = R"({"resource_spans":[{"resource":{"attributes":[{"key":"service.name","value":{"string_value":"unittest"}}]},"scope_spans":[{"spans":[{"trace_id":"unittest1234567890abcdef12345678","span_id":"unittest12345678","name":"/test/api","kind":2,"status":{"code":1}}]}]}]})";
    SenderQueueItem testItem(std::string(testData), testData.size(), flusher.get(), 0, RawDataType::EVENT_GROUP);

    std::unique_ptr<HttpSinkRequest> req;
    bool keepItem = false;
    std::string errMsg;
    APSARA_TEST_TRUE(flusher->BuildRequest(&testItem, req, &keepItem, &errMsg));
    APSARA_TEST_TRUE(req != nullptr);

    APSARA_TEST_STREQ(req->mHost.c_str(), "localhost");
    APSARA_TEST_EQUAL(req->mPort, 4318);
    APSARA_TEST_STREQ(req->mUrl.c_str(), "/v1/traces");
    APSARA_TEST_TRUE(req->mBody.find("resource_spans") != std::string::npos);
    APSARA_TEST_TRUE(req->mBody.find("/test/api") != std::string::npos);
}

void FlusherOTLPHttpNativeUnittest::TestBuildRequest_EmptyData() {
    Json::Value config;
    config["Url"] = "http://localhost:4318/v1/logs";

    auto flusher = CreateAndInitFlusher(config);
    APSARA_TEST_TRUE(flusher != nullptr);

    std::string emptyData;
    SenderQueueItem testItem(std::string(emptyData), 0, flusher.get(), 0, RawDataType::EVENT_GROUP);

    std::unique_ptr<HttpSinkRequest> req;
    bool keepItem = true;
    std::string errMsg;
    APSARA_TEST_TRUE(flusher->BuildRequest(&testItem, req, &keepItem, &errMsg));
    APSARA_TEST_FALSE(keepItem); // empty data should be discarded
}

void FlusherOTLPHttpNativeUnittest::TestOnSendDone_Success() {
    Json::Value config;
    config["Url"] = "http://localhost:4318/v1/logs";

    auto flusher = CreateAndInitFlusher(config);
    APSARA_TEST_TRUE(flusher != nullptr);

    std::string testData = R"({"resource_logs":[]})";
    SenderQueueItem testItem(std::move(testData), testData.size(), flusher.get(), 0, RawDataType::EVENT_GROUP);

    // Simulate successful HTTP response (200)
    HttpResponse response;
    response.SetStatusCode(200);

    flusher->OnSendDone(response, &testItem);
    APSARA_TEST_EQUAL(1U, flusher->mSendSuccessCnt->GetValue());
}

void FlusherOTLPHttpNativeUnittest::TestOnSendDone_Failure() {
    Json::Value config;
    config["Url"] = "http://localhost:4318/v1/logs";

    auto flusher = CreateAndInitFlusher(config);
    APSARA_TEST_TRUE(flusher != nullptr);

    std::string testData = R"({"resource_logs":[]})";
    SenderQueueItem testItem(std::move(testData), testData.size(), flusher.get(), 0, RawDataType::EVENT_GROUP);

    // Simulate server error (503)
    HttpResponse response;
    response.SetStatusCode(503);

    flusher->OnSendDone(response, &testItem);
    APSARA_TEST_EQUAL(1U, flusher->mSendFailCnt->GetValue());
}

void FlusherOTLPHttpNativeUnittest::TestFlusherName() {
    APSARA_TEST_STREQ(FlusherOTLPHttpNative::sName.c_str(), "flusher_otlp_http_native");
}

void FlusherOTLPHttpNativeUnittest::TestInit_ProtobufFormat() {
    Json::Value config;
    config["Url"] = "http://localhost:4318/v1/logs";
    config["Format"] = "protobuf";

    auto flusher = CreateAndInitFlusher(config);
    APSARA_TEST_TRUE(flusher != nullptr);
    APSARA_TEST_TRUE(flusher->GetFormat() == OTLPHttpFormat::Protobuf);
}

void FlusherOTLPHttpNativeUnittest::TestBuildRequest_ProtobufContentType() {
    Json::Value config;
    config["Url"] = "http://localhost:4318/v1/logs";
    config["Format"] = "protobuf";

    auto flusher = CreateAndInitFlusher(config);
    APSARA_TEST_TRUE(flusher != nullptr);

    std::string testData = "binary-protobuf-data";
    SenderQueueItem testItem(std::string(testData), testData.size(), flusher.get(), 0, RawDataType::EVENT_GROUP);

    std::unique_ptr<HttpSinkRequest> req;
    bool keepItem = false;
    std::string errMsg;
    APSARA_TEST_TRUE(flusher->BuildRequest(&testItem, req, &keepItem, &errMsg));
    APSARA_TEST_TRUE(req != nullptr);

    auto it = req->mHeader.find("Content-Type");
    APSARA_TEST_TRUE(it != req->mHeader.end());
    APSARA_TEST_STREQ(it->second.c_str(), "application/x-protobuf");
}

void FlusherOTLPHttpNativeUnittest::TestSerialize_ProtobufBody() {
    Json::Value config;
    config["Url"] = "http://localhost:4318/v1/logs";
    config["Format"] = "protobuf";

    auto flusher = CreateAndInitFlusher(config);
    APSARA_TEST_TRUE(flusher != nullptr);

    auto group = MakeLogGroup();
    // Call Send which should trigger protobuf serialization
    APSARA_TEST_TRUE(flusher->Send(std::move(group)));
    APSARA_TEST_EQUAL(1U, flusher->mSendCnt->GetValue());
}

UNIT_TEST_CASE(FlusherOTLPHttpNativeUnittest, TestInit_ValidConfig)
UNIT_TEST_CASE(FlusherOTLPHttpNativeUnittest, TestInit_MissingUrl)
UNIT_TEST_CASE(FlusherOTLPHttpNativeUnittest, TestInit_WithTLSAndHeaders)
UNIT_TEST_CASE(FlusherOTLPHttpNativeUnittest, TestBuildRequestLogs)
UNIT_TEST_CASE(FlusherOTLPHttpNativeUnittest, TestBuildRequestMetrics)
UNIT_TEST_CASE(FlusherOTLPHttpNativeUnittest, TestBuildRequestTraces)
UNIT_TEST_CASE(FlusherOTLPHttpNativeUnittest, TestBuildRequest_EmptyData)
UNIT_TEST_CASE(FlusherOTLPHttpNativeUnittest, TestOnSendDone_Success)
UNIT_TEST_CASE(FlusherOTLPHttpNativeUnittest, TestOnSendDone_Failure)
UNIT_TEST_CASE(FlusherOTLPHttpNativeUnittest, TestFlusherName)
UNIT_TEST_CASE(FlusherOTLPHttpNativeUnittest, TestInit_ProtobufFormat)
UNIT_TEST_CASE(FlusherOTLPHttpNativeUnittest, TestBuildRequest_ProtobufContentType)
UNIT_TEST_CASE(FlusherOTLPHttpNativeUnittest, TestSerialize_ProtobufBody)

} // namespace logtail

UNIT_TEST_MAIN

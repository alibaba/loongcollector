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

#include <grpcpp/grpcpp.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "collection_pipeline/queue/QueueKeyManager.h"
#include "forward/otlp/OTLPForwardService.h"
#include "json/reader.h"
#include "logger/Logger.h"
#include "protobuf/opentelemetry/proto/collector/logs/v1/logs_service.grpc.pb.h"
#include "protobuf/opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "protobuf/opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.h"
#include "unittest/Unittest.h"

using namespace std;
using namespace opentelemetry::proto::collector::logs::v1;
using namespace opentelemetry::proto::collector::metrics::v1;
using namespace opentelemetry::proto::collector::trace::v1;

namespace logtail {

class OTLPForwardServiceUnittest : public testing::Test {
public:
    void SetUp() override {
        mServerAddress = "0.0.0.0:15899";
        // Create a dummy queue for PushQueue to work
        mQueueKey = QueueKeyManager::GetInstance()->GetKey("test-queue-key");
        CollectionPipelineContext dummyCtx;
        ProcessQueueManager::GetInstance()->CreateOrUpdateCountBoundedQueue(mQueueKey, 0, dummyCtx);
    }

    void TearDown() override {
        ProcessQueueManager::GetInstance()->Clear();
        QueueKeyManager::GetInstance()->Clear();
        if (mServer) {
            mServer->Shutdown(std::chrono::system_clock::now() + std::chrono::seconds(1));
            mServer->Wait();
            mServerThread.join();
        }
    }

    bool StartMockServer() {
        mOtlpService = std::make_unique<OTLPForwardServiceImpl>(mServerAddress);

        grpc::ServerBuilder builder;
        builder.AddListeningPort(mServerAddress, grpc::InsecureServerCredentials());
        for (auto* svc : mOtlpService->GetGrpcServices()) {
            builder.RegisterService(svc);
        }
        mServer = builder.BuildAndStart();
        if (!mServer) {
            return false;
        }

        mServerThread = std::thread([this]() { mServer->Wait(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return true;
    }

    bool CreateGrpcClient() {
        auto channel = grpc::CreateChannel(mServerAddress, grpc::InsecureChannelCredentials());
        mLogsStub = LogsService::NewStub(channel);
        mMetricsStub = MetricsService::NewStub(channel);
        mTraceStub = TraceService::NewStub(channel);
        return mLogsStub != nullptr;
    }

    void TestOTLPLogsForward();
    void TestOTLPLogsForwardWithConfig();
    void TestOTLPLogsForwardEmptyRequest();
    void TestOTLPMetricsForward();
    void TestOTLPMetricsForwardWithConfig();
    void TestOTLPTracesForward();
    void TestOTLPTracesForwardWithConfig();
    void TestOTLPTracesForwardEmptyRequest();
    void TestOTLPConfigUpdate();

protected:
    std::string mServerAddress;
    std::unique_ptr<grpc::Server> mServer;
    std::thread mServerThread;
    std::unique_ptr<OTLPForwardServiceImpl> mOtlpService;

    std::unique_ptr<LogsService::Stub> mLogsStub;
    std::unique_ptr<MetricsService::Stub> mMetricsStub;
    std::unique_ptr<TraceService::Stub> mTraceStub;
    QueueKey mQueueKey;
};

void OTLPForwardServiceUnittest::TestOTLPLogsForward() {
    ASSERT_TRUE(StartMockServer());
    ASSERT_TRUE(CreateGrpcClient());

    ExportLogsServiceRequest request;
    auto* resourceLogs = request.add_resource_logs();
    auto* scopeLogs = resourceLogs->add_scope_logs();

    // Add resource attributes
    auto* attr = resourceLogs->mutable_resource()->add_attributes();
    attr->set_key("service.name");
    attr->mutable_value()->set_string_value("unittest");

    // Add a log record
    auto* logRecord = scopeLogs->add_log_records();
    logRecord->set_time_unix_nano(1748313840259486017ULL);
    logRecord->mutable_body()->set_string_value("Test log message");
    logRecord->set_severity_text("INFO");

    grpc::ClientContext context;
    context.AddMetadata("x-otlp-apm-configname", "test-config");
    ExportLogsServiceResponse response;

    // No matching config -> should return NOT_FOUND, discarded counter incremented
    grpc::Status status = mLogsStub->Export(&context, request, &response);
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
    APSARA_TEST_EQUAL(1U, mOtlpService->mDiscardedEventsTotal->GetValue());
}

void OTLPForwardServiceUnittest::TestOTLPLogsForwardWithConfig() {
    ASSERT_TRUE(StartMockServer());
    ASSERT_TRUE(CreateGrpcClient());

    // Configure the service
    Json::Value config;
    config["QueueKey"] = static_cast<int>(mQueueKey);
    config["InputIndex"] = 0;
    ASSERT_TRUE(mOtlpService->Update("test-config", config));

    ExportLogsServiceRequest request;
    auto* resourceLogs = request.add_resource_logs();
    auto* scopeLogs = resourceLogs->add_scope_logs();

    auto* attr = resourceLogs->mutable_resource()->add_attributes();
    attr->set_key("service.name");
    attr->mutable_value()->set_string_value("unittest");

    auto* logRecord = scopeLogs->add_log_records();
    logRecord->set_time_unix_nano(1748313840259486017ULL);
    logRecord->mutable_body()->set_string_value("Test log message");
    logRecord->set_severity_text("INFO");
    auto* attr2 = logRecord->add_attributes();
    attr2->set_key("key1");
    attr2->mutable_value()->set_string_value("value1");

    grpc::ClientContext context;
    context.AddMetadata("x-otlp-apm-configname", "test-config");
    ExportLogsServiceResponse response;

    grpc::Status status = mLogsStub->Export(&context, request, &response);
    EXPECT_TRUE(status.ok());
    APSARA_TEST_EQUAL(1U, mOtlpService->mLogInEventsTotal->GetValue());
    APSARA_TEST_TRUE(mOtlpService->mLogInSizeBytes->GetValue() > 0);
}

void OTLPForwardServiceUnittest::TestOTLPLogsForwardEmptyRequest() {
    ASSERT_TRUE(StartMockServer());
    ASSERT_TRUE(CreateGrpcClient());

    Json::Value config;
    config["QueueKey"] = static_cast<int>(mQueueKey);
    config["InputIndex"] = 0;
    ASSERT_TRUE(mOtlpService->Update("test-config", config));

    // Empty request (no log records)
    ExportLogsServiceRequest request;
    auto* resourceLogs = request.add_resource_logs();
    resourceLogs->add_scope_logs();

    grpc::ClientContext context;
    context.AddMetadata("x-otlp-apm-configname", "test-config");
    ExportLogsServiceResponse response;

    grpc::Status status = mLogsStub->Export(&context, request, &response);
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    APSARA_TEST_EQUAL(1U, mOtlpService->mLogInEventsTotal->GetValue());
}

void OTLPForwardServiceUnittest::TestOTLPMetricsForwardWithConfig() {
    ASSERT_TRUE(StartMockServer());
    ASSERT_TRUE(CreateGrpcClient());

    Json::Value config;
    config["QueueKey"] = static_cast<int>(mQueueKey);
    config["InputIndex"] = 0;
    ASSERT_TRUE(mOtlpService->Update("test-config", config));

    // Gauge metric
    ExportMetricsServiceRequest request;
    auto* resourceMetrics = request.add_resource_metrics();
    auto* scopeMetrics = resourceMetrics->add_scope_metrics();
    auto* metric = scopeMetrics->add_metrics();
    metric->set_name("test.gauge.metric");
    auto* gauge = metric->mutable_gauge();
    auto* dp = gauge->add_data_points();
    dp->set_as_double(42.5);
    dp->set_time_unix_nano(1748313840259486017ULL);

    grpc::ClientContext context;
    context.AddMetadata("x-otlp-apm-configname", "test-config");
    ExportMetricsServiceResponse response;

    grpc::Status status = mMetricsStub->Export(&context, request, &response);
    EXPECT_TRUE(status.ok());
    APSARA_TEST_EQUAL(1U, mOtlpService->mMetricInEventsTotal->GetValue());
}

void OTLPForwardServiceUnittest::TestOTLPTracesForwardWithConfig() {
    ASSERT_TRUE(StartMockServer());
    ASSERT_TRUE(CreateGrpcClient());

    Json::Value config;
    config["QueueKey"] = static_cast<int>(mQueueKey);
    config["InputIndex"] = 0;
    ASSERT_TRUE(mOtlpService->Update("test-config", config));

    ExportTraceServiceRequest request;
    auto* resourceSpans = request.add_resource_spans();
    auto* scopeSpans = resourceSpans->add_scope_spans();

    auto* span = scopeSpans->add_spans();
    span->set_trace_id("unittest1234567890abcdef12345678");
    span->set_span_id("unittest12345678");
    span->set_parent_span_id("parent12345678901234");
    span->set_name("/unittest/api");
    span->set_kind(opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER);
    span->set_start_time_unix_nano(1748313840259486017ULL);
    span->set_end_time_unix_nano(1748313840259765375ULL);
    span->mutable_status()->set_code(opentelemetry::proto::trace::v1::Status::STATUS_CODE_OK);
    auto* attr = span->add_attributes();
    attr->set_key("http.method");
    attr->mutable_value()->set_string_value("GET");

    grpc::ClientContext context;
    context.AddMetadata("x-otlp-apm-configname", "test-config");
    ExportTraceServiceResponse response;

    grpc::Status status = mTraceStub->Export(&context, request, &response);
    EXPECT_TRUE(status.ok());
    APSARA_TEST_EQUAL(1U, mOtlpService->mTraceInEventsTotal->GetValue());
}

void OTLPForwardServiceUnittest::TestOTLPTracesForwardEmptyRequest() {
    ASSERT_TRUE(StartMockServer());
    ASSERT_TRUE(CreateGrpcClient());

    Json::Value config;
    config["QueueKey"] = static_cast<int>(mQueueKey);
    config["InputIndex"] = 0;
    ASSERT_TRUE(mOtlpService->Update("test-config", config));

    ExportTraceServiceRequest request;
    auto* resourceSpans = request.add_resource_spans();
    resourceSpans->add_scope_spans();

    grpc::ClientContext context;
    context.AddMetadata("x-otlp-apm-configname", "test-config");
    ExportTraceServiceResponse response;

    grpc::Status status = mTraceStub->Export(&context, request, &response);
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    APSARA_TEST_EQUAL(1U, mOtlpService->mTraceInEventsTotal->GetValue());
}

void OTLPForwardServiceUnittest::TestOTLPMetricsForward() {
    ASSERT_TRUE(StartMockServer());
    ASSERT_TRUE(CreateGrpcClient());

    ExportMetricsServiceRequest request;
    auto* resourceMetrics = request.add_resource_metrics();
    auto* scopeMetrics = resourceMetrics->add_scope_metrics();

    auto* metric = scopeMetrics->add_metrics();
    metric->set_name("test.metric");
    auto* gauge = metric->mutable_gauge();
    auto* dp = gauge->add_data_points();
    dp->set_as_double(42.0);

    grpc::ClientContext context;
    context.AddMetadata("x-otlp-apm-configname", "test-config");
    ExportMetricsServiceResponse response;

    // No matching config -> should return NOT_FOUND
    grpc::Status status = mMetricsStub->Export(&context, request, &response);
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
    APSARA_TEST_EQUAL(1U, mOtlpService->mDiscardedEventsTotal->GetValue());
}

void OTLPForwardServiceUnittest::TestOTLPTracesForward() {
    ASSERT_TRUE(StartMockServer());
    ASSERT_TRUE(CreateGrpcClient());

    ExportTraceServiceRequest request;
    auto* resourceSpans = request.add_resource_spans();
    auto* scopeSpans = resourceSpans->add_scope_spans();

    auto* span = scopeSpans->add_spans();
    span->set_trace_id("unittest1234567890abcdef12345678");
    span->set_span_id("unittest12345678");
    span->set_parent_span_id("parent12345678901234");
    span->set_name("/unittest/api");
    span->set_kind(opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER);
    span->set_start_time_unix_nano(1748313840259486017ULL);
    span->set_end_time_unix_nano(1748313840259765375ULL);
    span->mutable_status()->set_code(opentelemetry::proto::trace::v1::Status::STATUS_CODE_OK);

    grpc::ClientContext context;
    context.AddMetadata("x-otlp-apm-configname", "test-config");
    ExportTraceServiceResponse response;

    // No matching config -> should return NOT_FOUND, discarded counter incremented
    grpc::Status status = mTraceStub->Export(&context, request, &response);
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
    APSARA_TEST_EQUAL(1U, mOtlpService->mDiscardedEventsTotal->GetValue());
}

void OTLPForwardServiceUnittest::TestOTLPConfigUpdate() {
    auto service = std::make_unique<OTLPForwardServiceImpl>("0.0.0.0:15899");

    // Test valid config
    Json::Value validConfig;
    validConfig["QueueKey"] = 1;
    validConfig["InputIndex"] = 0;
    EXPECT_TRUE(service->Update("config1", validConfig));

    // Test missing QueueKey
    Json::Value invalidConfig1;
    invalidConfig1["InputIndex"] = 0;
    EXPECT_FALSE(service->Update("config2", invalidConfig1));

    // Test missing InputIndex
    Json::Value invalidConfig2;
    invalidConfig2["QueueKey"] = 1;
    EXPECT_FALSE(service->Update("config3", invalidConfig2));

    // Test remove
    Json::Value removeConfig;
    EXPECT_TRUE(service->Remove("config1", removeConfig));

    EXPECT_EQ(service->Name(), "OTLPForwardService");
}

UNIT_TEST_CASE(OTLPForwardServiceUnittest, TestOTLPLogsForward)
UNIT_TEST_CASE(OTLPForwardServiceUnittest, TestOTLPLogsForwardWithConfig)
UNIT_TEST_CASE(OTLPForwardServiceUnittest, TestOTLPLogsForwardEmptyRequest)
UNIT_TEST_CASE(OTLPForwardServiceUnittest, TestOTLPMetricsForward)
UNIT_TEST_CASE(OTLPForwardServiceUnittest, TestOTLPMetricsForwardWithConfig)
UNIT_TEST_CASE(OTLPForwardServiceUnittest, TestOTLPTracesForward)
UNIT_TEST_CASE(OTLPForwardServiceUnittest, TestOTLPTracesForwardWithConfig)
UNIT_TEST_CASE(OTLPForwardServiceUnittest, TestOTLPTracesForwardEmptyRequest)
UNIT_TEST_CASE(OTLPForwardServiceUnittest, TestOTLPConfigUpdate)

} // namespace logtail

UNIT_TEST_MAIN

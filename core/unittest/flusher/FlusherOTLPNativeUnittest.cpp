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

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/memory/SourceBuffer.h"
#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/MetricValue.h"
#include "models/PipelineEventGroup.h"
#include "models/RawEvent.h"
#include "models/SpanEvent.h"
#include "plugin/flusher/opentelemetry/FlusherOTLPNative.h"
#include "protobuf/opentelemetry/proto/collector/logs/v1/logs_service.grpc.pb.h"
#include "protobuf/opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "protobuf/opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.h"
#include "unittest/Unittest.h"

using namespace std;
using namespace opentelemetry::proto::collector::logs::v1;
using namespace opentelemetry::proto::collector::metrics::v1;
using namespace opentelemetry::proto::collector::trace::v1;

namespace logtail {

// ==================== Mock gRPC Collector Service ====================

class MockOTLPCollectorService : public LogsService::CallbackService,
                                 public MetricsService::CallbackService,
                                 public TraceService::CallbackService {
public:
    grpc::ServerUnaryReactor* Export(grpc::CallbackServerContext* context,
                                     const ExportLogsServiceRequest* request,
                                     ExportLogsServiceResponse* response) override {
        auto* reactor = context->DefaultReactor();
        if (request && !request->resource_logs().empty()) {
            lock_guard<mutex> lock(mMutex);
            mLastLogRequest = make_unique<ExportLogsServiceRequest>(*request);
            mLogCallCount++;
        }
        reactor->Finish(mLogStatus);
        return reactor;
    }

    grpc::ServerUnaryReactor* Export(grpc::CallbackServerContext* context,
                                     const ExportMetricsServiceRequest* request,
                                     ExportMetricsServiceResponse* response) override {
        auto* reactor = context->DefaultReactor();
        if (request && !request->resource_metrics().empty()) {
            lock_guard<mutex> lock(mMutex);
            mLastMetricRequest = make_unique<ExportMetricsServiceRequest>(*request);
            mMetricCallCount++;
        }
        reactor->Finish(mMetricStatus);
        return reactor;
    }

    grpc::ServerUnaryReactor* Export(grpc::CallbackServerContext* context,
                                     const ExportTraceServiceRequest* request,
                                     ExportTraceServiceResponse* response) override {
        auto* reactor = context->DefaultReactor();
        if (request && !request->resource_spans().empty()) {
            lock_guard<mutex> lock(mMutex);
            mLastTraceRequest = make_unique<ExportTraceServiceRequest>(*request);
            mTraceCallCount++;
        }
        reactor->Finish(mTraceStatus);
        return reactor;
    }

    void SetLogStatus(const grpc::Status& s) { mLogStatus = s; }
    void SetMetricStatus(const grpc::Status& s) { mMetricStatus = s; }
    void SetTraceStatus(const grpc::Status& s) { mTraceStatus = s; }
    void Reset() {
        lock_guard<mutex> lock(mMutex);
        mLastLogRequest.reset();
        mLastMetricRequest.reset();
        mLastTraceRequest.reset();
        mLogCallCount = 0;
        mMetricCallCount = 0;
        mTraceCallCount = 0;
        mLogStatus = grpc::Status::OK;
        mMetricStatus = grpc::Status::OK;
        mTraceStatus = grpc::Status::OK;
    }

    int GetLogCallCount() const { return mLogCallCount; }
    int GetMetricCallCount() const { return mMetricCallCount; }
    int GetTraceCallCount() const { return mTraceCallCount; }
    const ExportLogsServiceRequest* GetLastLogRequest() const { return mLastLogRequest.get(); }
    const ExportMetricsServiceRequest* GetLastMetricRequest() const { return mLastMetricRequest.get(); }
    const ExportTraceServiceRequest* GetLastTraceRequest() const { return mLastTraceRequest.get(); }

private:
    mutable mutex mMutex;
    unique_ptr<ExportLogsServiceRequest> mLastLogRequest;
    unique_ptr<ExportMetricsServiceRequest> mLastMetricRequest;
    unique_ptr<ExportTraceServiceRequest> mLastTraceRequest;
    int mLogCallCount = 0;
    int mMetricCallCount = 0;
    int mTraceCallCount = 0;
    grpc::Status mLogStatus = grpc::Status::OK;
    grpc::Status mMetricStatus = grpc::Status::OK;
    grpc::Status mTraceStatus = grpc::Status::OK;
};

// ==================== Test Fixture ====================

class FlusherOTLPNativeUnittest : public testing::Test {
public:
    void SetUp() override {
        mServerAddress = "0.0.0.0:15900";
        mMockService = make_unique<MockOTLPCollectorService>();
        StartServer();
        ctx.SetConfigName("test_config");
        ctx.SetPipeline(pipeline);
    }

    void TearDown() override {
        if (mServer) {
            mServer->Shutdown(chrono::system_clock::now() + chrono::seconds(1));
            mServer->Wait();
            mServerThread.join();
        }
    }

    void TestInit_ValidConfig();
    void TestInit_MissingEndpoint();
    void TestInit_CustomHeaders();
    void TestSerializeLogs();
    void TestSerializeMetrics();
    void TestSerializeTraces();
    void TestSerializeRawEvents();
    void TestBuildGrpcRequest_Logs();
    void TestBuildGrpcRequest_Metrics();
    void TestBuildGrpcRequest_Traces();
    void TestHandleGrpcCallback_Success();
    void TestHandleGrpcCallback_Failure();
    void TestSend_ReturnsTrueForEmpty();
    void TestFlusherName();
    void TestSinkType();

protected:
    string mServerAddress;
    unique_ptr<grpc::Server> mServer;
    thread mServerThread;
    unique_ptr<MockOTLPCollectorService> mMockService;

    CollectionPipeline pipeline;
    CollectionPipelineContext ctx;

    void StartServer() {
        grpc::ServerBuilder builder;
        builder.AddListeningPort(mServerAddress, grpc::InsecureServerCredentials());
        builder.RegisterService(
            static_cast<grpc::Service*>(static_cast<LogsService::CallbackService*>(mMockService.get())));
        builder.RegisterService(
            static_cast<grpc::Service*>(static_cast<MetricsService::CallbackService*>(mMockService.get())));
        builder.RegisterService(
            static_cast<grpc::Service*>(static_cast<TraceService::CallbackService*>(mMockService.get())));
        mServer = builder.BuildAndStart();
        ASSERT_NE(mServer, nullptr);
        mServerThread = thread([this]() { mServer->Wait(); });
        this_thread::sleep_for(chrono::milliseconds(500));
    }

    unique_ptr<FlusherOTLPNative> CreateAndInitFlusher() {
        auto flusher = make_unique<FlusherOTLPNative>();
        flusher->SetContext(ctx);
        flusher->CreateMetricsRecordRef(FlusherOTLPNative::sName, "1");

        Json::Value config;
        config["Endpoint"] = mServerAddress;
        config["TimeoutMs"] = 5000;
        config["EnableTLS"] = false;
        Json::Value::Members headers;
        config["Headers"]["X-Custom-Header"] = "test-value";
        Json::Value optionalGoPipeline;

        if (!flusher->Init(config, optionalGoPipeline)) {
            return nullptr;
        }
        flusher->CommitMetricsRecordRef();

        if (!flusher->Start()) {
            return nullptr;
        }
        return flusher;
    }

    PipelineEventGroup MakeLogGroup() {
        auto group = PipelineEventGroup(make_shared<SourceBuffer>());
        auto* logEvent = group.AddLogEvent(true);
        logEvent->SetTimestamp(1748313840, 259486017);
        logEvent->SetContent(string("content"), string("Test log message"));
        logEvent->SetContent(string("key1"), string("value1"));
        group.SetTag(string("service.name"), string("unittest"));
        return group;
    }

    PipelineEventGroup MakeMetricGroup() {
        auto group = PipelineEventGroup(make_shared<SourceBuffer>());
        auto* metricEvent = group.AddMetricEvent(true);
        metricEvent->SetName("test.metric.value");
        metricEvent->SetValue<UntypedSingleValue>(42.5);
        metricEvent->SetTag(string("host"), string("test-host"));
        group.SetTag(string("service.name"), string("unittest"));
        return group;
    }

    PipelineEventGroup MakeSpanGroup() {
        auto group = PipelineEventGroup(make_shared<SourceBuffer>());
        auto* spanEvent = group.AddSpanEvent(true);
        spanEvent->SetTraceId("unittest1234567890abcdef12345678");
        spanEvent->SetSpanId("unittest12345678");
        spanEvent->SetParentSpanId("parent12345678901234");
        spanEvent->SetName("/test/api");
        spanEvent->SetKind(SpanEvent::Kind::Server);
        spanEvent->SetStatus(SpanEvent::StatusCode::Ok);
        spanEvent->SetStartTimeNs(1748313840259486017ULL);
        spanEvent->SetEndTimeNs(1748313840259765375ULL);
        spanEvent->SetTag(string("service.name"), string("unittest"));
        return group;
    }

    PipelineEventGroup MakeRawEventGroup() {
        auto group = PipelineEventGroup(make_shared<SourceBuffer>());
        auto* rawEvent = group.AddRawEvent(true);
        rawEvent->SetTimestamp(1748313840, 259486017);
        rawEvent->SetContent(string("raw log content"));
        group.SetTag(string("service.name"), string("unittest"));
        return group;
    }
};

// ==================== Init Tests ====================

void FlusherOTLPNativeUnittest::TestInit_ValidConfig() {
    auto flusher = CreateAndInitFlusher();
    APSARA_TEST_TRUE(flusher != nullptr);
    APSARA_TEST_STREQ(FlusherOTLPNative::sName.c_str(), "flusher_otlp_native");
}

void FlusherOTLPNativeUnittest::TestInit_MissingEndpoint() {
    auto flusher = make_unique<FlusherOTLPNative>();
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherOTLPNative::sName, "1");

    Json::Value config;
    config["TimeoutMs"] = 5000;
    Json::Value optionalGoPipeline;

    APSARA_TEST_FALSE(flusher->Init(config, optionalGoPipeline));
}

void FlusherOTLPNativeUnittest::TestInit_CustomHeaders() {
    auto flusher = make_unique<FlusherOTLPNative>();
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherOTLPNative::sName, "1");

    Json::Value config;
    config["Endpoint"] = mServerAddress;
    config["Headers"]["Authorization"] = "Bearer token123";
    config["Headers"]["X-Request-Id"] = "req-001";
    Json::Value optionalGoPipeline;

    APSARA_TEST_TRUE(flusher->Init(config, optionalGoPipeline));
    APSARA_TEST_EQUAL(flusher->GetHeaders().size(), 2);
    APSARA_TEST_STREQ(flusher->GetHeaders().at("Authorization").c_str(), "Bearer token123");
    APSARA_TEST_TRUE(flusher->GetTimeoutMs() == 30000); // default
}

// ==================== Serialization Tests ====================

void FlusherOTLPNativeUnittest::TestSerializeLogs() {
    auto flusher = make_unique<FlusherOTLPNative>();
    flusher->SetContext(ctx);

    auto group = MakeLogGroup();
    string data;
    string errMsg;
    APSARA_TEST_TRUE(flusher->SerializeLogsToOTLP(group, data, errMsg));
    APSARA_TEST_TRUE(data.size() > 0);

    // Verify by deserializing
    ExportLogsServiceRequest req;
    APSARA_TEST_TRUE(req.ParseFromString(data));
    APSARA_TEST_EQUAL(req.resource_logs_size(), 1);

    const auto& scopeLogs = req.resource_logs(0).scope_logs(0);
    APSARA_TEST_EQUAL(scopeLogs.log_records_size(), 1);
    const auto& record = scopeLogs.log_records(0);
    APSARA_TEST_STREQ(record.body().string_value().c_str(), "Test log message");
    APSARA_TEST_TRUE(record.time_unix_nano() > 0);
    // Check attributes (key1, message)
    bool foundKey1 = false;
    for (int i = 0; i < record.attributes_size(); i++) {
        if (record.attributes(i).key() == "key1") {
            foundKey1 = true;
            APSARA_TEST_STREQ(record.attributes(i).value().string_value().c_str(), "value1");
        }
    }
    APSARA_TEST_TRUE(foundKey1);
}

void FlusherOTLPNativeUnittest::TestSerializeMetrics() {
    auto flusher = make_unique<FlusherOTLPNative>();
    flusher->SetContext(ctx);

    auto group = MakeMetricGroup();
    string data;
    string errMsg;
    APSARA_TEST_TRUE(flusher->SerializeMetricsToOTLP(group, data, errMsg));
    APSARA_TEST_TRUE(data.size() > 0);

    ExportMetricsServiceRequest req;
    APSARA_TEST_TRUE(req.ParseFromString(data));
    APSARA_TEST_EQUAL(req.resource_metrics_size(), 1);

    const auto& metric = req.resource_metrics(0).scope_metrics(0).metrics(0);
    APSARA_TEST_STREQ(metric.name().c_str(), "test.metric.value");
    APSARA_TEST_TRUE(metric.has_gauge());
    APSARA_TEST_EQUAL(metric.gauge().data_points_size(), 1);
    APSARA_TEST_EQUAL(metric.gauge().data_points(0).as_double(), 42.5);
}

void FlusherOTLPNativeUnittest::TestSerializeTraces() {
    auto flusher = make_unique<FlusherOTLPNative>();
    flusher->SetContext(ctx);

    auto group = MakeSpanGroup();
    string data;
    string errMsg;
    APSARA_TEST_TRUE(flusher->SerializeTracesToOTLP(group, data, errMsg));
    APSARA_TEST_TRUE(data.size() > 0);

    ExportTraceServiceRequest req;
    APSARA_TEST_TRUE(req.ParseFromString(data));
    APSARA_TEST_EQUAL(req.resource_spans_size(), 1);

    const auto& span = req.resource_spans(0).scope_spans(0).spans(0);
    APSARA_TEST_STREQ(span.name().c_str(), "/test/api");
    APSARA_TEST_EQUAL(span.kind(), opentelemetry::proto::trace::v1::Span_SpanKind_SPAN_KIND_SERVER);
    APSARA_TEST_EQUAL(span.status().code(), opentelemetry::proto::trace::v1::Status_StatusCode_STATUS_CODE_OK);
    APSARA_TEST_TRUE(span.start_time_unix_nano() > 0);
    APSARA_TEST_TRUE(span.end_time_unix_nano() > 0);
}

void FlusherOTLPNativeUnittest::TestSerializeRawEvents() {
    auto flusher = make_unique<FlusherOTLPNative>();
    flusher->SetContext(ctx);

    auto group = MakeRawEventGroup();
    string data;
    string errMsg;
    APSARA_TEST_TRUE(flusher->SerializeLogsToOTLP(group, data, errMsg));
    APSARA_TEST_TRUE(data.size() > 0);

    ExportLogsServiceRequest req;
    APSARA_TEST_TRUE(req.ParseFromString(data));
    APSARA_TEST_EQUAL(req.resource_logs_size(), 1);

    const auto& record = req.resource_logs(0).scope_logs(0).log_records(0);
    APSARA_TEST_STREQ(record.body().string_value().c_str(), "raw log content");
    APSARA_TEST_TRUE(record.time_unix_nano() > 0);
}

// ==================== BuildGrpcRequest Tests ====================

void FlusherOTLPNativeUnittest::TestBuildGrpcRequest_Logs() {
    auto flusher = make_unique<FlusherOTLPNative>();
    flusher->SetContext(ctx);

    auto group = MakeLogGroup();
    string data;
    string errMsg;
    APSARA_TEST_TRUE(flusher->SerializeLogsToOTLP(group, data, errMsg));

    auto item = make_unique<OTLPSenderQueueItem>(
        move(data), data.size(), flusher.get(), 1, OTLPGrpcCallContext::DataType::Logs);

    unique_ptr<OTLPGrpcCallContext> ctx2;
    bool keepItem = false;
    string buildErr;
    APSARA_TEST_TRUE(flusher->BuildGrpcRequest(item.get(), ctx2, &keepItem, &buildErr));
    APSARA_TEST_TRUE(ctx2 != nullptr);
    APSARA_TEST_TRUE(ctx2->logsReq != nullptr);
    APSARA_TEST_EQUAL(ctx2->logsReq->resource_logs_size(), 1);
    APSARA_TEST_FALSE(keepItem);
}

void FlusherOTLPNativeUnittest::TestBuildGrpcRequest_Metrics() {
    auto flusher = make_unique<FlusherOTLPNative>();
    flusher->SetContext(ctx);

    auto group = MakeMetricGroup();
    string data;
    string errMsg;
    APSARA_TEST_TRUE(flusher->SerializeMetricsToOTLP(group, data, errMsg));

    auto item = make_unique<OTLPSenderQueueItem>(
        move(data), data.size(), flusher.get(), 1, OTLPGrpcCallContext::DataType::Metrics);

    unique_ptr<OTLPGrpcCallContext> ctx2;
    bool keepItem = false;
    string buildErr;
    APSARA_TEST_TRUE(flusher->BuildGrpcRequest(item.get(), ctx2, &keepItem, &buildErr));
    APSARA_TEST_TRUE(ctx2 != nullptr);
    APSARA_TEST_TRUE(ctx2->metricsReq != nullptr);
    APSARA_TEST_EQUAL(ctx2->metricsReq->resource_metrics_size(), 1);
}

void FlusherOTLPNativeUnittest::TestBuildGrpcRequest_Traces() {
    auto flusher = make_unique<FlusherOTLPNative>();
    flusher->SetContext(ctx);

    auto group = MakeSpanGroup();
    string data;
    string errMsg;
    APSARA_TEST_TRUE(flusher->SerializeTracesToOTLP(group, data, errMsg));

    auto item = make_unique<OTLPSenderQueueItem>(
        move(data), data.size(), flusher.get(), 1, OTLPGrpcCallContext::DataType::Traces);

    unique_ptr<OTLPGrpcCallContext> ctx2;
    bool keepItem = false;
    string buildErr;
    APSARA_TEST_TRUE(flusher->BuildGrpcRequest(item.get(), ctx2, &keepItem, &buildErr));
    APSARA_TEST_TRUE(ctx2 != nullptr);
    APSARA_TEST_TRUE(ctx2->traceReq != nullptr);
    APSARA_TEST_EQUAL(ctx2->traceReq->resource_spans_size(), 1);
}

// ==================== HandleGrpcCallback Tests ====================

void FlusherOTLPNativeUnittest::TestHandleGrpcCallback_Success() {
    auto flusher = make_unique<FlusherOTLPNative>();
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherOTLPNative::sName, "1");
    flusher->mSendSuccessCnt = flusher->GetMetricsRecordRef().CreateCounter("mSendSuccessCnt");
    flusher->CommitMetricsRecordRef();

    string data = "test-data";
    auto item = make_unique<OTLPSenderQueueItem>(
        move(data), data.size(), flusher.get(), 1, OTLPGrpcCallContext::DataType::Logs);
    SenderQueueItem* rawItem = item.get();

    auto* ctx2 = new OTLPGrpcCallContext();
    ctx2->item = item.release();
    ctx2->type = OTLPGrpcCallContext::DataType::Logs;
    ctx2->logsReq = make_unique<ExportLogsServiceRequest>();
    ctx2->logsResp = make_unique<ExportLogsServiceResponse>();

    flusher->IncInFlight();
    APSARA_TEST_EQUAL(flusher->InFlightCount(), 1);

    flusher->HandleGrpcCallback(grpc::Status(grpc::StatusCode::OK, ""), ctx2);

    this_thread::sleep_for(chrono::milliseconds(50));
    APSARA_TEST_EQUAL(flusher->InFlightCount(), 0);
    APSARA_TEST_EQUAL(1U, flusher->mSendSuccessCnt->GetValue());
    // Clean up: DealSenderQueueItemAfterSend doesn't delete the item, relies on queue manager
    delete rawItem;
}

void FlusherOTLPNativeUnittest::TestHandleGrpcCallback_Failure() {
    auto flusher = make_unique<FlusherOTLPNative>();
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherOTLPNative::sName, "1");
    flusher->mSendFailCnt = flusher->GetMetricsRecordRef().CreateCounter("mSendFailCnt");
    flusher->CommitMetricsRecordRef();

    string data = "test-data";
    auto item = make_unique<OTLPSenderQueueItem>(
        move(data), data.size(), flusher.get(), 1, OTLPGrpcCallContext::DataType::Logs);
    SenderQueueItem* rawItem = item.get();

    auto* ctx2 = new OTLPGrpcCallContext();
    ctx2->item = item.release();
    ctx2->type = OTLPGrpcCallContext::DataType::Logs;
    ctx2->logsReq = make_unique<ExportLogsServiceRequest>();
    ctx2->logsResp = make_unique<ExportLogsServiceResponse>();

    flusher->IncInFlight();
    auto status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "Server unavailable");
    flusher->HandleGrpcCallback(move(status), ctx2);

    this_thread::sleep_for(chrono::milliseconds(50));
    APSARA_TEST_EQUAL(flusher->InFlightCount(), 0);
    APSARA_TEST_EQUAL(1U, flusher->mSendFailCnt->GetValue());
    delete rawItem;
}

// ==================== Misc Tests ====================

void FlusherOTLPNativeUnittest::TestSend_ReturnsTrueForEmpty() {
    auto flusher = make_unique<FlusherOTLPNative>();
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherOTLPNative::sName, "1");
    Json::Value config;
    config["Endpoint"] = "localhost:4317";
    Json::Value optionalGoPipeline;
    APSARA_TEST_TRUE(flusher->Init(config, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();

    // Empty group → returns true, counter NOT incremented
    PipelineEventGroup emptyGroup(make_shared<SourceBuffer>());
    APSARA_TEST_TRUE(flusher->Send(move(emptyGroup)));
    APSARA_TEST_EQUAL(0U, flusher->mSendCnt->GetValue());

    // Non-empty group → returns true, counter incremented
    auto group = MakeLogGroup();
    APSARA_TEST_TRUE(flusher->Send(move(group)));
    APSARA_TEST_EQUAL(1U, flusher->mSendCnt->GetValue());
}

void FlusherOTLPNativeUnittest::TestFlusherName() {
    APSARA_TEST_STREQ(FlusherOTLPNative::sName.c_str(), "flusher_otlp_native");
}

void FlusherOTLPNativeUnittest::TestSinkType() {
    auto flusher = make_unique<FlusherOTLPNative>();
    APSARA_TEST_TRUE(flusher->GetSinkType() == SinkType::GRPC);
}

// ==================== Test Registration ====================

UNIT_TEST_CASE(FlusherOTLPNativeUnittest, TestInit_ValidConfig)
UNIT_TEST_CASE(FlusherOTLPNativeUnittest, TestInit_MissingEndpoint)
UNIT_TEST_CASE(FlusherOTLPNativeUnittest, TestInit_CustomHeaders)
UNIT_TEST_CASE(FlusherOTLPNativeUnittest, TestSerializeLogs)
UNIT_TEST_CASE(FlusherOTLPNativeUnittest, TestSerializeMetrics)
UNIT_TEST_CASE(FlusherOTLPNativeUnittest, TestSerializeTraces)
UNIT_TEST_CASE(FlusherOTLPNativeUnittest, TestSerializeRawEvents)
UNIT_TEST_CASE(FlusherOTLPNativeUnittest, TestBuildGrpcRequest_Logs)
UNIT_TEST_CASE(FlusherOTLPNativeUnittest, TestBuildGrpcRequest_Metrics)
UNIT_TEST_CASE(FlusherOTLPNativeUnittest, TestBuildGrpcRequest_Traces)
UNIT_TEST_CASE(FlusherOTLPNativeUnittest, TestHandleGrpcCallback_Success)
UNIT_TEST_CASE(FlusherOTLPNativeUnittest, TestHandleGrpcCallback_Failure)
UNIT_TEST_CASE(FlusherOTLPNativeUnittest, TestSend_ReturnsTrueForEmpty)
UNIT_TEST_CASE(FlusherOTLPNativeUnittest, TestFlusherName)
UNIT_TEST_CASE(FlusherOTLPNativeUnittest, TestSinkType)

} // namespace logtail

UNIT_TEST_MAIN

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

// Integration test: PushToGrpcSink -> GrpcSink path via mock.

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/MetricValue.h"
#include "models/PipelineEventGroup.h"
#include "models/SpanEvent.h"
#include "common/memory/SourceBuffer.h"
#include "plugin/flusher/opentelemetry/FlusherOTLPNative.h"
#include "runner/FlusherRunner.h"
#include "unittest/pipeline/GrpcSinkMock.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class PushToGrpcSinkUnittest : public testing::Test {
public:
    void SetUp() override {
        mServerAddress = "0.0.0.0:15902";
        ctx.SetConfigName("test_config");
        ctx.SetPipeline(pipeline);
        // Initialize the GrpcSinkMock
        GrpcSinkMock::GetInstance()->Init();
    }

    void TearDown() override {
        GrpcSinkMock::GetInstance()->Stop();
    }

    void TestPushToGrpcSink_Success();
    void TestPushToGrpcSink_BuildRequestFailure();
    void TestSinkTypeIsGrpc();

protected:
    string mServerAddress;
    CollectionPipeline pipeline;
    CollectionPipelineContext ctx;

    unique_ptr<FlusherOTLPNative> CreateAndInitFlusher() {
        auto flusher = make_unique<FlusherOTLPNative>();
        flusher->SetContext(ctx);
        flusher->CreateMetricsRecordRef(FlusherOTLPNative::sName, "1");

        Json::Value config;
        config["Endpoint"] = mServerAddress;
        config["TimeoutMs"] = 5000;
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
        logEvent->SetContent(string("message"), string("Integration test log"));
        group.SetTag(string("service.name"), string("integration-test"));
        return group;
    }
};

void PushToGrpcSinkUnittest::TestPushToGrpcSink_Success() {
    auto flusher = CreateAndInitFlusher();
    APSARA_TEST_TRUE(flusher != nullptr);

    auto group = MakeLogGroup();
    string data;
    string errMsg;
    APSARA_TEST_TRUE(flusher->SerializeLogsToOTLP(group, data, errMsg));

    // Create item that PushToGrpcSink will consume
    auto item = make_unique<OTLPSenderQueueItem>(
        std::move(data), data.size(), flusher.get(), 1, OTLPGrpcCallContext::DataType::Logs);
    SenderQueueItem* rawItem = item.get();

    // Call PushToGrpcSink directly
    bool result = FlusherRunner::GetInstance()->PushToGrpcSink(rawItem, false);
    APSARA_TEST_TRUE(result);

    // Wait for mock to process
    this_thread::sleep_for(chrono::milliseconds(500));

    // Verify mock captured the request
    auto requests = GrpcSinkMock::GetInstance()->GetRequests();
    APSARA_TEST_TRUE(requests.size() >= 1);

    // Verify in-flight returned to 0
    APSARA_TEST_EQUAL(flusher->InFlightCount(), 0);
}

void PushToGrpcSinkUnittest::TestPushToGrpcSink_BuildRequestFailure() {
    auto flusher = CreateAndInitFlusher();
    APSARA_TEST_TRUE(flusher != nullptr);

    // Create an item with invalid data
    string badData = "not-valid-protobuf";
    auto item = make_unique<OTLPSenderQueueItem>(
        std::move(badData), badData.size(), flusher.get(), 1, OTLPGrpcCallContext::DataType::Logs);
    SenderQueueItem* rawItem = item.get();

    // BuildGrpcRequest should fail on invalid protobuf
    bool result = FlusherRunner::GetInstance()->PushToGrpcSink(rawItem, false);
    APSARA_TEST_FALSE(result);
}

void PushToGrpcSinkUnittest::TestSinkTypeIsGrpc() {
    auto flusher = CreateAndInitFlusher();
    APSARA_TEST_TRUE(flusher != nullptr);
    APSARA_TEST_TRUE(flusher->GetSinkType() == SinkType::GRPC);
}

UNIT_TEST_CASE(PushToGrpcSinkUnittest, TestPushToGrpcSink_Success)
UNIT_TEST_CASE(PushToGrpcSinkUnittest, TestPushToGrpcSink_BuildRequestFailure)
UNIT_TEST_CASE(PushToGrpcSinkUnittest, TestSinkTypeIsGrpc)

} // namespace logtail

UNIT_TEST_MAIN

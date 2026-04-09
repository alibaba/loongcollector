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

#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/channel.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "collection_pipeline/plugin/interface/Flusher.h"
#include "protobuf/opentelemetry/proto/collector/logs/v1/logs_service.grpc.pb.h"
#include "protobuf/opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "protobuf/opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.h"
#include "runner/sink/SinkType.h"

namespace logtail {

using OTLPLogsService = opentelemetry::proto::collector::logs::v1::LogsService;
using OTLPMetricsService = opentelemetry::proto::collector::metrics::v1::MetricsService;
using OTLPTraceService = opentelemetry::proto::collector::trace::v1::TraceService;

// Context for a single async gRPC Export call.
// Allocated in BuildGrpcRequest, deleted in HandleGrpcCallback.
struct OTLPGrpcCallContext {
    SenderQueueItem* item = nullptr;
    std::unique_ptr<grpc::ClientContext> context;

    enum class DataType : uint8_t { Logs, Metrics, Traces };
    DataType type = DataType::Logs;

    // Only the fields matching `type` are populated.
    std::unique_ptr<opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest> logsReq;
    std::unique_ptr<opentelemetry::proto::collector::logs::v1::ExportLogsServiceResponse> logsResp;
    std::unique_ptr<opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest> metricsReq;
    std::unique_ptr<opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceResponse> metricsResp;
    std::unique_ptr<opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest> traceReq;
    std::unique_ptr<opentelemetry::proto::collector::trace::v1::ExportTraceServiceResponse> traceResp;
};

// Custom SenderQueueItem that carries OTLP data type info.
struct OTLPSenderQueueItem : public SenderQueueItem {
    OTLPGrpcCallContext::DataType dataType;

    OTLPSenderQueueItem(std::string&& data,
                        size_t rawSize,
                        Flusher* flusher,
                        QueueKey key,
                        OTLPGrpcCallContext::DataType type)
        : SenderQueueItem(std::move(data), rawSize, flusher, key, RawDataType::EVENT_GROUP),
          dataType(type) {}

    OTLPSenderQueueItem(const OTLPSenderQueueItem& other)
        : SenderQueueItem(other), dataType(other.dataType) {}

    ~OTLPSenderQueueItem() override = default;

    OTLPSenderQueueItem* Clone() override { return new OTLPSenderQueueItem(*this); }
};

class FlusherOTLPNative : public Flusher {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override;
    bool Start() override;
    bool Stop(bool isPipelineRemoving) override;
    bool Send(PipelineEventGroup&& g) override;
    bool Flush(size_t /*key*/) override { return true; }
    bool FlushAll() override { return true; }

    SinkType GetSinkType() override { return SinkType::GRPC; }

    // Build gRPC call context from SenderQueueItem binary data.
    bool BuildGrpcRequest(SenderQueueItem* item,
                          std::unique_ptr<OTLPGrpcCallContext>& ctx,
                          bool* keepItem,
                          std::string* errMsg);

    // Called after async Export completes (handles retry/release).
    void OnSendDone(const grpc::Status& status, SenderQueueItem* item);

    // Public getters for GrpcSink to access stubs.
    OTLPLogsService::Stub* GetLogsStub() const { return mLogsStub.get(); }
    OTLPMetricsService::Stub* GetMetricsStub() const { return mMetricsStub.get(); }
    OTLPTraceService::Stub* GetTraceStub() const { return mTraceStub.get(); }
    int32_t GetTimeoutMs() const { return mTimeoutMs; }
    const std::unordered_map<std::string, std::string>& GetHeaders() const { return mHeaders; }

    // Called by GrpcSink callback to handle the async Export result.
    void HandleGrpcCallback(grpc::Status&& status, OTLPGrpcCallContext* ctx);

    // In-flight tracking for Stop()
    void IncInFlight() { mInFlightCnt.fetch_add(1); }
    void DecInFlight() {
        mInFlightCnt.fetch_sub(1);
        mStopCV.notify_all();
    }
    int32_t InFlightCount() const { return mInFlightCnt.load(); }

private:
    std::string mEndpoint;
    int32_t mTimeoutMs = 30000;
    std::unordered_map<std::string, std::string> mHeaders;
    bool mEnableTLS = false;

    std::shared_ptr<grpc::Channel> mChannel;
    std::unique_ptr<OTLPLogsService::Stub> mLogsStub;
    std::unique_ptr<OTLPMetricsService::Stub> mMetricsStub;
    std::unique_ptr<OTLPTraceService::Stub> mTraceStub;

    std::atomic<int32_t> mInFlightCnt{0};
    std::mutex mStopMutex;
    std::condition_variable mStopCV;
    std::atomic<bool> mIsStopping{false};

    CounterPtr mSendCnt;
    CounterPtr mSendSuccessCnt;
    CounterPtr mSendFailCnt;
    CounterPtr mDiscardedEventsTotal;

    bool CreateGrpcChannel();

    // Serialize PipelineEventGroup to OTLP protobuf binary string (by type).
    bool SerializeLogsToOTLP(const PipelineEventGroup& group, std::string& serializedData, std::string& errMsg);
    bool SerializeMetricsToOTLP(const PipelineEventGroup& group, std::string& serializedData, std::string& errMsg);
    bool SerializeTracesToOTLP(const PipelineEventGroup& group, std::string& serializedData, std::string& errMsg);

#ifdef APSARA_UNIT_TEST_MAIN
    friend class FlusherOTLPNativeUnittest;
    friend class GrpcSinkMock;
#endif
};

} // namespace logtail

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

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "collection_pipeline/queue/QueueKey.h"
#include "forward/BaseService.h"
#include "monitor/MetricManager.h"
#include "protobuf/opentelemetry/proto/collector/logs/v1/logs_service.grpc.pb.h"
#include "protobuf/opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "protobuf/opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.h"

namespace logtail {

struct OTLPForwardConfig {
    std::string configName;
    QueueKey queueKey;
    size_t inputIndex;
};

class OTLPForwardServiceImpl;

// gRPC service wrappers for Logs, Metrics, and Traces.
// Each inherits from exactly one CallbackService to avoid vtable method index conflicts
// (all three CallbackService types use method index 0).
class OTLPLogsGrpcService : public opentelemetry::proto::collector::logs::v1::LogsService::CallbackService {
public:
    explicit OTLPLogsGrpcService(OTLPForwardServiceImpl* impl) : mImpl(impl) {}
    grpc::ServerUnaryReactor*
    Export(grpc::CallbackServerContext* context,
           const opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest* request,
           opentelemetry::proto::collector::logs::v1::ExportLogsServiceResponse* response) override;

private:
    OTLPForwardServiceImpl* mImpl;
};

class OTLPMetricsGrpcService : public opentelemetry::proto::collector::metrics::v1::MetricsService::CallbackService {
public:
    explicit OTLPMetricsGrpcService(OTLPForwardServiceImpl* impl) : mImpl(impl) {}
    grpc::ServerUnaryReactor*
    Export(grpc::CallbackServerContext* context,
           const opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest* request,
           opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceResponse* response) override;

private:
    OTLPForwardServiceImpl* mImpl;
};

class OTLPTraceGrpcService : public opentelemetry::proto::collector::trace::v1::TraceService::CallbackService {
public:
    explicit OTLPTraceGrpcService(OTLPForwardServiceImpl* impl) : mImpl(impl) {}
    grpc::ServerUnaryReactor*
    Export(grpc::CallbackServerContext* context,
           const opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest* request,
           opentelemetry::proto::collector::trace::v1::ExportTraceServiceResponse* response) override;

private:
    OTLPForwardServiceImpl* mImpl;
};

class OTLPForwardServiceImpl : public BaseService {
public:
    OTLPForwardServiceImpl(const std::string& address);
    ~OTLPForwardServiceImpl() override = default;

    bool Update(std::string configName, const Json::Value& config) override;
    bool Remove(std::string configName, const Json::Value& config) override;
    [[nodiscard]] const std::string& Name() const override { return sName; }
    std::vector<::grpc::Service*> GetGrpcServices() override;

private:
    static const std::string sName;

    // configName -> OTLPForwardConfig
    std::unordered_map<std::string, std::shared_ptr<OTLPForwardConfig>> mMatchIndex;
    mutable std::shared_mutex mMatchIndexMutex;

    MetricsRecordRef mMetricsRecordRef;
    CounterPtr mLogInEventsTotal;
    CounterPtr mLogInSizeBytes;
    CounterPtr mMetricInEventsTotal;
    CounterPtr mTraceInEventsTotal;
    CounterPtr mDiscardedEventsTotal;
    TimeCounterPtr mTotalDelayMs;

    // gRPC service wrappers for Logs, Metrics, and Traces
    std::unique_ptr<OTLPLogsGrpcService> mLogsService;
    std::unique_ptr<OTLPMetricsGrpcService> mMetricsService;
    std::unique_ptr<OTLPTraceGrpcService> mTraceService;

    bool AddToIndex(std::string& configName, OTLPForwardConfig&& config, std::string& errorMsg);
    bool FindMatchingConfig(grpc::CallbackServerContext* context, std::shared_ptr<OTLPForwardConfig>& config) const;
    void ProcessLogExport(const opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest* request,
                          const std::shared_ptr<OTLPForwardConfig>& config,
                          grpc::Status& status);
    void ProcessMetricExport(const opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest* request,
                             const std::shared_ptr<OTLPForwardConfig>& config,
                             grpc::Status& status);
    void ProcessTraceExport(const opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest* request,
                            const std::shared_ptr<OTLPForwardConfig>& config,
                            grpc::Status& status);

#ifdef APSARA_UNIT_TEST_MAIN
    friend class OTLPForwardServiceUnittest;
#endif
    friend class OTLPLogsGrpcService;
    friend class OTLPMetricsGrpcService;
    friend class OTLPTraceGrpcService;
};

} // namespace logtail

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

#include "forward/otlp/OTLPForwardService.h"

#include <grpcpp/support/status.h>

#include <memory>

#include "common/Flags.h"
#include "common/ParamExtractor.h"
#include "common/TimeKeeper.h"
#include "logger/Logger.h"
#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/MetricValue.h"
#include "models/SpanEvent.h"
#include "models/PipelineEventGroup.h"
#include "runner/ProcessorRunner.h"

using namespace opentelemetry::proto::collector::logs::v1;
using namespace opentelemetry::proto::collector::metrics::v1;
using namespace opentelemetry::proto::collector::trace::v1;

namespace logtail {

const std::string OTLPForwardServiceImpl::sName = "OTLPForwardService";

std::vector<::grpc::Service*> OTLPForwardServiceImpl::GetGrpcServices() {
    return {mLogsService.get(), mMetricsService.get(), mTraceService.get()};
}

OTLPForwardServiceImpl::OTLPForwardServiceImpl(const std::string& address) : BaseService(address) {
    mLogsService = std::make_unique<OTLPLogsGrpcService>(this);
    mMetricsService = std::make_unique<OTLPMetricsGrpcService>(this);
    mTraceService = std::make_unique<OTLPTraceGrpcService>(this);

    WriteMetrics::GetInstance()->CreateMetricsRecordRef(
        mMetricsRecordRef,
        MetricCategory::METRIC_CATEGORY_COMPONENT,
        {{METRIC_LABEL_KEY_COMPONENT_NAME, "otlp_forward"}, {METRIC_LABEL_KEY_SERVICE_ADDRESS, address}});
    mLogInEventsTotal = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_IN_EVENTS_TOTAL);
    mLogInSizeBytes = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_IN_SIZE_BYTES);
    mMetricInEventsTotal = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_IN_EVENTS_TOTAL);
    mTraceInEventsTotal = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_IN_EVENTS_TOTAL);
    mDiscardedEventsTotal = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_DISCARDED_ITEMS_TOTAL);
    mTotalDelayMs = mMetricsRecordRef.CreateTimeCounter(METRIC_COMPONENT_TOTAL_DELAY_MS);
    WriteMetrics::GetInstance()->CommitMetricsRecordRef(mMetricsRecordRef);
}

bool OTLPForwardServiceImpl::Update(std::string configName, const Json::Value& config) {
    OTLPForwardConfig forwardConfig;
    forwardConfig.configName = configName;

    std::string errorMsg;
    int32_t queueKey = -1;
    if (!GetMandatoryIntParam(config, "QueueKey", queueKey, errorMsg)) {
        return false;
    }
    forwardConfig.queueKey = static_cast<QueueKey>(queueKey);

    int inputIndex = -1;
    if (!GetMandatoryIntParam(config, "InputIndex", inputIndex, errorMsg)) {
        return false;
    }
    forwardConfig.inputIndex = static_cast<size_t>(inputIndex);
    if (!AddToIndex(configName, std::move(forwardConfig), errorMsg)) {
        LOG_ERROR(sLogger, ("Update OTLP forward match rule failed", configName)("error", errorMsg));
        return false;
    }
    LOG_INFO(sLogger,
             ("OTLPForwardServiceImpl config updated", configName)("queueKey", queueKey)("inputIndex", inputIndex));
    return true;
}

bool OTLPForwardServiceImpl::Remove(std::string configName, const Json::Value& config) {
    std::unique_lock<std::shared_mutex> lock(mMatchIndexMutex);
    auto it = mMatchIndex.find(configName);
    if (it != mMatchIndex.end()) {
        mMatchIndex.erase(it);
        LOG_INFO(sLogger, ("OTLPForwardServiceImpl config removed", configName));
    }
    return true;
}

// ==================== Logs Export ====================

grpc::ServerUnaryReactor* OTLPLogsGrpcService::Export(
    grpc::CallbackServerContext* context,
    const ExportLogsServiceRequest* request,
    ExportLogsServiceResponse* response) {
    auto* reactor = context->DefaultReactor();
    grpc::Status status(grpc::StatusCode::NOT_FOUND, "No matching config");

    if (!request) {
        ADD_COUNTER(mImpl->mDiscardedEventsTotal, 1);
        reactor->Finish(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid request"));
        return reactor;
    }

    auto before = TimeKeeper::GetInstance()->NowMs();
    ADD_COUNTER(mImpl->mLogInEventsTotal, 1);

    std::shared_ptr<OTLPForwardConfig> config;
    if (mImpl->FindMatchingConfig(context, config)) {
        mImpl->ProcessLogExport(request, config, status);
    } else {
        ADD_COUNTER(mImpl->mDiscardedEventsTotal, 1);
    }

    ADD_COUNTER(mImpl->mTotalDelayMs, std::chrono::milliseconds(TimeKeeper::GetInstance()->NowMs() - before));
    reactor->Finish(status);
    return reactor;
}

void OTLPForwardServiceImpl::ProcessLogExport(const ExportLogsServiceRequest* request,
                                               const std::shared_ptr<OTLPForwardConfig>& config,
                                               grpc::Status& status) {
    auto eventGroup = PipelineEventGroup(std::make_shared<SourceBuffer>());
    int eventCount = 0;
    size_t totalBytes = 0;

    for (const auto& resourceLogs : request->resource_logs()) {
        for (const auto& scopeLogs : resourceLogs.scope_logs()) {
            for (const auto& logRecord : scopeLogs.log_records()) {
                auto* logEvent = eventGroup.AddLogEvent(true);

                // timestamp in nanoseconds
                uint64_t timeUnixNano = logRecord.time_unix_nano();
                logEvent->SetTimestamp(timeUnixNano / 1000000000,
                                       (timeUnixNano % 1000000000) / 1000);

                // body
                if (logRecord.has_body() && logRecord.body().has_string_value()) {
                    logEvent->SetContent("message", logRecord.body().string_value());
                }

                // severity
                if (!logRecord.severity_text().empty()) {
                    logEvent->SetContent("severity", logRecord.severity_text());
                }

                // trace/span ids
                if (!logRecord.trace_id().empty()) {
                    logEvent->SetContent("trace_id", logRecord.trace_id());
                }
                if (!logRecord.span_id().empty()) {
                    logEvent->SetContent("span_id", logRecord.span_id());
                }

                // attributes
                for (const auto& attr : logRecord.attributes()) {
                    if (attr.value().has_string_value()) {
                        logEvent->SetContent(attr.key(), attr.value().string_value());
                    } else if (attr.value().has_int_value()) {
                        logEvent->SetContent(attr.key(), std::to_string(attr.value().int_value()));
                    } else if (attr.value().has_double_value()) {
                        logEvent->SetContent(attr.key(), std::to_string(attr.value().double_value()));
                    }
                }

                eventCount++;
            }
        }

        // resource attributes as tags
        for (const auto& attr : resourceLogs.resource().attributes()) {
            if (attr.value().has_string_value()) {
                eventGroup.SetTag(attr.key(), attr.value().string_value());
            }
        }
    }

    totalBytes = request->ByteSizeLong();
    ADD_COUNTER(mLogInSizeBytes, totalBytes);

    if (eventCount == 0) {
        status = grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "No log events");
        return;
    }

    bool result = ProcessorRunner::GetInstance()->PushQueue(
        config->queueKey, config->inputIndex, std::move(eventGroup), 3);
    status = result ? grpc::Status::OK : grpc::Status(grpc::StatusCode::UNAVAILABLE, "Queue full, retry");
}

// ==================== Metrics Export ====================

grpc::ServerUnaryReactor* OTLPMetricsGrpcService::Export(
    grpc::CallbackServerContext* context,
    const ExportMetricsServiceRequest* request,
    ExportMetricsServiceResponse* response) {
    auto* reactor = context->DefaultReactor();
    grpc::Status status(grpc::StatusCode::NOT_FOUND, "No matching config");

    if (!request) {
        ADD_COUNTER(mImpl->mDiscardedEventsTotal, 1);
        reactor->Finish(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid request"));
        return reactor;
    }

    auto before = TimeKeeper::GetInstance()->NowMs();
    ADD_COUNTER(mImpl->mMetricInEventsTotal, 1);

    LOG_INFO(sLogger, ("OTLPMetricsGrpcService Export", "processing")("resourceMetrics", request->resource_metrics().size()));

    std::shared_ptr<OTLPForwardConfig> config;
    if (mImpl->FindMatchingConfig(context, config)) {
        mImpl->ProcessMetricExport(request, config, status);
    } else {
        ADD_COUNTER(mImpl->mDiscardedEventsTotal, 1);
    }

    ADD_COUNTER(mImpl->mTotalDelayMs, std::chrono::milliseconds(TimeKeeper::GetInstance()->NowMs() - before));
    reactor->Finish(status);
    return reactor;
}

void OTLPForwardServiceImpl::ProcessMetricExport(const ExportMetricsServiceRequest* request,
                                                  const std::shared_ptr<OTLPForwardConfig>& config,
                                                  grpc::Status& status) {
    auto eventGroup = PipelineEventGroup(std::make_shared<SourceBuffer>());
    int eventCount = 0;

    for (const auto& resourceMetrics : request->resource_metrics()) {
        // resource attributes as tags
        for (const auto& attr : resourceMetrics.resource().attributes()) {
            if (attr.value().has_string_value()) {
                eventGroup.SetTag(attr.key(), attr.value().string_value());
            }
        }

        for (const auto& scopeMetrics : resourceMetrics.scope_metrics()) {
            for (const auto& metric : scopeMetrics.metrics()) {
                // handle different metric types
                switch (metric.data_case()) {
                    case opentelemetry::proto::metrics::v1::Metric::DataCase::kGauge: {
                        for (const auto& dp : metric.gauge().data_points()) {
                            auto* metricEvent = eventGroup.AddMetricEvent(true);
                            metricEvent->SetName(metric.name());
                            metricEvent->SetValue<UntypedSingleValue>(dp.as_double());
                            eventCount++;
                        }
                        break;
                    }
                    case opentelemetry::proto::metrics::v1::Metric::DataCase::kSum: {
                        for (const auto& dp : metric.sum().data_points()) {
                            auto* metricEvent = eventGroup.AddMetricEvent(true);
                            metricEvent->SetName(metric.name());
                            metricEvent->SetValue<UntypedSingleValue>(dp.as_double());
                            eventCount++;
                        }
                        break;
                    }
                    case opentelemetry::proto::metrics::v1::Metric::DataCase::kHistogram: {
                        for (const auto& dp : metric.histogram().data_points()) {
                            auto* metricEvent = eventGroup.AddMetricEvent(true);
                            metricEvent->SetName(metric.name());
                            metricEvent->SetValue<UntypedSingleValue>(dp.sum());
                            eventCount++;
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }

    if (eventCount == 0) {
        status = grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "No metric events");
        return;
    }

    bool result = ProcessorRunner::GetInstance()->PushQueue(
        config->queueKey, config->inputIndex, std::move(eventGroup), 3);
    status = result ? grpc::Status::OK : grpc::Status(grpc::StatusCode::UNAVAILABLE, "Queue full, retry");
}

// ==================== Traces Export ====================

grpc::ServerUnaryReactor* OTLPTraceGrpcService::Export(
    grpc::CallbackServerContext* context,
    const ExportTraceServiceRequest* request,
    ExportTraceServiceResponse* response) {
    auto* reactor = context->DefaultReactor();
    grpc::Status status(grpc::StatusCode::NOT_FOUND, "No matching config");

    if (!request) {
        ADD_COUNTER(mImpl->mDiscardedEventsTotal, 1);
        reactor->Finish(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid request"));
        return reactor;
    }

    auto before = TimeKeeper::GetInstance()->NowMs();
    ADD_COUNTER(mImpl->mTraceInEventsTotal, 1);

    std::shared_ptr<OTLPForwardConfig> config;
    if (mImpl->FindMatchingConfig(context, config)) {
        mImpl->ProcessTraceExport(request, config, status);
    } else {
        ADD_COUNTER(mImpl->mDiscardedEventsTotal, 1);
    }

    ADD_COUNTER(mImpl->mTotalDelayMs, std::chrono::milliseconds(TimeKeeper::GetInstance()->NowMs() - before));
    reactor->Finish(status);
    return reactor;
}

void OTLPForwardServiceImpl::ProcessTraceExport(const ExportTraceServiceRequest* request,
                                                 const std::shared_ptr<OTLPForwardConfig>& config,
                                                 grpc::Status& status) {
    auto eventGroup = PipelineEventGroup(std::make_shared<SourceBuffer>());
    int eventCount = 0;

    for (const auto& resourceSpans : request->resource_spans()) {
        for (const auto& scopeSpans : resourceSpans.scope_spans()) {
            for (const auto& span : scopeSpans.spans()) {
                auto* spanEvent = eventGroup.AddSpanEvent(true);
                eventCount++;

                // trace_id, span_id
                spanEvent->SetTraceId(span.trace_id());
                spanEvent->SetSpanId(span.span_id());
                spanEvent->SetParentSpanId(span.parent_span_id());
                spanEvent->SetName(span.name());

                // timestamps
                spanEvent->SetStartTimeNs(span.start_time_unix_nano());
                spanEvent->SetEndTimeNs(span.end_time_unix_nano());

                // status
                if (span.has_status()) {
                    switch (span.status().code()) {
                        case opentelemetry::proto::trace::v1::Status::STATUS_CODE_OK:
                            spanEvent->SetStatus(SpanEvent::StatusCode::Ok);
                            break;
                        case opentelemetry::proto::trace::v1::Status::STATUS_CODE_ERROR:
                            spanEvent->SetStatus(SpanEvent::StatusCode::Error);
                            break;
                        default:
                            spanEvent->SetStatus(SpanEvent::StatusCode::Unset);
                            break;
                    }
                }

                // kind
                switch (span.kind()) {
                    case opentelemetry::proto::trace::v1::Span::SPAN_KIND_CLIENT:
                        spanEvent->SetKind(SpanEvent::Kind::Client);
                        break;
                    case opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER:
                        spanEvent->SetKind(SpanEvent::Kind::Server);
                        break;
                    case opentelemetry::proto::trace::v1::Span::SPAN_KIND_PRODUCER:
                        spanEvent->SetKind(SpanEvent::Kind::Producer);
                        break;
                    case opentelemetry::proto::trace::v1::Span::SPAN_KIND_CONSUMER:
                        spanEvent->SetKind(SpanEvent::Kind::Consumer);
                        break;
                    default:
                        spanEvent->SetKind(SpanEvent::Kind::Internal);
                        break;
                }

                // attributes as tags
                for (const auto& attr : span.attributes()) {
                    if (attr.value().has_string_value()) {
                        spanEvent->SetTag(attr.key(), attr.value().string_value());
                    } else if (attr.value().has_int_value()) {
                        spanEvent->SetTag(attr.key(), std::to_string(attr.value().int_value()));
                    }
                }
            }
        }

        // resource attributes as tags on eventGroup
        for (const auto& attr : resourceSpans.resource().attributes()) {
            if (attr.value().has_string_value()) {
                eventGroup.SetTag(attr.key(), attr.value().string_value());
            }
        }
    }

    if (eventCount == 0) {
        status = grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "No span events");
        return;
    }

    LOG_INFO(sLogger,
             ("OTLPForwardService ProcessTraceExport", "sending to queue")("config", config->configName)(
                 "queueKey", config->queueKey)("inputIndex", config->inputIndex)("spanCount", eventCount));

    bool result = ProcessorRunner::GetInstance()->PushQueue(
        config->queueKey, config->inputIndex, std::move(eventGroup), 3);
    status = result ? grpc::Status::OK : grpc::Status(grpc::StatusCode::UNAVAILABLE, "Queue full, retry");
}

// ==================== Common ====================

bool OTLPForwardServiceImpl::AddToIndex(std::string& configName, OTLPForwardConfig&& config, std::string& errorMsg) {
    errorMsg.clear();
    std::unique_lock<std::shared_mutex> lock(mMatchIndexMutex);
    if (!configName.empty()) {
        mMatchIndex[configName] = std::make_shared<OTLPForwardConfig>(std::move(config));
        return true;
    }
    errorMsg = "Empty config name";
    return false;
}

bool OTLPForwardServiceImpl::FindMatchingConfig(grpc::CallbackServerContext* context,
                                                 std::shared_ptr<OTLPForwardConfig>& config) const {
    std::shared_lock<std::shared_mutex> lock(mMatchIndexMutex);

    // Try to match via x-otlp-apm-configname metadata first
    const auto& metadata = context->client_metadata();
    for (const auto& metadataPair : metadata) {
        if (metadataPair.first != "x-otlp-apm-configname") {
            continue;
        }
        std::string value(metadataPair.second.data(), metadataPair.second.size());
        auto it = mMatchIndex.find(value);
        if (it != mMatchIndex.end()) {
            config = it->second;
            return true;
        }
    }

    // Fallback: if only one config is registered, use it (onetime pipeline case)
    if (mMatchIndex.size() == 1) {
        config = mMatchIndex.begin()->second;
        return true;
    }

    return false;
}

} // namespace logtail

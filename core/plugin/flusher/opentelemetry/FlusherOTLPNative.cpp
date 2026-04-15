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

#include "plugin/flusher/opentelemetry/FlusherOTLPNative.h"

#include <chrono>

#include "collection_pipeline/queue/SenderQueueManager.h"
#include "common/Flags.h"
#include "common/ParamExtractor.h"
#include "logger/Logger.h"
#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/MetricValue.h"
#include "models/RawEvent.h"
#include "models/SpanEvent.h"

namespace logtail {

const std::string FlusherOTLPNative::sName = "flusher_otlp_native";

using OTLPGrpcDataType = OTLPGrpcCallContext::DataType;

bool FlusherOTLPNative::Init(const Json::Value& config, Json::Value& /*optionalGoPipeline*/) {
    std::string errorMsg;

    if (!GetMandatoryStringParam(config, "Endpoint", mEndpoint, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    if (config.isMember("TimeoutMs")) {
        mTimeoutMs = config["TimeoutMs"].asInt();
    }
    if (config.isMember("EnableTLS")) {
        mEnableTLS = config["EnableTLS"].asBool();
    }

    // Parse extra headers
    if (config.isMember("Headers") && config["Headers"].isObject()) {
        const auto& headers = config["Headers"];
        auto memberNames = headers.getMemberNames();
        for (const auto& name : memberNames) {
            mHeaders[name] = headers[name].asString();
        }
    }

    // Create sender queue
    GenerateQueueKey(mEndpoint);
    SenderQueueManager::GetInstance()->CreateQueue(mQueueKey, mPluginID, mEndpoint, *mContext);

    // Metrics
    mSendCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL);
    mSendSuccessCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_SUCCESSFUL_EVENTS_TOTAL);
    mSendFailCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_FAILED_EVENTS_TOTAL);
    mDiscardedEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_DISCARDED_EVENTS_TOTAL);

    LOG_INFO(sLogger,
             ("FlusherOTLPNative initialized", "success")("endpoint", mEndpoint)("timeout_ms", mTimeoutMs)(
                 "tls", mEnableTLS)("headers", mHeaders.size()));
    return true;
}

bool FlusherOTLPNative::Start() {
    if (!CreateGrpcChannel()) {
        return false;
    }
    LOG_INFO(sLogger, ("FlusherOTLPNative started", "success")("endpoint", mEndpoint));
    return true;
}

bool FlusherOTLPNative::Stop(bool isPipelineRemoving) {
    mIsStopping.store(true);

    {
        std::unique_lock<std::mutex> lock(mStopMutex);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        mStopCV.wait_until(lock, deadline, [this]() { return mInFlightCnt.load() == 0; });
    }

    if (mInFlightCnt.load() > 0) {
        LOG_WARNING(sLogger, ("FlusherOTLPNative Stop timeout, cancelling in-flight RPCs", mInFlightCnt.load()));
        CancelAllInFlight();

        std::unique_lock<std::mutex> lock(mStopMutex);
        auto cancelDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        mStopCV.wait_until(lock, cancelDeadline, [this]() { return mInFlightCnt.load() == 0; });
        if (mInFlightCnt.load() > 0) {
            LOG_ERROR(sLogger, ("FlusherOTLPNative cancelled RPCs still not drained", mInFlightCnt.load()));
        }
    }

    mChannel.reset();
    mLogsStub.reset();
    mMetricsStub.reset();
    mTraceStub.reset();

    return Flusher::Stop(isPipelineRemoving);
}

// ==================== Send (SenderQueue pattern) ====================

bool FlusherOTLPNative::Send(PipelineEventGroup&& g) {
    if (g.GetEvents().empty()) {
        return true;
    }

    ADD_COUNTER(mSendCnt, 1);

    const auto& events = g.GetEvents();
    bool hasLog = false, hasMetric = false, hasSpan = false;
    for (const auto& event : events) {
        if (event.Is<LogEvent>() || event.Is<RawEvent>())
            hasLog = true;
        else if (event.Is<MetricEvent>())
            hasMetric = true;
        else if (event.Is<SpanEvent>())
            hasSpan = true;
    }

    if (!hasLog && !hasMetric && !hasSpan) {
        return true;
    }

    bool allOk = true;
    std::string errMsg;

    if (hasLog) {
        std::string data;
        if (!SerializeLogsToOTLP(g, data, errMsg)) {
            LOG_WARNING(sLogger, ("failed to serialize OTLP logs", errMsg)("config", mContext->GetConfigName()));
            allOk = false;
        } else {
            const size_t rawSize = data.size();
            auto item = std::make_unique<OTLPSenderQueueItem>(
                std::move(data), rawSize, this, mQueueKey, OTLPGrpcDataType::Logs);
            allOk &= PushToQueue(std::move(item));
        }
    }

    if (hasMetric) {
        std::string data;
        if (!SerializeMetricsToOTLP(g, data, errMsg)) {
            LOG_WARNING(sLogger, ("failed to serialize OTLP metrics", errMsg)("config", mContext->GetConfigName()));
            allOk = false;
        } else {
            const size_t rawSize = data.size();
            auto item = std::make_unique<OTLPSenderQueueItem>(
                std::move(data), rawSize, this, mQueueKey, OTLPGrpcDataType::Metrics);
            allOk &= PushToQueue(std::move(item));
        }
    }

    if (hasSpan) {
        std::string data;
        if (!SerializeTracesToOTLP(g, data, errMsg)) {
            LOG_WARNING(sLogger, ("failed to serialize OTLP traces", errMsg)("config", mContext->GetConfigName()));
            allOk = false;
        } else {
            const size_t rawSize = data.size();
            auto item = std::make_unique<OTLPSenderQueueItem>(
                std::move(data), rawSize, this, mQueueKey, OTLPGrpcDataType::Traces);
            allOk &= PushToQueue(std::move(item));
        }
    }

    return allOk;
}

// ==================== Serialize helpers ====================

bool FlusherOTLPNative::SerializeLogsToOTLP(const PipelineEventGroup& group,
                                            std::string& serializedData,
                                            std::string& errMsg) {
    opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest request;
    auto* resourceLogs = request.add_resource_logs();
    auto* scopeLogs = resourceLogs->add_scope_logs();

    for (const auto& event : group.GetEvents()) {
        if (event.Is<LogEvent>()) {
            const auto& logEvent = event.Cast<LogEvent>();
            auto* logRecord = scopeLogs->add_log_records();
            auto ts = logEvent.GetTimestamp();
            auto tsNs = logEvent.GetTimestampNanosecond();
            uint64_t timeUnixNano = static_cast<uint64_t>(ts) * 1000000000ULL;
            if (tsNs.has_value()) {
                timeUnixNano += tsNs.value();
            }
            logRecord->set_time_unix_nano(timeUnixNano);
            auto* body = logRecord->mutable_body();
            auto msg = logEvent.GetContent("content");
            if (!msg.empty()) {
                body->set_string_value(std::string(msg.data(), msg.size()));
            }
            auto level = logEvent.GetLevel();
            if (!level.empty()) {
                logRecord->set_severity_text(std::string(level.data(), level.size()));
            }
            for (auto it = logEvent.begin(); it != logEvent.end(); ++it) {
                auto* attr = logRecord->add_attributes();
                attr->set_key(std::string(it->first.data(), it->first.size()));
                attr->mutable_value()->set_string_value(std::string(it->second.data(), it->second.size()));
            }
        } else if (event.Is<RawEvent>()) {
            const auto& rawEvent = event.Cast<RawEvent>();
            auto* logRecord = scopeLogs->add_log_records();
            auto ts = rawEvent.GetTimestamp();
            auto tsNs = rawEvent.GetTimestampNanosecond();
            uint64_t timeUnixNano = static_cast<uint64_t>(ts) * 1000000000ULL;
            if (tsNs.has_value()) {
                timeUnixNano += tsNs.value();
            }
            logRecord->set_time_unix_nano(timeUnixNano);
            auto* body = logRecord->mutable_body();
            body->set_string_value(std::string(rawEvent.GetContent().data(), rawEvent.GetContent().size()));
        }
    }

    if (!request.SerializeToString(&serializedData)) {
        errMsg = "failed to serialize logs request";
        return false;
    }
    return true;
}

bool FlusherOTLPNative::SerializeMetricsToOTLP(const PipelineEventGroup& group,
                                               std::string& serializedData,
                                               std::string& errMsg) {
    opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest request;
    auto* resourceMetrics = request.add_resource_metrics();
    auto* scopeMetrics = resourceMetrics->add_scope_metrics();

    for (const auto& event : group.GetEvents()) {
        if (!event.Is<MetricEvent>())
            continue;
        const auto& metricEvent = event.Cast<MetricEvent>();
        auto* metric = scopeMetrics->add_metrics();
        metric->set_name(std::string(metricEvent.GetName().data(), metricEvent.GetName().size()));

        auto* gauge = metric->mutable_gauge();
        auto* dp = gauge->add_data_points();

        auto ts = metricEvent.GetTimestamp();
        auto tsNs = metricEvent.GetTimestampNanosecond();
        uint64_t timeUnixNano = static_cast<uint64_t>(ts) * 1000000000ULL;
        if (tsNs.has_value()) {
            timeUnixNano += tsNs.value();
        }
        dp->set_time_unix_nano(timeUnixNano);

        if (auto* untyped = metricEvent.GetValue<UntypedSingleValue>()) {
            dp->set_as_double(untyped->mValue);
        } else {
            dp->set_as_double(0.0);
        }

        for (auto tagIt = metricEvent.TagsBegin(); tagIt != metricEvent.TagsEnd(); ++tagIt) {
            auto* label = dp->add_attributes();
            label->set_key(std::string(tagIt->first.data(), tagIt->first.size()));
            label->mutable_value()->set_string_value(std::string(tagIt->second.data(), tagIt->second.size()));
        }
    }

    if (!request.SerializeToString(&serializedData)) {
        errMsg = "failed to serialize metrics request";
        return false;
    }
    return true;
}

bool FlusherOTLPNative::SerializeTracesToOTLP(const PipelineEventGroup& group,
                                              std::string& serializedData,
                                              std::string& errMsg) {
    opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest request;
    auto* resourceSpans = request.add_resource_spans();
    auto* scopeSpans = resourceSpans->add_scope_spans();

    for (const auto& event : group.GetEvents()) {
        if (!event.Is<SpanEvent>())
            continue;
        const auto& spanEvent = event.Cast<SpanEvent>();
        auto* span = scopeSpans->add_spans();

        span->set_trace_id(std::string(spanEvent.GetTraceId().data(), spanEvent.GetTraceId().size()));
        span->set_span_id(std::string(spanEvent.GetSpanId().data(), spanEvent.GetSpanId().size()));
        span->set_parent_span_id(std::string(spanEvent.GetParentSpanId().data(), spanEvent.GetParentSpanId().size()));
        span->set_name(std::string(spanEvent.GetName().data(), spanEvent.GetName().size()));
        span->set_start_time_unix_nano(spanEvent.GetStartTimeNs());
        span->set_end_time_unix_nano(spanEvent.GetEndTimeNs());

        switch (spanEvent.GetKind()) {
            case SpanEvent::Kind::Internal:
                span->set_kind(opentelemetry::proto::trace::v1::Span::SPAN_KIND_INTERNAL);
                break;
            case SpanEvent::Kind::Server:
                span->set_kind(opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER);
                break;
            case SpanEvent::Kind::Client:
                span->set_kind(opentelemetry::proto::trace::v1::Span::SPAN_KIND_CLIENT);
                break;
            case SpanEvent::Kind::Producer:
                span->set_kind(opentelemetry::proto::trace::v1::Span::SPAN_KIND_PRODUCER);
                break;
            case SpanEvent::Kind::Consumer:
                span->set_kind(opentelemetry::proto::trace::v1::Span::SPAN_KIND_CONSUMER);
                break;
            default:
                span->set_kind(opentelemetry::proto::trace::v1::Span::SPAN_KIND_UNSPECIFIED);
                break;
        }

        switch (spanEvent.GetStatus()) {
            case SpanEvent::StatusCode::Ok:
                span->mutable_status()->set_code(opentelemetry::proto::trace::v1::Status::STATUS_CODE_OK);
                break;
            case SpanEvent::StatusCode::Error:
                span->mutable_status()->set_code(opentelemetry::proto::trace::v1::Status::STATUS_CODE_ERROR);
                break;
            default:
                span->mutable_status()->set_code(opentelemetry::proto::trace::v1::Status::STATUS_CODE_UNSET);
                break;
        }

        for (auto tagIt = spanEvent.TagsBegin(); tagIt != spanEvent.TagsEnd(); ++tagIt) {
            auto* attr = span->add_attributes();
            attr->set_key(std::string(tagIt->first.data(), tagIt->first.size()));
            attr->mutable_value()->set_string_value(std::string(tagIt->second.data(), tagIt->second.size()));
        }
    }

    if (!request.SerializeToString(&serializedData)) {
        errMsg = "failed to serialize traces request";
        return false;
    }
    return true;
}

// ==================== BuildGrpcRequest (reconstruct from binary) ====================

bool FlusherOTLPNative::BuildGrpcRequest(SenderQueueItem* item,
                                         std::unique_ptr<OTLPGrpcCallContext>& ctx,
                                         bool* keepItem,
                                         std::string* errMsg) {
    auto* otlpItem = static_cast<OTLPSenderQueueItem*>(item);
    ctx = std::make_unique<OTLPGrpcCallContext>();
    ctx->item = item;
    ctx->type = otlpItem->dataType;

    switch (otlpItem->dataType) {
        case OTLPGrpcDataType::Logs: {
            ctx->logsReq = std::make_unique<opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest>();
            ctx->logsResp = std::make_unique<opentelemetry::proto::collector::logs::v1::ExportLogsServiceResponse>();
            if (!ctx->logsReq->ParseFromString(item->mData)) {
                *errMsg = "failed to parse logs request from binary";
                *keepItem = false;
                return false;
            }
            break;
        }
        case OTLPGrpcDataType::Metrics: {
            ctx->metricsReq
                = std::make_unique<opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest>();
            ctx->metricsResp
                = std::make_unique<opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceResponse>();
            if (!ctx->metricsReq->ParseFromString(item->mData)) {
                *errMsg = "failed to parse metrics request from binary";
                *keepItem = false;
                return false;
            }
            break;
        }
        case OTLPGrpcDataType::Traces: {
            ctx->traceReq = std::make_unique<opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest>();
            ctx->traceResp = std::make_unique<opentelemetry::proto::collector::trace::v1::ExportTraceServiceResponse>();
            if (!ctx->traceReq->ParseFromString(item->mData)) {
                *errMsg = "failed to parse traces request from binary";
                *keepItem = false;
                return false;
            }
            break;
        }
        default:
            *errMsg = "unknown OTLP data type in queue item";
            *keepItem = false;
            return false;
    }

    return true;
}

// ==================== OnSendDone ====================

void FlusherOTLPNative::OnSendDone(const grpc::Status& status, SenderQueueItem* item) {
    if (status.ok()) {
        DealSenderQueueItemAfterSend(item, false); // release item
    } else {
        DealSenderQueueItemAfterSend(item, true); // keep for retry by SenderQueue
    }
}

// ==================== HandleGrpcCallback (called by GrpcSink) ====================

void FlusherOTLPNative::HandleGrpcCallback(grpc::Status&& status, OTLPGrpcCallContext* ctx) {
    UntrackContext(ctx->context.get());

    if (status.ok()) {
        ADD_COUNTER(mSendSuccessCnt, 1);
    } else {
        ADD_COUNTER(mSendFailCnt, 1);
        if (status.error_code() == grpc::StatusCode::CANCELLED && mIsStopping.load()) {
            LOG_INFO(sLogger, ("FlusherOTLPNative RPC cancelled during shutdown", ""));
        } else {
            LOG_WARNING(sLogger,
                        ("FlusherOTLPNative async Export failed", status.error_message())("code", status.error_code()));
        }
    }

    OnSendDone(status, ctx->item);
    DecInFlight();
    delete ctx;
}

// ==================== In-flight context tracking ====================

void FlusherOTLPNative::TrackContext(grpc::ClientContext* ctx) {
    std::lock_guard<std::mutex> lock(mContextsMutex);
    mInFlightContexts.insert(ctx);
}

void FlusherOTLPNative::UntrackContext(grpc::ClientContext* ctx) {
    std::lock_guard<std::mutex> lock(mContextsMutex);
    mInFlightContexts.erase(ctx);
}

void FlusherOTLPNative::CancelAllInFlight() {
    std::lock_guard<std::mutex> lock(mContextsMutex);
    for (auto* ctx : mInFlightContexts) {
        ctx->TryCancel();
    }
}

// ==================== gRPC Channel ====================

bool FlusherOTLPNative::CreateGrpcChannel() {
    try {
        grpc::ChannelArguments args;
        args.SetInt(GRPC_ARG_MAX_SEND_MESSAGE_LENGTH, 64 * 1024 * 1024); // 64MB
        args.SetInt(GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH, 64 * 1024 * 1024);

        std::shared_ptr<grpc::ChannelCredentials> creds;
        if (mEnableTLS) {
            creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
        } else {
            creds = grpc::InsecureChannelCredentials();
        }

        mChannel = grpc::CreateCustomChannel(mEndpoint, creds, args);
        if (!mChannel) {
            LOG_ERROR(sLogger, ("FlusherOTLPNative failed to create gRPC channel", mEndpoint));
            return false;
        }

        mLogsStub = OTLPLogsService::NewStub(mChannel);
        mMetricsStub = OTLPMetricsService::NewStub(mChannel);
        mTraceStub = OTLPTraceService::NewStub(mChannel);

        LOG_INFO(sLogger, ("FlusherOTLPNative gRPC channel created", mEndpoint));
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(sLogger, ("FlusherOTLPNative exception creating gRPC channel", e.what()));
        return false;
    }
}

} // namespace logtail

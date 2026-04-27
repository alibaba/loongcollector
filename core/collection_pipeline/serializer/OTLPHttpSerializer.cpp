/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "collection_pipeline/serializer/OTLPHttpSerializer.h"

#include "json/writer.h"

#include "google/protobuf/json/json.h"
#include "google/protobuf/message.h"
#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/RawEvent.h"
#include "models/SpanEvent.h"
#include "protobuf/opentelemetry/proto/collector/logs/v1/logs_service.pb.h"
#include "protobuf/opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"
#include "protobuf/opentelemetry/proto/collector/trace/v1/trace_service.pb.h"
#include "protobuf/opentelemetry/proto/common/v1/common.pb.h"
#include "protobuf/opentelemetry/proto/logs/v1/logs.pb.h"
#include "protobuf/opentelemetry/proto/metrics/v1/metrics.pb.h"
#include "protobuf/opentelemetry/proto/resource/v1/resource.pb.h"
#include "protobuf/opentelemetry/proto/trace/v1/trace.pb.h"

namespace logtail {

using namespace opentelemetry::proto::collector::logs::v1;
using namespace opentelemetry::proto::collector::metrics::v1;
using namespace opentelemetry::proto::collector::trace::v1;
using namespace opentelemetry::proto::logs::v1;
using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::trace::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::resource::v1;

using google::protobuf::json::MessageToJsonString;
using google::protobuf::json::PrintOptions;

static void SetProtoAttribute(KeyValue& kv, const StringView& key, const StringView& value) {
    kv.set_key(std::string(key.data(), key.size()));
    kv.mutable_value()->set_string_value(std::string(value.data(), value.size()));
}

static void SetResourceAttributes(Resource& resource, const SizedMap& tags) {
    for (const auto& [key, val] : tags.mInner) {
        auto* attr = resource.add_attributes();
        SetProtoAttribute(*attr, key, val);
    }
}

static void SerializeRawEvents(const BatchedEvents& batch, ScopeLogs& scopeLogs) {
    for (const auto& event : batch.mEvents) {
        if (!event.Is<RawEvent>()) {
            continue;
        }
        const auto& rawEvent = event.Cast<RawEvent>();
        auto* logRecord = scopeLogs.add_log_records();

        auto ts = rawEvent.GetTimestamp();
        uint64_t timeUnixNano = static_cast<uint64_t>(ts) * 1000000000ULL;
        auto tsNs = rawEvent.GetTimestampNanosecond();
        if (tsNs.has_value()) {
            timeUnixNano += tsNs.value();
        }
        logRecord->set_time_unix_nano(timeUnixNano);

        logRecord->mutable_body()->set_string_value(
            std::string(rawEvent.GetContent().data(), rawEvent.GetContent().size()));
    }
}

static void SerializeLogEvents(const BatchedEvents& batch, ScopeLogs& scopeLogs) {
    for (const auto& event : batch.mEvents) {
        if (!event.Is<LogEvent>()) {
            continue;
        }
        const auto& logEvent = event.Cast<LogEvent>();
        auto* logRecord = scopeLogs.add_log_records();

        auto ts = logEvent.GetTimestamp();
        uint64_t timeUnixNano = static_cast<uint64_t>(ts) * 1000000000ULL;
        auto tsNs = logEvent.GetTimestampNanosecond();
        if (tsNs.has_value()) {
            timeUnixNano += tsNs.value();
        }
        logRecord->set_time_unix_nano(timeUnixNano);

        auto msg = logEvent.GetContent("content");
        if (!msg.empty()) {
            logRecord->mutable_body()->set_string_value(std::string(msg.data(), msg.size()));
        }

        auto level = logEvent.GetLevel();
        if (!level.empty()) {
            logRecord->set_severity_text(std::string(level.data(), level.size()));
        }

        for (auto it = logEvent.begin(); it != logEvent.end(); ++it) {
            auto* attr = logRecord->add_attributes();
            SetProtoAttribute(*attr, it->first, it->second);
        }
    }
}

static void SerializeMetricEvents(const BatchedEvents& batch, ScopeMetrics& scopeMetrics) {
    for (const auto& event : batch.mEvents) {
        if (!event.Is<MetricEvent>()) {
            continue;
        }
        const auto& metricEvent = event.Cast<MetricEvent>();
        auto* metric = scopeMetrics.add_metrics();
        metric->set_name(std::string(metricEvent.GetName().data(), metricEvent.GetName().size()));

        auto* gauge = metric->mutable_gauge();
        auto* dp = gauge->add_data_points();

        auto ts = metricEvent.GetTimestamp();
        uint64_t timeUnixNano = static_cast<uint64_t>(ts) * 1000000000ULL;
        auto tsNs = metricEvent.GetTimestampNanosecond();
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
            auto* attr = dp->add_attributes();
            SetProtoAttribute(*attr, tagIt->first, tagIt->second);
        }
    }
}

static void SerializeSpanEvents(const BatchedEvents& batch, ScopeSpans& scopeSpans) {
    for (const auto& event : batch.mEvents) {
        if (!event.Is<SpanEvent>()) {
            continue;
        }
        const auto& spanEvent = event.Cast<SpanEvent>();
        auto* span = scopeSpans.add_spans();

        span->set_trace_id(std::string(spanEvent.GetTraceId().data(), spanEvent.GetTraceId().size()));
        span->set_span_id(std::string(spanEvent.GetSpanId().data(), spanEvent.GetSpanId().size()));
        span->set_parent_span_id(std::string(spanEvent.GetParentSpanId().data(), spanEvent.GetParentSpanId().size()));
        span->set_name(std::string(spanEvent.GetName().data(), spanEvent.GetName().size()));
        span->set_start_time_unix_nano(spanEvent.GetStartTimeNs());
        span->set_end_time_unix_nano(spanEvent.GetEndTimeNs());

        switch (spanEvent.GetKind()) {
            case SpanEvent::Kind::Internal:
                span->set_kind(Span::SPAN_KIND_INTERNAL);
                break;
            case SpanEvent::Kind::Server:
                span->set_kind(Span::SPAN_KIND_SERVER);
                break;
            case SpanEvent::Kind::Client:
                span->set_kind(Span::SPAN_KIND_CLIENT);
                break;
            case SpanEvent::Kind::Producer:
                span->set_kind(Span::SPAN_KIND_PRODUCER);
                break;
            case SpanEvent::Kind::Consumer:
                span->set_kind(Span::SPAN_KIND_CONSUMER);
                break;
            default:
                span->set_kind(Span::SPAN_KIND_UNSPECIFIED);
                break;
        }

        switch (spanEvent.GetStatus()) {
            case SpanEvent::StatusCode::Ok:
                span->mutable_status()->set_code(Status::STATUS_CODE_OK);
                break;
            case SpanEvent::StatusCode::Error:
                span->mutable_status()->set_code(Status::STATUS_CODE_ERROR);
                break;
            default:
                span->mutable_status()->set_code(Status::STATUS_CODE_UNSET);
                break;
        }

        for (auto tagIt = spanEvent.TagsBegin(); tagIt != spanEvent.TagsEnd(); ++tagIt) {
            auto* attr = span->add_attributes();
            SetProtoAttribute(*attr, tagIt->first, tagIt->second);
        }
    }
}

// Scan batch to detect event types and build the corresponding protobuf message.
// Returns event type index (0=logs, 1=metrics, 2=traces) or -1 if empty.
// Caller owns the returned Message* and must delete it.
static int BuildProtobufMessage(const BatchedEvents& batch, google::protobuf::Message*& msg, std::string& errorMsg) {
    bool hasLog = false, hasMetric = false, hasSpan = false, hasRaw = false;
    for (const auto& event : batch.mEvents) {
        if (event.Is<LogEvent>())
            hasLog = true;
        else if (event.Is<MetricEvent>())
            hasMetric = true;
        else if (event.Is<SpanEvent>())
            hasSpan = true;
        else if (event.Is<RawEvent>())
            hasRaw = true;
    }

    if (!hasLog && !hasMetric && !hasSpan && !hasRaw) {
        return -1;
    }

    if (hasRaw || hasLog) {
        auto* logReq = new ExportLogsServiceRequest();
        auto* resourceLogs = logReq->add_resource_logs();
        SetResourceAttributes(*resourceLogs->mutable_resource(), batch.mTags);
        auto* scopeLogs = resourceLogs->add_scope_logs();
        if (hasRaw) {
            SerializeRawEvents(batch, *scopeLogs);
        }
        if (hasLog) {
            SerializeLogEvents(batch, *scopeLogs);
        }
        msg = logReq;
        return 0;
    }

    if (hasMetric) {
        auto* metricReq = new ExportMetricsServiceRequest();
        auto* resourceMetrics = metricReq->add_resource_metrics();
        SetResourceAttributes(*resourceMetrics->mutable_resource(), batch.mTags);
        auto* scopeMetrics = resourceMetrics->add_scope_metrics();
        SerializeMetricEvents(batch, *scopeMetrics);
        msg = metricReq;
        return 1;
    }

    if (hasSpan) {
        auto* traceReq = new ExportTraceServiceRequest();
        auto* resourceSpans = traceReq->add_resource_spans();
        SetResourceAttributes(*resourceSpans->mutable_resource(), batch.mTags);
        auto* scopeSpans = resourceSpans->add_scope_spans();
        SerializeSpanEvents(batch, *scopeSpans);
        msg = traceReq;
        return 2;
    }

    return -1;
}

bool OTLPEventGroupSerializer::SerializeToBinaryString(BatchedEvents&& batch, std::string& res, std::string& errorMsg) {
    errorMsg.clear();

    google::protobuf::Message* msg = nullptr;
    int type = BuildProtobufMessage(batch, msg, errorMsg);
    if (type < 0) {
        return true;
    }

    if (!msg->SerializeToString(&res)) {
        errorMsg = "Failed to serialize OTLP request to binary protobuf";
        delete msg;
        return false;
    }
    delete msg;
    return true;
}

bool OTLPEventGroupSerializer::Serialize(BatchedEvents&& batch, std::string& res, std::string& errorMsg) {
    errorMsg.clear();

    google::protobuf::Message* msg = nullptr;
    int type = BuildProtobufMessage(batch, msg, errorMsg);
    if (type < 0) {
        return true;
    }

    try {
        PrintOptions opts;
        opts.always_print_fields_with_no_presence = true;
        opts.preserve_proto_field_names = true;

        auto status = MessageToJsonString(*msg, &res, opts);
        delete msg;
        if (!status.ok()) {
            errorMsg = "Failed to serialize to OTLP JSON: " + std::string(status.ToString());
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        errorMsg = std::string("Exception during OTLP JSON serialization: ") + e.what();
        delete msg;
        return false;
    }
}

} // namespace logtail

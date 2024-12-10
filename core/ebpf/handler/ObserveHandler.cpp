// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <thread>
#include <mutex>
#include <iostream>

#include "ebpf/handler/ObserveHandler.h"
#include "pipeline/PipelineContext.h"
#include "common/RuntimeUtil.h"
#include "ebpf/SourceManager.h"
#include "models/SpanEvent.h"
#include "models/PipelineEventGroup.h"
#include "models/PipelineEvent.h"
#include "logger/Logger.h"
#include "pipeline/queue/ProcessQueueManager.h"
#include "pipeline/queue/ProcessQueueItem.h"

namespace logtail {
namespace ebpf {

#define ADD_STATUS_METRICS(METRIC_NAME, FIELD_NAME, VALUE) \
    {if (!inner->FIELD_NAME) return; \
    auto event = group.AddMetricEvent(); \
    for (const auto& tag : measure->tags_) { \
        event->SetTag(tag.first, tag.second); \
    } \
    event->SetTag(std::string("status_code"), std::string(VALUE)); \
    event->SetName(METRIC_NAME); \
    event->SetTimestamp(ts); \
    event->SetValue(UntypedSingleValue{(double)inner->FIELD_NAME});} \

#define GENERATE_METRICS(FUNC_NAME, MEASURE_TYPE, INNER_TYPE, METRIC_NAME, FIELD_NAME) \
void FUNC_NAME(PipelineEventGroup& group, std::unique_ptr<Measure>& measure, uint64_t ts) { \
    if (measure->type_ != MEASURE_TYPE) return; \
    auto inner = static_cast<INNER_TYPE*>(measure->inner_measure_.get()); \
    if (!inner->FIELD_NAME) return; \
    auto event = group.AddMetricEvent(); \
    for (const auto& tag : measure->tags_) { \
        event->SetTag(tag.first, tag.second); \
    } \
    event->SetName(METRIC_NAME); \
    event->SetTimestamp(ts); \
    event->SetValue(UntypedSingleValue{(double)inner->FIELD_NAME}); \
}

void OtelMeterHandler::handle(std::vector<std::unique_ptr<ApplicationBatchMeasure>>& measures, uint64_t timestamp) {
    if (measures.empty()) return;

    for (const auto& appBatchMeasures : measures) {
        PipelineEventGroup eventGroup(std::make_shared<SourceBuffer>());
        for (const auto& measure : appBatchMeasures->measures_) {
            auto type = measure->type_;
            if (type == MeasureType::MEASURE_TYPE_APP) {
                auto inner = static_cast<AppSingleMeasure*>(measure->inner_measure_.get());
                auto event = eventGroup.AddMetricEvent();
                for (const auto& tag : measure->tags_) {
                    event->SetTag(tag.first, tag.second);
                }
                event->SetName("service_requests_total");
                event->SetTimestamp(timestamp);
                event->SetValue(UntypedSingleValue{(double)inner->request_total_});
            }
            mProcessTotalCnt++;
        }
#ifdef APSARA_UNIT_TEST_MAIN
        continue;
#endif
        std::unique_ptr<ProcessQueueItem> item = std::make_unique<ProcessQueueItem>(std::move(eventGroup), mPluginIdx);
        if (ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
            LOG_WARNING(sLogger, ("configName", mCtx->GetConfigName())("pluginIdx",mPluginIdx)("[Otel Metrics] push queue failed!", ""));
        }
        
    }
    return;
}

void OtelSpanHandler::handle(std::vector<std::unique_ptr<ApplicationBatchSpan>>& spans) {
    if (spans.empty()) return;

    for (const auto& span : spans) {
        std::shared_ptr<SourceBuffer> sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        for (const auto& x : span->single_spans_) {
            auto spanEvent = eventGroup.AddSpanEvent();
            for (const auto& tag : x->tags_) {
                spanEvent->SetTag(tag.first, tag.second);
            }
            spanEvent->SetName(x->span_name_);
            spanEvent->SetKind(static_cast<SpanEvent::Kind>(x->span_kind_));
            spanEvent->SetStartTimeNs(x->start_timestamp_);
            spanEvent->SetEndTimeNs(x->end_timestamp_);
            spanEvent->SetTraceId(x->trace_id_);
            spanEvent->SetSpanId(x->span_id_);
            mProcessTotalCnt++;
        }
#ifdef APSARA_UNIT_TEST_MAIN
        continue;
#endif
        std::unique_ptr<ProcessQueueItem> item = std::make_unique<ProcessQueueItem>(std::move(eventGroup), mPluginIdx);
        if (ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
            LOG_WARNING(sLogger, ("configName", mCtx->GetConfigName())("pluginIdx",mPluginIdx)("[Span] push queue failed!", ""));
        }
        
    }

    return;
}

void EventHandler::handle(std::vector<std::unique_ptr<ApplicationBatchEvent>>& events) {
    if (events.empty()) return;

    for (const auto& appEvents : events) {
        if (!appEvents || appEvents->events_.empty()) continue;
        std::shared_ptr<SourceBuffer> sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        for (const auto& event : appEvents->events_) {
            if (!event || event->GetAllTags().empty()) continue;
            auto logEvent = eventGroup.AddLogEvent();
            for (const auto& tag : event->GetAllTags()) {
                logEvent->SetContent(tag.first, tag.second);
                auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::nanoseconds(event->GetTimestamp()));
                logEvent->SetTimestamp(seconds.count(), event->GetTimestamp() - seconds.count() * 1e9);
            }
            mProcessTotalCnt ++;
        }
        for (const auto& tag : appEvents->tags_) {
            eventGroup.SetTag(tag.first, tag.second);
        }
#ifdef APSARA_UNIT_TEST_MAIN
        continue;
#endif
        std::unique_ptr<ProcessQueueItem> item = std::make_unique<ProcessQueueItem>(std::move(eventGroup), mPluginIdx);
        if (ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
            LOG_WARNING(sLogger, ("configName", mCtx->GetConfigName())("pluginIdx",mPluginIdx)("[Event] push queue failed!", ""));
        }
    }
}

#ifdef __ENTERPRISE__

const static std::string app_id_key = "arms.appId";
const static std::string ip_key = "ip";

const static std::string rpc_request_total_count = "arms_rpc_requests_count";
const static std::string rpc_request_slow_count = "arms_rpc_requests_slow_count";
const static std::string rpc_request_err_count = "arms_rpc_requests_error_count";
const static std::string rpc_request_rt = "arms_rpc_requests_seconds";
const static std::string rpc_request_status_count = "arms_rpc_requests_by_status_count";
static const std::string status_2xx_key = "2xx";
static const std::string status_3xx_key = "2xx";
static const std::string status_4xx_key = "2xx";
static const std::string status_5xx_key = "2xx";

// FOR APP METRICS
GENERATE_METRICS(GenerateRequestsTotalMetrics, MeasureType::MEASURE_TYPE_APP, AppSingleMeasure, rpc_request_total_count, request_total_)
GENERATE_METRICS(GenerateRequestsSlowMetrics, MeasureType::MEASURE_TYPE_APP, AppSingleMeasure, rpc_request_slow_count, slow_total_)
GENERATE_METRICS(GenerateRequestsErrorMetrics, MeasureType::MEASURE_TYPE_APP, AppSingleMeasure, rpc_request_err_count, error_total_)
GENERATE_METRICS(GenerateRequestsDurationSumMetrics, MeasureType::MEASURE_TYPE_APP, AppSingleMeasure, rpc_request_status_count, duration_ms_sum_)

void GenerateRequestsStatusMetrics(PipelineEventGroup& group, std::unique_ptr<Measure>& measure, uint64_t ts) {
    if (measure->type_ != MeasureType::MEASURE_TYPE_APP) return;
    auto inner = static_cast<AppSingleMeasure*>(measure->inner_measure_.get());
    ADD_STATUS_METRICS(rpc_request_status_count, status_2xx_count_, status_2xx_key);
    ADD_STATUS_METRICS(rpc_request_status_count, status_3xx_count_, status_3xx_key);
    ADD_STATUS_METRICS(rpc_request_status_count, status_4xx_count_, status_4xx_key);
    ADD_STATUS_METRICS(rpc_request_status_count, status_5xx_count_, status_5xx_key);
}

const static std::string npm_tcp_drop_total = "arms_npm_tcp_drop_count";
const static std::string npm_tcp_retrans_total = "arms_npm_tcp_retrans_total";
const static std::string npm_tcp_count_total = "arms_npm_tcp_count_by_state";
const static std::string npm_recv_pkt_total = "arms_npm_recv_packets_total";
const static std::string npm_recv_byte_total = "arms_npm_recv_bytes_total";
const static std::string npm_send_pkt_total = "arms_npm_sent_packets_total";
const static std::string npm_send_byte_total = "arms_npm_sent_bytes_total";

// FOR NET METRICS
GENERATE_METRICS(GenerateTcpDropTotalMetrics, MeasureType::MEASURE_TYPE_NET, NetSingleMeasure, npm_tcp_drop_total, tcp_drop_total_)
GENERATE_METRICS(GenerateTcpRetransTotalMetrics, MeasureType::MEASURE_TYPE_NET, NetSingleMeasure, npm_tcp_retrans_total, tcp_retran_total_)
GENERATE_METRICS(GenerateTcpConnectionTotalMetrics, MeasureType::MEASURE_TYPE_NET, NetSingleMeasure, npm_tcp_count_total, tcp_connect_total_)
GENERATE_METRICS(GenerateTcpRecvPktsTotalMetrics, MeasureType::MEASURE_TYPE_NET, NetSingleMeasure, npm_recv_pkt_total, recv_pkt_total_)
GENERATE_METRICS(GenerateTcpRecvBytesTotalMetrics, MeasureType::MEASURE_TYPE_NET, NetSingleMeasure, npm_recv_byte_total, recv_byte_total_)
GENERATE_METRICS(GenerateTcpSendPktsTotalMetrics, MeasureType::MEASURE_TYPE_NET, NetSingleMeasure, npm_send_pkt_total, send_pkt_total_)
GENERATE_METRICS(GenerateTcpSendBytesTotalMetrics, MeasureType::MEASURE_TYPE_NET, NetSingleMeasure, npm_send_byte_total, send_byte_total_)

void ArmsSpanHandler::handle(std::vector<std::unique_ptr<ApplicationBatchSpan>>& spans) {
    if (spans.empty()) return;

    for (const auto& span : spans) {
        std::shared_ptr<SourceBuffer> sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        eventGroup.SetTag(app_id_key, span->app_id_);
        for (const auto& x : span->single_spans_) {
            auto spanEvent = eventGroup.AddSpanEvent();
            for (const auto& tag : x->tags_) {
                spanEvent->SetTag(tag.first, tag.second);
            }
            spanEvent->SetName(x->span_name_);
            spanEvent->SetKind(static_cast<SpanEvent::Kind>(x->span_kind_));
            spanEvent->SetStartTimeNs(x->start_timestamp_);
            spanEvent->SetEndTimeNs(x->end_timestamp_);
            spanEvent->SetTraceId(x->trace_id_);
            spanEvent->SetSpanId(x->span_id_);
            mProcessTotalCnt++;
        }
#ifdef APSARA_UNIT_TEST_MAIN
        continue;
#endif
        std::unique_ptr<ProcessQueueItem> item = std::make_unique<ProcessQueueItem>(std::move(eventGroup), mPluginIdx);
        if (ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
            LOG_WARNING(sLogger, ("configName", mCtx->GetConfigName())("pluginIdx",mPluginIdx)("[Span] push queue failed!", ""));
        }
    }

    return;
}

void ArmsMeterHandler::handle(std::vector<std::unique_ptr<ApplicationBatchMeasure>>& measures, uint64_t timestamp) {
    if (measures.empty()) return;

    for (const auto& appBatchMeasures : measures) {
        std::shared_ptr<SourceBuffer> sourceBuffer = std::make_shared<SourceBuffer>();;
        PipelineEventGroup eventGroup(sourceBuffer);
        
        // source_ip
        eventGroup.SetTag(std::string(app_id_key), appBatchMeasures->app_id_);
        eventGroup.SetTag(std::string(ip_key), appBatchMeasures->ip_);
        for (const auto& measure : appBatchMeasures->measures_) {
            auto type = measure->type_;
            if (type == MeasureType::MEASURE_TYPE_APP) {
                GenerateRequestsTotalMetrics(eventGroup, measure, timestamp);
                GenerateRequestsSlowMetrics(eventGroup, measure, timestamp);
                GenerateRequestsErrorMetrics(eventGroup, measure, timestamp);
                GenerateRequestsDurationSumMetrics(eventGroup, measure, timestamp);
                GenerateRequestsStatusMetrics(eventGroup, measure, timestamp);
            } else if (type == MeasureType::MEASURE_TYPE_NET) {
                GenerateTcpDropTotalMetrics(eventGroup, measure, timestamp);
                GenerateTcpRetransTotalMetrics(eventGroup, measure, timestamp);
                GenerateTcpConnectionTotalMetrics(eventGroup, measure, timestamp);
                GenerateTcpRecvPktsTotalMetrics(eventGroup, measure, timestamp);
                GenerateTcpRecvBytesTotalMetrics(eventGroup, measure, timestamp);
                GenerateTcpSendPktsTotalMetrics(eventGroup, measure, timestamp);
                GenerateTcpSendBytesTotalMetrics(eventGroup, measure, timestamp);
            }
            mProcessTotalCnt++;
        }
        
#ifdef APSARA_UNIT_TEST_MAIN
        continue;
#endif
        std::unique_ptr<ProcessQueueItem> item = std::make_unique<ProcessQueueItem>(std::move(eventGroup), mPluginIdx);
        if (ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
            LOG_WARNING(sLogger, ("configName", mCtx->GetConfigName())("pluginIdx",mPluginIdx)("[Metrics] push queue failed!", ""));
        }

    }
    return;
}

#endif

}
}

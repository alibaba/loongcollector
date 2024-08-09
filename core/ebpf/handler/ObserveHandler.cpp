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
#include "queue/ProcessQueueManager.h"
#include "queue/ProcessQueueItem.h"

namespace logtail {
namespace ebpf {

#define ADD_STATUS_METRICS(METRIC_NAME, FIELD_NAME, VALUE) \
    {if (!inner->FIELD_NAME) return; \
    auto event = group.AddMetricEvent(); \
    for (auto& tag : measure->tags_) { \
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
    for (auto& tag : measure->tags_) { \
        event->SetTag(tag.first, tag.second); \
    } \
    event->SetName(METRIC_NAME); \
    event->SetTimestamp(ts); \
    event->SetValue(UntypedSingleValue{(double)inner->FIELD_NAME}); \
}

void OtelMeterHandler::handle(std::vector<std::unique_ptr<ApplicationBatchMeasure>>&& measures, uint64_t timestamp) {
    if (measures.empty()) return;

    for (auto& app_batch_measures : measures) {
        PipelineEventGroup event_group(std::make_shared<SourceBuffer>());
        for (auto& measure : app_batch_measures->measures_) {
            auto type = measure->type_;
            if (type == MeasureType::MEASURE_TYPE_APP) {
                auto inner = static_cast<AppSingleMeasure*>(measure->inner_measure_.get());
                auto event = event_group.AddMetricEvent();
                for (auto& tag : measure->tags_) {
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
        std::unique_ptr<ProcessQueueItem> item = std::make_unique<ProcessQueueItem>(std::move(event_group), mPluginIdx);
        if (ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
            LOG_WARNING(sLogger, ("configName", mCtx->GetConfigName())("pluginIdx",mPluginIdx)("[Otel Metrics] push queue failed!", ""));
        }
        
    }
    return;
}

void OtelSpanHandler::handle(std::vector<std::unique_ptr<ApplicationBatchSpan>>&& spans) {
    if (spans.empty()) return;

    std::shared_ptr<SourceBuffer> source_buffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup event_group(source_buffer);

    for (auto& span : spans) {
        for (auto& x : span->single_spans_) {
            auto spanEvent = event_group.AddSpanEvent();
            for (auto& tag : x->tags_) {
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
        std::unique_ptr<ProcessQueueItem> item = std::make_unique<ProcessQueueItem>(std::move(event_group), mPluginIdx);
        if (ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
            LOG_WARNING(sLogger, ("configName", mCtx->GetConfigName())("pluginIdx",mPluginIdx)("[Span] push queue failed!", ""));
        }
        
    }

    return;
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

void ArmsSpanHandler::handle(std::vector<std::unique_ptr<ApplicationBatchSpan>>&& spans) {
    if (spans.empty()) return;

    for (auto& span : spans) {
        std::shared_ptr<SourceBuffer> source_buffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup event_group(source_buffer);
        event_group.SetTag(app_id_key, span->app_id_);
        for (auto& x : span->single_spans_) {
            auto spanEvent = event_group.AddSpanEvent();
            for (auto& tag : x->tags_) {
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
        std::unique_ptr<ProcessQueueItem> item = std::make_unique<ProcessQueueItem>(std::move(event_group), mPluginIdx);
        if (ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
            LOG_WARNING(sLogger, ("configName", mCtx->GetConfigName())("pluginIdx",mPluginIdx)("[Span] push queue failed!", ""));
        }
    }

    return;
}

void ArmsMeterHandler::handle(std::vector<std::unique_ptr<ApplicationBatchMeasure>>&& measures, uint64_t timestamp) {
    if (measures.empty()) return;

    for (auto& app_batch_measures : measures) {
        std::shared_ptr<SourceBuffer> source_buffer = std::make_shared<SourceBuffer>();;
        PipelineEventGroup event_group(source_buffer);
        
        // source_ip
        event_group.SetTag(std::string(app_id_key), app_batch_measures->app_id_);
        event_group.SetTag(std::string(ip_key), app_batch_measures->ip_);
        for (auto& measure : app_batch_measures->measures_) {
            auto type = measure->type_;
            if (type == MeasureType::MEASURE_TYPE_APP) {
                GenerateRequestsTotalMetrics(event_group, measure, timestamp);
                GenerateRequestsSlowMetrics(event_group, measure, timestamp);
                GenerateRequestsErrorMetrics(event_group, measure, timestamp);
                GenerateRequestsDurationSumMetrics(event_group, measure, timestamp);
                GenerateRequestsStatusMetrics(event_group, measure, timestamp);
                
            } else if (type == MeasureType::MEASURE_TYPE_NET) {
                GenerateTcpDropTotalMetrics(event_group, measure, timestamp);
                GenerateTcpRetransTotalMetrics(event_group, measure, timestamp);
                GenerateTcpConnectionTotalMetrics(event_group, measure, timestamp);
                GenerateTcpRecvPktsTotalMetrics(event_group, measure, timestamp);
                GenerateTcpRecvBytesTotalMetrics(event_group, measure, timestamp);
                GenerateTcpSendPktsTotalMetrics(event_group, measure, timestamp);
                GenerateTcpSendBytesTotalMetrics(event_group, measure, timestamp);
            }
            mProcessTotalCnt++;
        }
        
#ifdef APSARA_UNIT_TEST_MAIN
        continue;
#endif
        std::unique_ptr<ProcessQueueItem> item = std::make_unique<ProcessQueueItem>(std::move(event_group), mPluginIdx);
        if (ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
            LOG_WARNING(sLogger, ("configName", mCtx->GetConfigName())("pluginIdx",mPluginIdx)("[Metrics] push queue failed!", ""));
        }

    }
    return;
}

#endif

}
}

// Copyright 2025 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "runner/sink/grpc/GrpcSink.h"

#include "app_config/AppConfig.h"
#include "collection_pipeline/queue/SenderQueueItem.h"
#include "common/Flags.h"
#include "logger/Logger.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "plugin/flusher/opentelemetry/FlusherOTLPNative.h"
#ifdef APSARA_UNIT_TEST_MAIN
#include "unittest/pipeline/GrpcSinkMock.h"
#endif

DEFINE_FLAG_INT32(grpc_sink_exit_timeout_sec, "grpc sink exit timeout, seconds", 5);

using namespace std;

namespace logtail {

GrpcSink* GrpcSink::GetInstance() {
#ifndef APSARA_UNIT_TEST_MAIN
    static GrpcSink instance;
    return &instance;
#else
    return GrpcSinkMock::GetInstance();
#endif
}

bool GrpcSink::Init() {
    WriteMetrics::GetInstance()->CreateMetricsRecordRef(
        mMetricsRecordRef,
        MetricCategory::METRIC_CATEGORY_RUNNER,
        {{METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_GRPC_SINK}});
    mInItemsTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_IN_ITEMS_TOTAL);
    mLastRunTime = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_LAST_RUN_TIME);
    mOutSuccessfulItemsTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_SINK_OUT_SUCCESSFUL_ITEMS_TOTAL);
    mOutFailedItemsTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_SINK_OUT_FAILED_ITEMS_TOTAL);
    mSendingItemsTotal = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_SINK_SENDING_ITEMS_TOTAL);
    WriteMetrics::GetInstance()->CommitMetricsRecordRef(mMetricsRecordRef);

    mThreadRes = async(launch::async, &GrpcSink::Run, this);
    return true;
}

void GrpcSink::Stop() {
    mIsFlush.store(true);
    mCV.notify_all();
    if (!mThreadRes.valid()) {
        return;
    }
    future_status s = mThreadRes.wait_for(chrono::seconds(INT32_FLAG(grpc_sink_exit_timeout_sec)));
    if (s == future_status::ready) {
        LOG_INFO(sLogger, ("grpc sink", "stopped successfully"));
    } else {
        LOG_WARNING(sLogger, ("grpc sink", "forced to stopped"));
    }
}

void GrpcSink::AddRequest(unique_ptr<OTLPGrpcCallContext>&& ctx) {
    {
        lock_guard<mutex> lock(mPendingMutex);
        mPendingRequests.push_back(std::move(ctx));
    }
    mCV.notify_one();
}

void GrpcSink::Run() {
    LOG_INFO(sLogger, ("grpc sink", "started"));
    while (true) {
        SET_GAUGE(mLastRunTime,
                  chrono::duration_cast<chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count());

        DispatchRequests();

        if (mIsFlush.load()) {
            unique_lock<mutex> lock(mPendingMutex);
            if (mPendingRequests.empty() && mSendingCnt.load() == 0) {
                break;
            }
            mCV.wait_for(lock, chrono::milliseconds(100));
        } else {
            unique_lock<mutex> lock(mPendingMutex);
            if (mPendingRequests.empty()) {
                mCV.wait_for(lock, chrono::milliseconds(500));
            }
        }
    }
}

void GrpcSink::DispatchRequests() {
    vector<unique_ptr<OTLPGrpcCallContext>> items;
    {
        lock_guard<mutex> lock(mPendingMutex);
        if (mPendingRequests.empty()) {
            return;
        }
        items = std::move(mPendingRequests);
    }

    int32_t maxConcurrency = AppConfig::GetInstance()->GetSendRequestGlobalConcurrency();
    for (auto& ctx : items) {
        while (mSendingCnt.load() >= maxConcurrency) {
            this_thread::sleep_for(chrono::milliseconds(10));
        }

        ADD_COUNTER(mInItemsTotal, 1);
        ADD_GAUGE(mSendingItemsTotal, 1);
        mSendingCnt.fetch_add(1);

        auto* rawCtx = ctx.release();
        auto* flusher = static_cast<FlusherOTLPNative*>(rawCtx->item->mFlusher);

        auto callback = [rawCtx, flusher, this](grpc::Status status) {
            auto pipelinePlaceholder = rawCtx->item->mPipeline;
            // Decrement GrpcSink's in-flight counter
            mSendingCnt.fetch_sub(1);
            ADD_GAUGE(mSendingItemsTotal, -1);
            // Let the flusher handle metrics + OnSendDone + ctx cleanup
            flusher->HandleGrpcCallback(std::move(status), rawCtx);
        };

        rawCtx->context = make_unique<grpc::ClientContext>();
        rawCtx->context->set_deadline(chrono::system_clock::now() + chrono::milliseconds(flusher->GetTimeoutMs()));
        for (const auto& [k, v] : flusher->GetHeaders()) {
            rawCtx->context->AddMetadata(k, v);
        }

        flusher->IncInFlight();

        switch (rawCtx->type) {
            case OTLPGrpcCallContext::DataType::Logs:
                flusher->GetLogsStub()->async()->Export(
                    rawCtx->context.get(), rawCtx->logsReq.get(), rawCtx->logsResp.get(), std::move(callback));
                break;
            case OTLPGrpcCallContext::DataType::Metrics:
                flusher->GetMetricsStub()->async()->Export(
                    rawCtx->context.get(), rawCtx->metricsReq.get(), rawCtx->metricsResp.get(), std::move(callback));
                break;
            case OTLPGrpcCallContext::DataType::Traces:
                flusher->GetTraceStub()->async()->Export(
                    rawCtx->context.get(), rawCtx->traceReq.get(), rawCtx->traceResp.get(), std::move(callback));
                break;
        }
    }
}

} // namespace logtail

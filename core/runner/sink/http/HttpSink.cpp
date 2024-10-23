// Copyright 2024 iLogtail Authors
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

#include "runner/sink/http/HttpSink.h"

#include "app_config/AppConfig.h"
#include "common/StringTools.h"
#include "common/http/Curl.h"
#include "logger/Logger.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "pipeline/plugin/interface/HttpFlusher.h"
#include "pipeline/queue/QueueKeyManager.h"
#include "pipeline/queue/SenderQueueItem.h"
#include "runner/FlusherRunner.h"

using namespace std;

namespace logtail {

bool HttpSink::Init() {
    mClient = curl_multi_init();
    if (mClient == nullptr) {
        LOG_ERROR(sLogger, ("failed to init http sink", "failed to init curl multi client"));
        return false;
    }

    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
        mMetricsRecordRef,
        {{METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_HTTP_SINK},
         {METRIC_LABEL_KEY_METRIC_CATEGORY, METRIC_LABEL_KEY_METRIC_CATEGORY_RUNNER}});
    mInItemsTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_IN_ITEMS_TOTAL);
    mLastRunTime = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_LAST_RUN_TIME);
    mOutSuccessfulItemsTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_SINK_OUT_SUCCESSFUL_ITEMS_TOTAL);
    mOutFailedItemsTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_SINK_OUT_FAILED_ITEMS_TOTAL);
    mSuccessfulItemTotalResponseTimeMs = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_SINK_SUCCESSFUL_ITEM_TOTAL_RESPONSE_TIME_MS);
    mFailedItemTotalResponseTimeMs = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_SINK_FAILED_ITEM_TOTAL_RESPONSE_TIME_MS);
    mSendingItemsTotal = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_SINK_SENDING_ITEMS_TOTAL);
    mSendConcurrency = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_SINK_SEND_CONCURRENCY);

    // TODO: should be dynamic
    mSendConcurrency->Set(AppConfig::GetInstance()->GetSendRequestConcurrency());

    mThreadRes = async(launch::async, &HttpSink::Run, this);
    return true;
}

void HttpSink::Stop() {
    mIsFlush = true;
    future_status s = mThreadRes.wait_for(chrono::seconds(1));
    if (s == future_status::ready) {
        LOG_INFO(sLogger, ("http sink", "stopped successfully"));
    } else {
        LOG_WARNING(sLogger, ("http sink", "forced to stopped"));
    }
}

void HttpSink::Run() {
    LOG_INFO(sLogger, ("http sink", "started"));
    while (true) {
        mLastRunTime->Set(
            chrono::duration_cast<chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count());
        unique_ptr<HttpSinkRequest> request;
        if (mQueue.WaitAndPop(request, 500)) {
            mInItemsTotal->Add(1);
            LOG_DEBUG(
                sLogger,
                ("got item from flusher runner, item address", request->mItem)(
                    "config-flusher-dst", QueueKeyManager::GetInstance()->GetName(request->mItem->mQueueKey))(
                    "wait time", ToString(time(nullptr) - request->mEnqueTime))("try cnt", ToString(request->mTryCnt)));
            if (!AddRequestToClient(std::move(request))) {
                continue;
            }
            mSendingItemsTotal->Add(1);
        } else if (mIsFlush && mQueue.Empty()) {
            break;
        } else {
            continue;
        }
        DoRun();
    }
    auto mc = curl_multi_cleanup(mClient);
    if (mc != CURLM_OK) {
        LOG_ERROR(sLogger, ("failed to cleanup curl multi handle", "exit anyway")("errMsg", curl_multi_strerror(mc)));
    }
}

bool HttpSink::AddRequestToClient(unique_ptr<HttpSinkRequest>&& request) {
    curl_slist* headers = nullptr;
    CURL* curl = CreateCurlHandler(request->mMethod,
                                   request->mHTTPSFlag,
                                   request->mHost,
                                   request->mPort,
                                   request->mUrl,
                                   request->mQueryString,
                                   request->mHeader,
                                   request->mBody,
                                   request->mResponse,
                                   headers,
                                   request->mTimeout,
                                   AppConfig::GetInstance()->IsHostIPReplacePolicyEnabled(),
                                   AppConfig::GetInstance()->GetBindInterface());
    if (curl == nullptr) {
        request->mItem->mStatus = SendingStatus::IDLE;
        FlusherRunner::GetInstance()->DecreaseHttpSendingCnt();
        mOutFailedItemsTotal->Add(1);
        LOG_ERROR(sLogger,
                  ("failed to send request", "failed to init curl handler")(
                      "action", "put sender queue item back to sender queue")("item address", request->mItem)(
                      "config-flusher-dst", QueueKeyManager::GetInstance()->GetName(request->mItem->mQueueKey)));
        return false;
    }

    request->mPrivateData = headers;
    curl_easy_setopt(curl, CURLOPT_PRIVATE, request.get());
    request->mLastSendTime = chrono::system_clock::now();

    auto res = curl_multi_add_handle(mClient, curl);
    if (res != CURLM_OK) {
        request->mItem->mStatus = SendingStatus::IDLE;
        FlusherRunner::GetInstance()->DecreaseHttpSendingCnt();
        curl_easy_cleanup(curl);
        mOutFailedItemsTotal->Add(1);
        LOG_ERROR(sLogger,
                  ("failed to send request",
                   "failed to add the easy curl handle to multi_handle")("errMsg", curl_multi_strerror(res))(
                      "action", "put sender queue item back to sender queue")("item address", request->mItem)(
                      "config-flusher-dst", QueueKeyManager::GetInstance()->GetName(request->mItem->mQueueKey)));
        return false;
    }
    // let sink destruct the request
    request.release();
    return true;
}

void HttpSink::DoRun() {
    CURLMcode mc;
    int runningHandlers = 1;
    while (runningHandlers) {
        auto curTime = chrono::system_clock::now();
        mLastRunTime->Set(chrono::duration_cast<chrono::seconds>(curTime.time_since_epoch()).count());
        if ((mc = curl_multi_perform(mClient, &runningHandlers)) != CURLM_OK) {
            LOG_ERROR(
                sLogger,
                ("failed to call curl_multi_perform", "sleep 100ms and retry")("errMsg", curl_multi_strerror(mc)));
            this_thread::sleep_for(chrono::milliseconds(100));
            continue;
        }
        HandleCompletedRequests(runningHandlers);

        unique_ptr<HttpSinkRequest> request;
        if (mQueue.TryPop(request)) {
            mInItemsTotal->Add(1);
            LOG_DEBUG(
                sLogger,
                ("got item from flusher runner, item address", request->mItem)(
                    "config-flusher-dst", QueueKeyManager::GetInstance()->GetName(request->mItem->mQueueKey))(
                    "wait time", ToString(time(nullptr) - request->mEnqueTime))("try cnt", ToString(request->mTryCnt)));
            if (AddRequestToClient(std::move(request))) {
                ++runningHandlers;
                mSendingItemsTotal->Add(1);
            }
        }

        struct timeval timeout {
            1, 0
        };
        long curlTimeout = -1;
        if ((mc = curl_multi_timeout(mClient, &curlTimeout)) != CURLM_OK) {
            LOG_WARNING(
                sLogger,
                ("failed to call curl_multi_timeout", "use default timeout 1s")("errMsg", curl_multi_strerror(mc)));
        }
        if (curlTimeout >= 0) {
            auto sec = curlTimeout / 1000;
            // to avoid waiting too long so that adding new request is delayed
            if (sec <= 1) {
                timeout.tv_sec = sec;
                timeout.tv_usec = (curlTimeout % 1000) * 1000;
            }
        }

        int maxfd = -1;
        fd_set fdread;
        fd_set fdwrite;
        fd_set fdexcep;
        FD_ZERO(&fdread);
        FD_ZERO(&fdwrite);
        FD_ZERO(&fdexcep);
        if ((mc = curl_multi_fdset(mClient, &fdread, &fdwrite, &fdexcep, &maxfd)) != CURLM_OK) {
            LOG_ERROR(sLogger, ("failed to call curl_multi_fdset", "sleep 100ms")("errMsg", curl_multi_strerror(mc)));
        }
        if (maxfd == -1) {
            // sleep min(timeout, 100ms) according to libcurl
            int64_t sleepMs = (curlTimeout >= 0 && curlTimeout < 100) ? curlTimeout : 100;
            this_thread::sleep_for(chrono::milliseconds(sleepMs));
        } else {
            select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
        }
    }
}

void HttpSink::HandleCompletedRequests(int& runningHandlers) {
    int msgsLeft = 0;
    CURLMsg* msg = curl_multi_info_read(mClient, &msgsLeft);
    while (msg) {
        if (msg->msg == CURLMSG_DONE) {
            bool requestReused = false;
            CURL* handler = msg->easy_handle;
            HttpSinkRequest* request = nullptr;
            curl_easy_getinfo(handler, CURLINFO_PRIVATE, &request);
            auto responseTime
                = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - request->mLastSendTime)
                      .count();
            LOG_DEBUG(sLogger,
                      ("send http request completed, item address", request->mItem)(
                          "config-flusher-dst", QueueKeyManager::GetInstance()->GetName(request->mItem->mQueueKey))(
                          "response time", ToString(responseTime) + "ms")("try cnt", ToString(request->mTryCnt)));
            switch (msg->data.result) {
                case CURLE_OK: {
                    long statusCode = 0;
                    curl_easy_getinfo(handler, CURLINFO_RESPONSE_CODE, &statusCode);
                    request->mResponse.mStatusCode = (int32_t)statusCode;
                    static_cast<HttpFlusher*>(request->mItem->mFlusher)->OnSendDone(request->mResponse, request->mItem);
                    FlusherRunner::GetInstance()->DecreaseHttpSendingCnt();
                    mOutSuccessfulItemsTotal->Add(1);
                    mSuccessfulItemTotalResponseTimeMs->Add(responseTime);
                    mSendingItemsTotal->Sub(1);
                    break;
                }
                default:
                    // considered as network error
                    if (++request->mTryCnt <= request->mMaxTryCnt) {
                        LOG_WARNING(
                            sLogger,
                            ("failed to send request", "retry immediately")("item address", request->mItem)(
                                "try cnt", request->mTryCnt)("errMsg", curl_easy_strerror(msg->data.result))(
                                "config-flusher-dst",
                                QueueKeyManager::GetInstance()->GetName(request->mItem->mFlusher->GetQueueKey())));
                        // free first，becase mPrivateData will be reset in AddRequestToClient
                        if (request->mPrivateData) {
                            curl_slist_free_all((curl_slist*)request->mPrivateData);
                            request->mPrivateData = nullptr;
                        }
                        AddRequestToClient(unique_ptr<HttpSinkRequest>(request));
                        ++runningHandlers;
                        requestReused = true;
                    } else {
                        static_cast<HttpFlusher*>(request->mItem->mFlusher)
                            ->OnSendDone(request->mResponse, request->mItem);
                        FlusherRunner::GetInstance()->DecreaseHttpSendingCnt();
                    }
                    mOutFailedItemsTotal->Add(1);
                    mFailedItemTotalResponseTimeMs->Add(responseTime);
                    mSendingItemsTotal->Sub(1);
                    break;
            }
            curl_multi_remove_handle(mClient, handler);
            curl_easy_cleanup(handler);
            if (!requestReused) {
                if (request->mPrivateData) {
                    curl_slist_free_all((curl_slist*)request->mPrivateData);
                }
                delete request;
            }
        }
        msg = curl_multi_info_read(mClient, &msgsLeft);
    }
}

} // namespace logtail

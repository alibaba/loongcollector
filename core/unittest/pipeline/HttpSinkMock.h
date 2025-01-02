/*
 * Copyright 2024 iLogtail Authors
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

#pragma once

#include "logger/Logger.h"
#include "pipeline/plugin/interface/HttpFlusher.h"
#include "pipeline/queue/SLSSenderQueueItem.h"
#include "plugin/flusher/sls/FlusherSLS.h"
#include "plugin/flusher/sls/SLSConstant.h"
#include "runner/FlusherRunner.h"
#include "runner/sink/http/HttpSink.h"

namespace logtail {
class HttpSinkMock : public HttpSink {
public:
    HttpSinkMock(const HttpSinkMock&) = delete;
    HttpSinkMock& operator=(const HttpSinkMock&) = delete;

    static HttpSinkMock* GetInstance() {
        static HttpSinkMock instance;
        return &instance;
    }

    bool Init() override {
        mThreadRes = async(std::launch::async, &HttpSinkMock::Run, this);
        mIsFlush = false;
        return true;
    }

    void Stop() override {
        mIsFlush = true;
        if (!mThreadRes.valid()) {
            return;
        }
        std::future_status s = mThreadRes.wait_for(std::chrono::seconds(1));
        if (s == std::future_status::ready) {
            LOG_INFO(sLogger, ("http sink mock", "stopped successfully"));
        } else {
            LOG_WARNING(sLogger, ("http sink mock", "forced to stopped"));
        }
        ClearRequests();
    }

    void Run() {
        LOG_INFO(sLogger, ("http sink mock", "started"));
        while (true) {
            std::unique_ptr<HttpSinkRequest> request;
            if (mQueue.WaitAndPop(request, 500)) {
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    mRequests.push_back(*(request->mItem));
                }
                request->mResponse.SetNetworkStatus(NetworkCode::Ok, "");
                request->mResponse.SetStatusCode(200);
                request->mResponse.SetResponseTime(std::chrono::milliseconds(10));
                // for sls only
                request->mResponse.mHeader[X_LOG_REQUEST_ID] = "request_id";
                static_cast<HttpFlusher*>(request->mItem->mFlusher)->OnSendDone(request->mResponse, request->mItem);
                FlusherRunner::GetInstance()->DecreaseHttpSendingCnt();
                request.reset();
            } else if (mIsFlush && mQueue.Empty()) {
                break;
            } else {
                continue;
            }
        }
    }

    std::vector<SenderQueueItem>& GetRequests() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mRequests;
    }

    void ClearRequests() {
        std::lock_guard<std::mutex> lock(mMutex);
        mRequests.clear();
    }

private:
    HttpSinkMock() = default;
    ~HttpSinkMock() = default;

    std::atomic_bool mIsFlush = false;
    mutable std::mutex mMutex;
    std::vector<SenderQueueItem> mRequests;
};

} // namespace logtail
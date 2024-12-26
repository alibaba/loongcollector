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
#include "runner/FlusherRunner.h"
#include "runner/sink/http/HttpSink.h"
#include "sdk/Common.h"

namespace logtail {
class HttpSinkMock : public HttpSink {
public:
    HttpSinkMock(const HttpSinkMock&) = delete;
    HttpSinkMock& operator=(const HttpSinkMock&) = delete;

    static HttpSinkMock* GetInstance() {
        static HttpSinkMock instance;
        return &instance;
    }

    bool Init() override { return true; }

    bool AddRequest(std::unique_ptr<HttpSinkRequest>&& request) {
        if (useRealHttpSink) {
            return HttpSink::GetInstance()->AddRequest(std::move(request));
        }
        {
            std::lock_guard<std::mutex> lock(mMutex);
            std::string logstore = "default";
            if (static_cast<HttpFlusher*>(request->mItem->mFlusher)->Name().find("sls") != std::string::npos) {
                auto flusher = static_cast<FlusherSLS*>(request->mItem->mFlusher);
                logstore = flusher->mLogstore;
            }
            LOG_DEBUG(sLogger, ("http sink mock", "add request")("logstore", logstore)("body", request->mBody));
            mRequests[logstore].push_back(request->mBody);
        }
        request->mResponse.SetStatusCode(200);
        request->mResponse.mHeader[sdk::X_LOG_REQUEST_ID] = "request_id";
        static_cast<SLSSenderQueueItem*>(request->mItem)->mExactlyOnceCheckpoint = nullptr;
        static_cast<HttpFlusher*>(request->mItem->mFlusher)->OnSendDone(request->mResponse, request->mItem);
        FlusherRunner::GetInstance()->DecreaseHttpSendingCnt();
        request.reset();
        return true;
    }

    std::vector<std::string> GetRequests(std::string logstore) {
        std::lock_guard<std::mutex> lock(mMutex);
        return mRequests[logstore];
    }

    void ClearRequests() {
        std::lock_guard<std::mutex> lock(mMutex);
        mRequests.clear();
    }

    void SetUseRealHttpSink(bool useReal) { useRealHttpSink = useReal; }

private:
    HttpSinkMock() = default;
    ~HttpSinkMock() = default;

    bool useRealHttpSink = false;

    std::atomic_bool mIsFlush = false;
    mutable std::mutex mMutex;
    std::unordered_map<std::string, std::vector<std::string>> mRequests;
};

} // namespace logtail
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

#include "common/http/AsynCurlRunner.h"

#include <chrono>

#include "common/StringTools.h"
#include "common/http/Curl.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail {

bool AsynCurlRunner::Init() {
    mClient = curl_multi_init();
    mIsFlush = false;
    if (mClient == nullptr) {
        LOG_ERROR(sLogger, ("failed to init async curl runner", "failed to init curl client"));
        return false;
    }
    mThreadRes = async(launch::async, &AsynCurlRunner::Run, this);
    return true;
}

void AsynCurlRunner::Stop() {
    mIsFlush = true;
    future_status s = mThreadRes.wait_for(chrono::seconds(1));
    if (s == future_status::ready) {
        LOG_INFO(sLogger, ("async curl runner", "stopped successfully"));
    } else {
        LOG_WARNING(sLogger, ("async curl runner", "forced to stopped"));
    }
}

bool AsynCurlRunner::AddRequest(unique_ptr<AsynHttpRequest>&& request) {
    mQueue.Push(std::move(request));
    return true;
}

void AsynCurlRunner::Run() {
    while (true) {
        unique_ptr<AsynHttpRequest> request;
        if (mQueue.WaitAndPop(request, 500)) {
            LOG_DEBUG(
                sLogger,
                ("got request from queue, request address", request.get())("try cnt", ToString(request->mTryCnt)));
            if (!AddRequestToMultiCurlHandler(mClient, std::move(request))) {
                continue;
            }
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

void AsynCurlRunner::DoRun() {
    CURLMcode mc;
    int runningHandlers = 1;
    while (runningHandlers) {
        if ((mc = curl_multi_perform(mClient, &runningHandlers)) != CURLM_OK) {
            LOG_ERROR(
                sLogger,
                ("failed to call curl_multi_perform", "sleep 100ms and retry")("errMsg", curl_multi_strerror(mc)));
            this_thread::sleep_for(chrono::milliseconds(100));
            continue;
        }
        HandleCompletedAsynRequests(mClient, runningHandlers);

        unique_ptr<AsynHttpRequest> request;
        if (mQueue.TryPop(request)) {
            LOG_DEBUG(sLogger,
                      ("got item from flusher runner, request address", request.get())("try cnt",
                                                                                       ToString(request->mTryCnt)));
            if (AddRequestToMultiCurlHandler(mClient, std::move(request))) {
                ++runningHandlers;
            }
        }

        struct timeval timeout{1, 0};
        long curlTimeout = -1;
        if ((mc = curl_multi_timeout(mClient, &curlTimeout)) != CURLM_OK) {
            LOG_WARNING(
                sLogger,
                ("failed to call curl_multi_timeout", "use default timeout 1s")("errMsg", curl_multi_strerror(mc)));
        }
        if (curlTimeout >= 0) {
            auto sec = curlTimeout / 1000;
            // special treatment, do not know why
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

} // namespace logtail

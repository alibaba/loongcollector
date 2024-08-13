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

#include <cstdint>
#include <map>
#include <string>

#include "common/http/HttpResponse.h"

namespace logtail {

static constexpr uint32_t sDefaultTimeoutSec = 15;
static constexpr uint32_t sDefaultMaxTryCnt = 3;

struct HttpRequest {
    std::string mMethod;
    // TODO: upgrade curl to 7.62, and replace the following 4 members
    // CURLU* mURL;
    bool mHTTPSFlag = false;
    std::string mUrl;
    std::string mQueryString;

    std::map<std::string, std::string> mHeader;
    std::string mBody;
    std::string mHost;
    int32_t mPort;
    uint32_t mTimeout = sDefaultTimeoutSec;
    uint32_t mMaxTryCnt = sDefaultMaxTryCnt;

    uint32_t mTryCnt = 1;
    time_t mLastSendTime = 0;

    HttpRequest(const std::string& method,
                bool httpsFlag,
                const std::string& host,
                int32_t port,
                const std::string& url,
                const std::string& query,
                const std::map<std::string, std::string>& header,
                const std::string& body,
                uint32_t timeout = sDefaultTimeoutSec,
                uint32_t maxTryCnt = sDefaultMaxTryCnt)
        : mMethod(method),
          mHTTPSFlag(httpsFlag),
          mUrl(url),
          mQueryString(query),
          mHeader(header),
          mBody(body),
          mHost(host),
          mPort(port),
          mTimeout(timeout),
          mMaxTryCnt(maxTryCnt) {}
    virtual ~HttpRequest() = default;
};

struct AsynHttpRequest : public HttpRequest {
    HttpResponse mResponse;
    void* mPrivateData = nullptr;
    time_t mEnqueTime = 0;

    AsynHttpRequest(const std::string& method,
                    bool httpsFlag,
                    const std::string& host,
                    int32_t port,
                    const std::string& url,
                    const std::string& query,
                    const std::map<std::string, std::string>& header,
                    const std::string& body,
                    uint32_t timeout = sDefaultTimeoutSec,
                    uint32_t maxTryCnt = sDefaultMaxTryCnt)
        : HttpRequest(method, httpsFlag, host, port, url, query, header, body, timeout, maxTryCnt) {}

    virtual bool IsContextValid() const = 0;
    virtual void OnSendDone(const HttpResponse& response) = 0;
};

} // namespace logtail

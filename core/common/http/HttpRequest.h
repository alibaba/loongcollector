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

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

#include "common/Flags.h"
#include "common/http/HttpResponse.h"

DECLARE_FLAG_INT32(default_http_request_timeout_secs);
DECLARE_FLAG_INT32(default_http_request_max_try_cnt);

namespace logtail {

struct CurlTLS {
    std::string mCaFile;
    std::string mCertFile;
    std::string mKeyFile;
    bool mInsecureSkipVerify = true;
};

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
    uint32_t mTimeout = static_cast<uint32_t>(INT32_FLAG(default_http_request_timeout_secs));
    uint32_t mMaxTryCnt = static_cast<uint32_t>(INT32_FLAG(default_http_request_max_try_cnt));
    bool mFollowRedirects = false;
    std::optional<CurlTLS> mTls = std::nullopt;

    uint32_t mTryCnt = 1;
    std::chrono::system_clock::time_point mLastSendTime;

    HttpRequest(const std::string& method,
                bool httpsFlag,
                const std::string& host,
                int32_t port,
                const std::string& url,
                const std::string& query,
                const std::map<std::string, std::string>& header,
                const std::string& body,
                uint32_t timeout = static_cast<uint32_t>(INT32_FLAG(default_http_request_timeout_secs)),
                uint32_t maxTryCnt = static_cast<uint32_t>(INT32_FLAG(default_http_request_max_try_cnt)),
                bool followRedirects = false,
                std::optional<CurlTLS> tls = std::nullopt)
        : mMethod(method),
          mHTTPSFlag(httpsFlag),
          mUrl(url),
          mQueryString(query),
          mHeader(header),
          mBody(body),
          mHost(host),
          mPort(port),
          mTimeout(timeout),
          mMaxTryCnt(maxTryCnt),
          mFollowRedirects(followRedirects),
          mTls(std::move(tls)) {}
    virtual ~HttpRequest() = default;
};

struct AsynHttpRequest : public HttpRequest {
    HttpResponse mResponse;
    void* mPrivateData = nullptr;
    std::chrono::system_clock::time_point mEnqueTime;

    AsynHttpRequest(const std::string& method,
                    bool httpsFlag,
                    const std::string& host,
                    int32_t port,
                    const std::string& url,
                    const std::string& query,
                    const std::map<std::string, std::string>& header,
                    const std::string& body,
                    HttpResponse&& response = HttpResponse(),
                    uint32_t timeout = static_cast<uint32_t>(INT32_FLAG(default_http_request_timeout_secs)),
                    uint32_t maxTryCnt = static_cast<uint32_t>(INT32_FLAG(default_http_request_max_try_cnt)),
                    bool followRedirects = false,
                    std::optional<CurlTLS> tls = std::nullopt)
        : HttpRequest(method,
                      httpsFlag,
                      host,
                      port,
                      url,
                      query,
                      header,
                      body,
                      timeout,
                      maxTryCnt,
                      followRedirects,
                      std::move(tls)),
          mResponse(std::move(response)) {}

    virtual bool IsContextValid() const = 0;
    virtual void OnSendDone(HttpResponse& response) = 0;
};

std::string GetQueryString(const std::map<std::string, std::string>& parameters);

} // namespace logtail

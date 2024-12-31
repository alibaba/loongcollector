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

#include <curl/curl.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "common/http/HttpRequest.h"
#include "common/http/HttpResponse.h"

namespace logtail {

NetworkCode GetNetworkStatus(CURLcode code);

CURL* CreateCurlHandler(const std::string& method,
                        bool httpsFlag,
                        const std::string& host,
                        int32_t port,
                        const std::string& url,
                        const std::string& queryString,
                        const std::map<std::string, std::string>& header,
                        const std::string& body,
                        HttpResponse& response,
                        curl_slist*& headers,
                        uint32_t timeout,
                        bool replaceHostWithIp = true,
                        const std::string& intf = "",
                        bool followRedirects = false,
                        std::optional<CurlTLS> tls = std::nullopt);

bool SendHttpRequest(std::unique_ptr<HttpRequest>&& request, HttpResponse& response);

bool AddRequestToMultiCurlHandler(CURLM* multiCurl, std::unique_ptr<AsynHttpRequest>&& request);
void SendAsynRequests(CURLM* multiCurl);
void HandleCompletedAsynRequests(CURLM* multiCurl, int& runningHandlers);

} // namespace logtail

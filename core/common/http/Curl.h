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
#include <memory>
#include <optional>
#include <string>

#include "curl/curl.h"

#include "common/http/HttpRequest.h"
#include "common/http/HttpResponse.h"

namespace logtail {

// Installs the OpenSSL locking / thread-id callbacks that OpenSSL versions
// prior to 1.1.0 require to be used safely from multiple threads. Without them,
// concurrent SSL traffic from different threads races on OpenSSL's global
// per-thread error-state hash table (ERR_get_state / ERR_remove_thread_state)
// and crashes the process with SIGSEGV.
//
// It is a no-op when linked against OpenSSL >= 1.1.0 (which is thread-safe
// internally), is safe to call multiple times, and never overrides a callback
// another component already installed. MUST be called once during process
// startup, before any thread performs SSL operations.
void SetupOpenSSLThreadSupport();

NetworkCode GetNetworkStatus(CURLcode code);

CURL* CreateCurlHandler(const std::string& method,
                        bool httpsFlag,
                        const std::string& endpoint,
                        int32_t port,
                        const std::string& url,
                        const std::string& queryString,
                        const std::map<std::string, std::string>& header,
                        const std::string& body,
                        HttpResponse& response,
                        curl_slist*& headers,
                        uint32_t timeout,
                        const std::string& intf = "",
                        bool followRedirects = false,
                        const std::optional<CurlTLS>& tls = std::nullopt,
                        const std::optional<CurlSocket>& socket = std::nullopt);

bool SendHttpRequest(std::unique_ptr<HttpRequest>&& request, HttpResponse& response);

bool AddRequestToMultiCurlHandler(CURLM* multiCurl, std::unique_ptr<AsynHttpRequest>&& request);
void SendAsynRequests(CURLM* multiCurl);
void HandleCompletedAsynRequests(CURLM* multiCurl, int& runningHandlers);

} // namespace logtail

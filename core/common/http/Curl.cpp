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

#include "common/http/Curl.h"

#include <cstdint>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>

#include <map>
#include <string>

#include "app_config/AppConfig.h"
#include "common/DNSCache.h"
#include "common/Flags.h"
#include "common/StringTools.h"
#include "common/http/HttpRequest.h"
#include "common/http/HttpResponse.h"
#include "logger/Logger.h"

DECLARE_FLAG_INT32(curl_ip_dscp);

using namespace std;

namespace logtail {

NetworkCode GetNetworkStatus(CURLcode code) {
    // please refer to https://curl.se/libcurl/c/libcurl-errors.html
    switch (code) {
        case CURLE_OK:
            return NetworkCode::Ok;
        case CURLE_COULDNT_CONNECT:
            return NetworkCode::ConnectionFailed;
        case CURLE_LOGIN_DENIED:
        case CURLE_REMOTE_ACCESS_DENIED:
            return NetworkCode::RemoteAccessDenied;
        case CURLE_OPERATION_TIMEDOUT:
            return NetworkCode::Timeout;
        case CURLE_SSL_CONNECT_ERROR:
            return NetworkCode::SSLConnectError;
        case CURLE_SSL_CERTPROBLEM:
        case CURLE_SSL_CACERT:
            return NetworkCode::SSLCertError;
        case CURLE_SEND_ERROR:
        case CURLE_SEND_FAIL_REWIND:
            return NetworkCode::SendDataFailed;
        case CURLE_RECV_ERROR:
            return NetworkCode::RecvDataFailed;
        case CURLE_SSL_PINNEDPUBKEYNOTMATCH:
        case CURLE_SSL_INVALIDCERTSTATUS:
        case CURLE_SSL_CACERT_BADFILE:
        case CURLE_SSL_CIPHER:
        case CURLE_SSL_ENGINE_NOTFOUND:
        case CURLE_SSL_ENGINE_SETFAILED:
        case CURLE_USE_SSL_FAILED:
        case CURLE_SSL_ENGINE_INITFAILED:
        case CURLE_SSL_CRL_BADFILE:
        case CURLE_SSL_ISSUER_ERROR:
        case CURLE_SSL_SHUTDOWN_FAILED:
            return NetworkCode::SSLOtherProblem;
        case CURLE_FAILED_INIT:
        default:
            return NetworkCode::Other;
    }
}

static size_t header_write_callback(char* buffer,
                                    size_t size,
                                    size_t nmemb,
                                    map<string, string, decltype(compareHeader)*>* write_buf) {
    unsigned long sizes = size * nmemb;

    if (buffer == NULL) {
        return 0;
    }
    unsigned long colonIndex;
    for (colonIndex = 1; colonIndex < sizes - 2; colonIndex++) {
        if (buffer[colonIndex] == ':')
            break;
    }
    if (colonIndex < sizes - 2) {
        unsigned long leftSpaceNum, rightSpaceNum;
        for (leftSpaceNum = 0; leftSpaceNum < colonIndex - 1; leftSpaceNum++) {
            if (buffer[colonIndex - leftSpaceNum - 1] != ' ')
                break;
        }
        for (rightSpaceNum = 0; rightSpaceNum < sizes - colonIndex - 1 - 2; rightSpaceNum++) {
            if (buffer[colonIndex + rightSpaceNum + 1] != ' ')
                break;
        }
        (*write_buf)[string(buffer, 0, colonIndex - leftSpaceNum)]
            = string(buffer, colonIndex + 1 + rightSpaceNum, sizes - colonIndex - 1 - 2 - rightSpaceNum);
    }
    return sizes;
}

static size_t socket_write_callback(void* socketData, curl_socket_t fd, curlsocktype purpose) {
    auto* socket = static_cast<CurlSocket*>(socketData);
    if (socket->mTOS.has_value()) {
        setsockopt(fd, IPPROTO_IP, IP_TOS, &socket->mTOS, sizeof(socket->mTOS));
    }
    return 0;
}

CURL* CreateCurlHandler(const string& method,
                        bool httpsFlag,
                        const string& host,
                        int32_t port,
                        const string& url,
                        const string& queryString,
                        const map<string, string>& header,
                        const string& body,
                        HttpResponse& response,
                        curl_slist*& headers,
                        uint32_t timeout,
                        bool replaceHostWithIp,
                        const string& intf,
                        bool followRedirects,
                        const optional<CurlTLS>& tls,
                        const optional<CurlSocket>& socket // socket is used async, the lifespan must be longer
) {
    static DnsCache* dnsCache = DnsCache::GetInstance();

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        return nullptr;
    }

    string totalUrl = httpsFlag ? "https://" : "http://";
    string hostIP;
    if (replaceHostWithIp && dnsCache->GetIPFromDnsCache(host, hostIP)) {
        totalUrl.append(hostIP);
    } else {
        totalUrl.append(host);
    }
    totalUrl.append(url);
    if (!queryString.empty()) {
        totalUrl.append("?").append(queryString);
    }
    curl_easy_setopt(curl, CURLOPT_URL, totalUrl.c_str());
    for (const auto& iter : header) {
        headers = curl_slist_append(headers, (iter.first + ":" + iter.second).c_str());
    }
    if (headers != NULL) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    curl_easy_setopt(curl, CURLOPT_PORT, port);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
    if (!body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (void*)body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
    }

    if (followRedirects) {
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    }

    if (httpsFlag) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    if (tls.has_value()) {
        if (!tls->mCaFile.empty()) {
            curl_easy_setopt(curl, CURLOPT_CAINFO, tls->mCaFile.c_str());
        }
        if (!tls->mCertFile.empty()) {
            curl_easy_setopt(curl, CURLOPT_SSLCERT, tls->mCertFile.c_str());
        }
        if (!tls->mKeyFile.empty()) {
            curl_easy_setopt(curl, CURLOPT_SSLKEY, tls->mKeyFile.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, tls->mInsecureSkipVerify ? 0 : 1);
    }

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
    if (!intf.empty()) {
        curl_easy_setopt(curl, CURLOPT_INTERFACE, intf.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response.mBody.get());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response.mWriteCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &(response.GetHeader()));
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_write_callback);

    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1);
    curl_easy_setopt(curl, CURLOPT_NETRC, CURL_NETRC_IGNORED);
    if (socket.has_value()) {
        curl_easy_setopt(curl, CURLOPT_SOCKOPTDATA, &socket.value());
        curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION, socket_write_callback);
    }

    return curl;
}

bool SendHttpRequest(unique_ptr<HttpRequest>&& request, HttpResponse& response) {
    curl_slist* headers = NULL;
    CURL* curl = CreateCurlHandler(request->mMethod,
                                   request->mHTTPSFlag,
                                   request->mHost,
                                   request->mPort,
                                   request->mUrl,
                                   request->mQueryString,
                                   request->mHeader,
                                   request->mBody,
                                   response,
                                   headers,
                                   request->mTimeout,
                                   AppConfig::GetInstance()->IsHostIPReplacePolicyEnabled(),
                                   AppConfig::GetInstance()->GetBindInterface(),
                                   request->mFollowRedirects,
                                   request->mTls,
                                   request->mSocket);
    if (curl == NULL) {
        LOG_ERROR(sLogger,
                  ("failed to init curl handler", "failed to init curl client")("request address", request.get()));
        return false;
    }
    bool success = false;
    while (true) {
        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            long statusCode = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
            curl_off_t responseTime;
            curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME_T, &responseTime);
            auto responseTimeMs = responseTime / 1000;
            response.SetNetworkStatus(NetworkCode::Ok, "");
            response.SetStatusCode(statusCode);
            response.SetResponseTime(chrono::milliseconds(responseTimeMs));
            success = true;
            LOG_DEBUG(sLogger,
                      ("send http request succeeded, host", request->mHost)(
                          "response time", ToString(responseTimeMs) + "ms")("try cnt", ToString(request->mTryCnt)));
            break;
        } else if (request->mTryCnt < request->mMaxTryCnt) {
            LOG_DEBUG(sLogger,
                      ("failed to send http request", "retry immediately")("host", request->mHost)(
                          "try cnt", request->mTryCnt)("errMsg", curl_easy_strerror(res)));
            ++request->mTryCnt;
        } else {
            auto errMsg = curl_easy_strerror(res);
            response.SetNetworkStatus(GetNetworkStatus(res), errMsg);
            LOG_DEBUG(sLogger,
                      ("failed to send http request",
                       "abort")("host", request->mHost)("try cnt", ToString(request->mTryCnt))("errMsg", errMsg));
            break;
        }
    }
    if (headers != NULL) {
        curl_slist_free_all(headers);
    }
    curl_easy_cleanup(curl);
    return success;
}

bool AddRequestToMultiCurlHandler(CURLM* multiCurl, unique_ptr<AsynHttpRequest>&& request) {
    curl_slist* headers = NULL;
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
                                   AppConfig::GetInstance()->GetBindInterface(),
                                   request->mFollowRedirects,
                                   request->mTls,
                                   request->mSocket);
    if (curl == NULL) {
        request->mResponse.SetNetworkStatus(NetworkCode::Other, "failed to init curl handler");
        LOG_ERROR(sLogger, ("failed to send request", "failed to init curl handler")("request address", request.get()));
        request->OnSendDone(request->mResponse);
        return false;
    }

    request->mPrivateData = headers;
    curl_easy_setopt(curl, CURLOPT_PRIVATE, request.get());
    request->mLastSendTime = chrono::system_clock::now();

    CURLMcode res = curl_multi_add_handle(multiCurl, curl);
    if (res != CURLM_OK) {
        request->mResponse.SetNetworkStatus(NetworkCode::Other, "failed to add the easy curl handle to multi_handle");
        LOG_ERROR(sLogger,
                  ("failed to send request", "failed to add the easy curl handle to multi_handle")(
                      "errMsg", curl_multi_strerror(res))("request address", request.get()));
        request->OnSendDone(request->mResponse);
        curl_easy_cleanup(curl);
        return false;
    }
    // let callback destruct the request
    request.release();
    return true;
}

void HandleCompletedAsynRequests(CURLM* multiCurl, int& runningHandlers) {
    int msgsLeft = 0;
    CURLMsg* msg = curl_multi_info_read(multiCurl, &msgsLeft);
    while (msg) {
        if (msg->msg == CURLMSG_DONE) {
            bool requestReused = false;
            CURL* handler = msg->easy_handle;
            AsynHttpRequest* request = nullptr;
            curl_easy_getinfo(handler, CURLINFO_PRIVATE, &request);
            switch (msg->data.result) {
                case CURLE_OK: {
                    long statusCode = 0;
                    curl_easy_getinfo(handler, CURLINFO_RESPONSE_CODE, &statusCode);
                    curl_off_t responseTime;
                    curl_easy_getinfo(handler, CURLINFO_TOTAL_TIME_T, &responseTime);
                    auto responseTimeMs = responseTime / 1000;
                    request->mResponse.SetNetworkStatus(NetworkCode::Ok, "");
                    request->mResponse.SetStatusCode(statusCode);
                    request->mResponse.SetResponseTime(chrono::milliseconds(responseTimeMs));
                    LOG_DEBUG(
                        sLogger,
                        ("send http request succeeded, request address",
                         request)("host", request->mHost)("response time", ToString(responseTimeMs) + "ms")(
                            "try cnt", ToString(request->mTryCnt))("errMsg", curl_easy_strerror(msg->data.result)));
                    request->OnSendDone(request->mResponse);
                    break;
                }
                default:
                    // considered as network error
                    if (request->mTryCnt < request->mMaxTryCnt) {
                        LOG_DEBUG(sLogger,
                                  ("failed to send http request", "retry immediately")("request address",
                                                                                       request)("host", request->mHost)(
                                      "try cnt", request->mTryCnt)("errMsg", curl_easy_strerror(msg->data.result)));
                        // free first，because mPrivateData will be reset in AddRequestToMultiCurlHandler
                        if (request->mPrivateData) {
                            curl_slist_free_all((curl_slist*)request->mPrivateData);
                            request->mPrivateData = nullptr;
                        }
                        ++request->mTryCnt;
                        AddRequestToMultiCurlHandler(multiCurl, unique_ptr<AsynHttpRequest>(request));
                        ++runningHandlers;
                        requestReused = true;
                    } else {
                        auto errMsg = curl_easy_strerror(msg->data.result);
                        request->mResponse.SetNetworkStatus(GetNetworkStatus(msg->data.result), errMsg);
                        LOG_DEBUG(sLogger,
                                  ("failed to send http request", "abort")("request address", request)(
                                      "host", request->mHost)("try cnt", ToString(request->mTryCnt))("errMsg", errMsg));
                        request->OnSendDone(request->mResponse);
                    }
                    break;
            }

            curl_multi_remove_handle(multiCurl, handler);
            curl_easy_cleanup(handler);
            if (!requestReused) {
                if (request->mPrivateData) {
                    curl_slist_free_all((curl_slist*)request->mPrivateData);
                }
                delete request;
            }
        }
        msg = curl_multi_info_read(multiCurl, &msgsLeft);
    }
}

void SendAsynRequests(CURLM* multiCurl) {
    CURLMcode mc;
    int runningHandlers = 0;
    do {
        if ((mc = curl_multi_perform(multiCurl, &runningHandlers)) != CURLM_OK) {
            LOG_ERROR(
                sLogger,
                ("failed to call curl_multi_perform", "sleep 100ms and retry")("errMsg", curl_multi_strerror(mc)));
            this_thread::sleep_for(chrono::milliseconds(100));
            continue;
        }
        HandleCompletedAsynRequests(multiCurl, runningHandlers);

        long curlTimeout = -1;
        if ((mc = curl_multi_timeout(multiCurl, &curlTimeout)) != CURLM_OK) {
            LOG_WARNING(
                sLogger,
                ("failed to call curl_multi_timeout", "use default timeout 1s")("errMsg", curl_multi_strerror(mc)));
        }
        struct timeval timeout {
            1, 0
        };
        if (curlTimeout >= 0) {
            timeout.tv_sec = curlTimeout / 1000;
            timeout.tv_usec = (curlTimeout % 1000) * 1000;
        }

        int maxfd = -1;
        fd_set fdread;
        fd_set fdwrite;
        fd_set fdexcep;
        FD_ZERO(&fdread);
        FD_ZERO(&fdwrite);
        FD_ZERO(&fdexcep);
        if ((mc = curl_multi_fdset(multiCurl, &fdread, &fdwrite, &fdexcep, &maxfd)) != CURLM_OK) {
            LOG_ERROR(sLogger, ("failed to call curl_multi_fdset", "sleep 100ms")("errMsg", curl_multi_strerror(mc)));
        }
        if (maxfd == -1) {
            // sleep min(timeout, 100ms) according to libcurl
            int64_t sleepMs = (curlTimeout >= 0 && curlTimeout < 100) ? curlTimeout : 100;
            this_thread::sleep_for(chrono::milliseconds(sleepMs));
        } else {
            select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
        }
    } while (runningHandlers);
}

} // namespace logtail

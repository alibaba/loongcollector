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
#include <functional>
#include <map>
#include <memory>
#include <string>

class curl_slist;

namespace logtail {

struct CurlTLS;

enum NetworkCode {
    Ok = 0,
    ConnectionFailed,
    RemoteAccessDenied,
    SSLConnectError,
    SSLCertError,
    SSLOtherProblem,
    SendDataFailed,
    RecvDataFailed,
    TIMEOUT, // 超时
    Other // 回显请求收到
};

struct NetWorkStatus {
    NetworkCode mCode = NetworkCode::Ok;
    std::string mMessage;
};

bool caseInsensitiveComp(const char lhs, const char rhs);

bool compareHeader(const std::string& lhs, const std::string& rhs);

size_t DefaultWriteCallback(char* buffer, size_t size, size_t nmemb, void* data);

class HttpResponse {
    friend void* CreateCurlHandler(const std::string& method,
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
                                   bool replaceHostWithIp,
                                   const std::string& intf,
                                   bool followRedirects,
                                   std::optional<CurlTLS> tls);

public:
    HttpResponse()
        : mHeader(compareHeader),
          mBody(new std::string(), [](void* p) { delete static_cast<std::string*>(p); }),
          mWriteCallback(DefaultWriteCallback) {};
    HttpResponse(void* body,
                 const std::function<void(void*)>& bodyDeleter,
                 size_t (*callback)(char*, size_t, size_t, void*))
        : mHeader(compareHeader), mBody(body, bodyDeleter), mWriteCallback(callback) {}

    int32_t GetStatusCode() const { return mStatusCode; }
    const std::map<std::string, std::string, decltype(compareHeader)*>& GetHeader() const { return mHeader; }

    template <class T>
    const T* GetBody() const {
        return static_cast<const T*>(mBody.get());
    }

    template <class T>
    T* GetBody() {
        return static_cast<T*>(mBody.get());
    }

    void SetStatusCode(int32_t code) { mStatusCode = code; }

    void SetNetworkStatus(CURLcode code) {
        mNetworkStatus.mMessage = curl_easy_strerror(code);
        // please refer to https://curl.se/libcurl/c/libcurl-errors.html
        switch (code) {
            case CURLE_OK:
                mNetworkStatus.mCode = NetworkCode::Ok;
                break;
            case CURLE_COULDNT_CONNECT:
                mNetworkStatus.mCode = NetworkCode::ConnectionFailed;
                break;
            case CURLE_LOGIN_DENIED:
            case CURLE_REMOTE_ACCESS_DENIED:
                mNetworkStatus.mCode = NetworkCode::RemoteAccessDenied;
                break;
            case CURLE_OPERATION_TIMEDOUT:
                mNetworkStatus.mCode = NetworkCode::TIMEOUT;
                break;
            case CURLE_SSL_CONNECT_ERROR:
                mNetworkStatus.mCode = NetworkCode::SSLConnectError;
                break;
            case CURLE_SSL_CERTPROBLEM:
            case CURLE_SSL_CACERT:
                mNetworkStatus.mCode = NetworkCode::SSLCertError;
                break;
            case CURLE_SEND_ERROR:
            case CURLE_SEND_FAIL_REWIND:
                mNetworkStatus.mCode = NetworkCode::SendDataFailed;
                break;
            case CURLE_RECV_ERROR:
                mNetworkStatus.mCode = NetworkCode::RecvDataFailed;
                break;
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
                mNetworkStatus.mCode = NetworkCode::SSLOtherProblem;
                break;
            case CURLE_FAILED_INIT:
            default:
                mNetworkStatus.mCode = NetworkCode::Other;
                break;
        }
    }

    NetWorkStatus GetNetworkStatus() { return mNetworkStatus; }

private:
    int32_t mStatusCode = 0; // 0 means no response from server
    NetWorkStatus mNetworkStatus; // 0 means no error
    std::map<std::string, std::string, decltype(compareHeader)*> mHeader;
    std::unique_ptr<void, std::function<void(void*)>> mBody;
    size_t (*mWriteCallback)(char*, size_t, size_t, void*) = nullptr;
};

} // namespace logtail

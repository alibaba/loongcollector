// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "CurlImp.h"
#include "Exception.h"
#include "CurlAsynInstance.h"
#include "DNSCache.h"
#include "app_config/AppConfig.h"
#include <curl/curl.h>

using namespace std;

namespace logtail {
namespace sdk {

    static CURLcode globalInitCode = curl_global_init(CURL_GLOBAL_ALL);

    CurlClient::CurlClient() {
        // Initliaze sending threads.
        CurlAsynInstance::GetInstance();
    }

    // callback function to store the response
    static size_t data_write_callback(char* buffer, size_t size, size_t nmemb, string* write_buf) {
        unsigned long sizes = size * nmemb;

        if (buffer == NULL) {
            return 0;
        }

        write_buf->append(buffer, sizes);
        return sizes;
    }

    static size_t header_write_callback(char* buffer, size_t size, size_t nmemb, map<string, string>* write_buf) {
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

    CURL* PackCurlRequest(const std::string& httpMethod,
                          const std::string& host,
                          const int32_t port,
                          const std::string& url,
                          const std::string& queryString,
                          const std::map<std::string, std::string>& header,
                          const std::string& body,
                          const int32_t timeout,
                          HttpMessage& httpMessage,
                          const std::string& intf,
                          const bool httpsFlag,
                          curl_slist*& headers) {
        static DnsCache* dnsCache = DnsCache::GetInstance();

        CURL* curl = curl_easy_init();
        if (curl == NULL)
            return NULL;

        string totalUrl = httpsFlag ? "https://" : "http://";
        std::string hostIP;
        if (AppConfig::GetInstance()->IsHostIPReplacePolicyEnabled() && dnsCache->GetIPFromDnsCache(host, hostIP)) {
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
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, httpMethod.c_str());
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (void*)body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
        }

        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
        curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1);
        curl_easy_setopt(curl, CURLOPT_NETRC, CURL_NETRC_IGNORED);

        if (httpsFlag) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        }
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
        if (!intf.empty()) {
            curl_easy_setopt(curl, CURLOPT_INTERFACE, intf.c_str());
        }
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &(httpMessage.content));
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, data_write_callback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &(httpMessage.header));
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_write_callback);
        return curl;
    }

    void CurlClient::Send(const std::string& httpMethod,
                          const std::string& host,
                          const int32_t port,
                          const std::string& url,
                          const std::string& queryString,
                          const std::map<std::string, std::string>& header,
                          const std::string& body,
                          const int32_t timeout,
                          HttpMessage& httpMessage,
                          const std::string& intf,
                          const bool httpsFlag) {
        curl_slist* headers = NULL;
        CURL* curl = PackCurlRequest(
            httpMethod, host, port, url, queryString, header, body, timeout, httpMessage, intf, httpsFlag, headers);
        if (curl == NULL) {
            throw LOGException(LOGE_UNKNOWN_ERROR, "Init curl instance error.");
        }

        CURLcode res = curl_easy_perform(curl);
        if (headers != NULL) {
            curl_slist_free_all(headers);
        }

        switch (res) {
            case CURLE_OK:
                break;
            case CURLE_OPERATION_TIMEDOUT:
                curl_easy_cleanup(curl);
                throw LOGException(LOGE_CLIENT_OPERATION_TIMEOUT, "Request operation timeout.");
                break;
            case CURLE_COULDNT_CONNECT:
                curl_easy_cleanup(curl);
                throw LOGException(LOGE_REQUEST_TIMEOUT, "Can not connect to server.");
                break;
            default:
                curl_easy_cleanup(curl);
                throw LOGException(LOGE_REQUEST_ERROR,
                                   string("Request operation failed, curl error code : ") + curl_easy_strerror(res));
                break;
        }

        long http_code = 0;
        if ((res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code)) != CURLE_OK) {
            curl_easy_cleanup(curl);
            throw LOGException(LOGE_UNKNOWN_ERROR,
                               string("Get curl response code error, curl error code : ") + curl_easy_strerror(res));
        }
        httpMessage.statusCode = (int32_t)http_code;
        curl_easy_cleanup(curl);
        if (!httpMessage.IsLogServiceResponse()) {
            throw LOGException(LOGE_REQUEST_ERROR, "Get invalid response");
        }
    }

    void CurlClient::AsynSend(AsynRequest* request) {
        CurlAsynInstance* instance = CurlAsynInstance::GetInstance();
        instance->AddRequest(request);
    }


} // namespace sdk
} // namespace logtail

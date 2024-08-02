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

#include "common/DNSCache.h"

using namespace std;

namespace logtail {

static size_t data_write_callback(char* buffer, size_t size, size_t nmemb, string* write_buf) {
    unsigned long sizes = size * nmemb;

    if (buffer == NULL) {
        return 0;
    }

    write_buf->append(buffer, sizes);
    return sizes;
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

CURL* CreateCurlHandler(const std::string& method,
                        bool httpsFlag,
                        const std::string& host,
                        const std::string& url,
                        const std::string& queryString,
                        const std::map<std::string, std::string>& header,
                        const std::string& body,
                        HttpResponse& response,
                        curl_slist*& headers,
                        bool replaceHostWithIp,
                        const std::string& intf,
                        uint32_t timeout) {
    static DnsCache* dnsCache = DnsCache::GetInstance();

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        return nullptr;
    }

    string totalUrl = httpsFlag ? "https://" : "http://";
    std::string hostIP;
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
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
    if (!body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (void*)body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
    }

    if (httpsFlag) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
    if (!intf.empty()) {
        curl_easy_setopt(curl, CURLOPT_INTERFACE, intf.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &(response.mBody));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, data_write_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &(response.mHeader));
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_write_callback);

    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1);
    curl_easy_setopt(curl, CURLOPT_NETRC, CURL_NETRC_IGNORED);

    return curl;
}
} // namespace logtail

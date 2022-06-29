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

#include "DNSCache.h"
#include <cstring>
#if defined(__linux__)
#include <arpa/inet.h>
#include <netdb.h>
#elif defined(_MSC_VER)
#include <WinSock2.h>
#include <ws2tcpip.h>
#endif

namespace logtail {
namespace sdk {

    // ParseHost only supports IPv4 now.
    bool DnsCache::ParseHost(const char* host, std::string& ip) {
#if defined(__linux__)
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;

        char* buffer = NULL;
        if (host && host[0]) {
            if (IsRawIp(host)) {
                if ((addr.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE)
                    return false;
            } else {
                int bufferLen = 2048;
                int rc, res;
                struct hostent* hp = NULL;
                struct hostent h;
                while (true) {
                    buffer = new char[bufferLen];
                    res = gethostbyname_r(host, &h, buffer, bufferLen, &hp, &rc);
                    if (res == ERANGE) {
                        if (buffer != NULL)
                            delete[] buffer;
                        bufferLen *= 4;
                        if (bufferLen > 32768) //32KB
                            return false;
                        continue;
                    }
                    if (res != 0 || hp == NULL || hp->h_addr == NULL) {
                        if (buffer != NULL)
                            delete[] buffer;
                        return false;
                    } else
                        break;
                }
                addr.sin_addr.s_addr = *((in_addr_t*)(hp->h_addr));
            }
        } else {
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        }
        ip = inet_ntoa(addr.sin_addr);
        if (buffer != NULL)
            delete[] buffer;
        return true;
#elif defined(_MSC_VER)
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        if (host && host[0]) {
            if (IsRawIp(host)) {
                if ((addr.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE)
                    return false;
            } else {
                addrinfo hints;
                struct addrinfo* result = NULL;
                std::memset(&hints, 0, sizeof(hints));
                auto ret = ::getaddrinfo(host, NULL, &hints, &result);
                if (ret != 0) {
                    return false;
                }

                bool found = false;
                for (auto ptr = result; ptr != NULL; ptr = ptr->ai_next) {
                    if (AF_INET == ptr->ai_family) {
                        addr.sin_addr = ((struct sockaddr_in*)ptr->ai_addr)->sin_addr;
                        found = true;
                        break;
                    }
                }
                freeaddrinfo(result);
                if (!found)
                    return false;
            }
        } else {
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        }
        ip = inet_ntoa(addr.sin_addr);
        return true;
#endif
    }

} // namespace sdk
} // namespace logtail
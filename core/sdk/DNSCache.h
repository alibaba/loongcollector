/*
 * Copyright 2022 iLogtail Authors
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
#include <ctime>
#include <cstdint>
#include <mutex>
#include <map>

namespace logtail {
namespace sdk {

    class DnsCache {
        int32_t mUpdateTime;
        int32_t mDnsTTL;
        std::mutex mDnsCacheLock;
        std::map<std::string, std::pair<std::string, int32_t>> mDnsCacheData;

    public:
        static DnsCache* GetInstance() {
            static DnsCache singleton;
            return &singleton;
        }

        bool UpdateHostInDnsCache(const std::string& host, std::string& address) {
            int32_t currentTime = time(NULL);
            bool status = false;
            std::lock_guard<std::mutex> lock(mDnsCacheLock);

            auto itr = mDnsCacheData.find(host);
            if (itr == mDnsCacheData.end() || currentTime - (itr->second).second >= 3) {
                if (ParseHost(host.c_str(), address)) {
                    status = true;
                    mDnsCacheData[host] = std::make_pair(address, currentTime);
                } else {
                    if (itr == mDnsCacheData.end()) {
                        mDnsCacheData[host] = std::make_pair(host, currentTime);
                    } else {
                        mDnsCacheData[host] = std::make_pair((itr->second).first, currentTime);
                    }
                }
            }

            return status;
        }

        bool GetIPFromDnsCache(const std::string& host, std::string& address) {
            bool found = false;
            {
                std::lock_guard<std::mutex> lock(mDnsCacheLock);
                if (!RemoveTimeOutDnsCache()) // not time out
                {
                    auto itr = mDnsCacheData.find(host);
                    if (itr != mDnsCacheData.end()) {
                        address = (itr->second).first;
                        found = true;
                    }
                }
            }
            if (!found) // time out or not found
                return UpdateHostInDnsCache(host, address);
            return found;
        }

    private:
        DnsCache(const int32_t ttlSeconds = 60 * 10) : mUpdateTime(time(NULL)), mDnsTTL(ttlSeconds) {}
        ~DnsCache() = default;

        bool IsRawIp(const char* host) {
            unsigned char c, *p;
            p = (unsigned char*)host;
            while ((c = (*p++)) != '\0') {
                if ((c != '.') && (c < '0' || c > '9'))
                    return false;
            }
            return true;
        }

        bool ParseHost(const char* host, std::string& ip);

        bool RemoveTimeOutDnsCache() {
            int32_t currentTime = time(NULL);
            bool isTimeOut = false;
            if (currentTime - mUpdateTime >= mDnsTTL) {
                isTimeOut = true;
                mDnsCacheData.clear();
                mUpdateTime = currentTime;
            }
            return isTimeOut;
        }
    };

} // namespace sdk
} // namespace logtail
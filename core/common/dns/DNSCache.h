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

#include <cstdint>
#include <ctime>

#include <map>
#include <mutex>

#include "common/Flags.h"

DECLARE_FLAG_INT32(dns_cache_ttl_sec);

namespace logtail {

class DnsCache {
    static constexpr int32_t kDnsRetryCooldownSec = 3; // DNS 解析重试冷却时间

    int32_t mUpdateTime;
    int32_t mDnsTTL;
    std::mutex mDnsCacheLock;
    std::map<std::string, std::pair<std::string, int32_t>> mDnsCacheData;
    std::map<std::string, int32_t> mDnsFailedCache; // 失败缓存：host -> 失败时间

public:
    static DnsCache* GetInstance() {
        static DnsCache singleton;
        return &singleton;
    }

    bool UpdateHostInDnsCache(const std::string& host, std::string& address) {
        int32_t currentTime = time(NULL);
        bool status = false;
        std::lock_guard<std::mutex> lock(mDnsCacheLock);

        // 检查失败缓存，在冷却期内不重试 DNS
        auto failedItr = mDnsFailedCache.find(host);
        if (failedItr != mDnsFailedCache.end() && currentTime - failedItr->second < kDnsRetryCooldownSec) {
            return false;
        }

        auto itr = mDnsCacheData.find(host);
        if (itr == mDnsCacheData.end() || currentTime - (itr->second).second >= kDnsRetryCooldownSec) {
            if (ParseHost(host.c_str(), address)) {
                status = true;
                mDnsCacheData[host] = std::make_pair(address, currentTime);
                mDnsFailedCache.erase(host); // 解析成功，移除失败缓存
            } else {
                // DNS 解析失败，记录到失败缓存
                mDnsFailedCache[host] = currentTime;
                if (itr != mDnsCacheData.end()) {
                    // 保留之前成功解析的 IP，更新时间戳
                    mDnsCacheData[host] = std::make_pair((itr->second).first, currentTime);
                }
                status = false;
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
    DnsCache(const int32_t ttlSeconds = INT32_FLAG(dns_cache_ttl_sec));
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
            mDnsFailedCache.clear(); // 同时清理失败缓存
            mUpdateTime = currentTime;
        }
        return isTimeOut;
    }
};

} // namespace logtail

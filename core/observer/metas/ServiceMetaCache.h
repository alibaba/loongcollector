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


#include <utility>
#include <list>
#include <unordered_map>
#include <ostream>
#include "interface/type.h"
#include "network/NetworkConfig.h"

namespace logtail {


struct ServiceMeta {
    ServiceMeta(const std::string& host, ServiceCategory category, long time)
        : Host(host), Category(category), time(time) {}
    ServiceMeta() = default;

    // Only configured when found unknown dns addr.
    std::string Host;
    ServiceCategory Category{ServiceCategory::Unknown};
    long time{0};

    friend std::ostream& operator<<(std::ostream& os, const ServiceMeta& hostname) {
        os << "Host: " << hostname.Host << " Category: " << ServiceCategoryToString(hostname.Category)
           << " time: " << hostname.time;
        return os;
    }

    std::string ToString() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

    bool Empty() const { return time == 0; }
};

static const ServiceMeta* sEmptyHost = new ServiceMeta;


template <typename K, typename V>
class LRUCache {
public:
    LRUCache() = delete;
    explicit LRUCache(uint32_t capacity) : cap(capacity) {}

    const V* Get(const K& key, std::function<void(V*)> wrapper) {
        if (mIndexMap.find(key) == mIndexMap.end()) {
            return nullptr;
        }
        mData.splice(mData.begin(), mData, mIndexMap[key]);
        if (wrapper != nullptr) {
            wrapper(&(mData.begin()->second));
        }
        return &mData.front().second;
    }

    void Put(const K& k,const V&& v, std::function<void(V*)> wrapper) {
        if (Get(k, wrapper) != nullptr) {
            mData.begin()->second = v;
            wrapper(&mData.begin()->second);
        } else {
            if (mData.size() == cap) {
                K delKey = std::move(mData.back().first);
                mData.pop_back();
                mIndexMap.erase(delKey);
            }
            mData.emplace_front(k, v);
            mIndexMap[k] = mData.begin();
        }
    }

private:
    uint32_t cap = 0;
    std::list<std::pair<K, V>> mData;
    std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> mIndexMap;
};

// todo: Use LRUCache to refine ServiceMetaCache
class ServiceMetaCache {
public:
    ServiceMetaCache() = delete;

private:
    uint32_t cap = 0;
    std::list<std::pair<std::string, ServiceMeta>> mData;
    std::unordered_map<std::string, std::list<std::pair<std::string, ServiceMeta>>::iterator> mIndexMap;

private:
    explicit ServiceMetaCache(uint32_t capacity) { cap = capacity; }

    const ServiceMeta& Get(const std::string& remoteIP, ProtocolType protocolType);

    const ServiceMeta& Get(const std::string& remoteIP);

    void Put(const std::string& remoteIP, const std::string& host, ProtocolType protocolType);

    friend class ServiceMetaManager;
    friend class HostnameMetaUnittest;
};

class ServiceMetaManager {
public:
    static ServiceMetaManager* GetInstance() {
        static auto sManager = new ServiceMetaManager();
        return sManager;
    }

    // AddHostName method only called by dns protocol parser to store recently used dns hostname and ip mapping.
    void AddHostName(uint32_t pid, const std::string& hostname, const std::string& ip);

    // GetHostName called by other protocol parser to get remote hostname and wrapper hostname category.
    const ServiceMeta& GetOrPutServiceMeta(uint32_t pid, const std::string& ip, ProtocolType protocolType);

    // GetHostName called by statistics to get remote hostname and hostname category.
    const ServiceMeta& GetServiceMeta(uint32_t pid, const std::string& ip);

    // OnProcessDestroy delete cache metas.
    void OnProcessDestroy(uint32_t pid);

    // GarbageTimeoutHostname delete the unused metas.
    void GarbageTimeoutHostname(long currentTime);

private:
    ServiceMetaManager() = default;
    inline const ServiceMeta& doGetOrPutServiceMeta(uint32_t pid, const std::string& ip, ProtocolType protocolType);
    inline const ServiceMeta& doGetServiceMeta(uint32_t pid, const std::string& ip);


private:
    std::unordered_map<uint32_t, ServiceMetaCache*> mHostnameMetas;
    friend class HostnameMetaUnittest;
};


} // namespace logtail

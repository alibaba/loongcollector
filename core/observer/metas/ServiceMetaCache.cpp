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

#include "ServiceMetaCache.h"
#include "Logger.h"

namespace logtail {

const ServiceMeta& ServiceMetaCache::Get(const std::string& remoteIP, ProtocolType protocolType) {
    if (mIndexMap.find(remoteIP) == mIndexMap.end()) {
        return *sEmptyHost;
    }
    mData.splice(mData.begin(), mData, mIndexMap[remoteIP]);
    mData.begin()->second.Category = DetectRemoteServiceCategory(protocolType);
    mData.front().second.time = time(nullptr);
    return mData.front().second;
}

const ServiceMeta& ServiceMetaCache::Get(const std::string& remoteIP) {
    if (mIndexMap.find(remoteIP) == mIndexMap.end()) {
        return *sEmptyHost;
    }
    mData.splice(mData.begin(), mData, mIndexMap[remoteIP]);
    mData.front().second.time = time(nullptr);
    return mData.front().second;
}


void ServiceMetaCache::Put(const std::string& remoteIP, const std::string& host, ProtocolType protocolType) {
    if (!Get(remoteIP, ProtocolType_NumProto).Host.empty()) {
        mData.begin()->second.Host = host;
        mData.begin()->second.Category = DetectRemoteServiceCategory(protocolType);
        mData.begin()->second.time = time(nullptr);
    } else {
        if (mData.size() == cap) {
            std::string delKey = std::move(mData.back().first);
            mData.pop_back();
            mIndexMap.erase(delKey);
        }
        mData.emplace_front(remoteIP, ServiceMeta{host, DetectRemoteServiceCategory(protocolType), time(nullptr)});
        mIndexMap[remoteIP] = mData.begin();
    }
}


void ServiceMetaManager::AddHostName(uint32_t pid, const std::string& hostname, const std::string& ip) {
    auto meta = mHostnameMetas.find(pid);
    if (meta == mHostnameMetas.end()) {
        meta = mHostnameMetas.insert(std::make_pair(pid, new ServiceMetaCache(200))).first;
    }
    meta->second->Put(ip, hostname, ProtocolType_HTTP);
    LOG_TRACE(sLogger, ("ServiceMeta ADD hostname, ip", ip)("data", meta->second->mData.begin()->second.ToString()));
}

const ServiceMeta&
ServiceMetaManager::GetOrPutServiceMeta(uint32_t pid, const std::string& ip, ProtocolType protocolType) {
    auto& meta = doGetOrPutServiceMeta(pid, ip, protocolType);
    LOG_TRACE(sLogger, ("ServiceMeta GET or PUT, pid", pid)("ip", ip)("data", meta.ToString()));
    return meta;
}

const ServiceMeta& ServiceMetaManager::GetServiceMeta(uint32_t pid, const std::string& ip) {
    auto& meta = doGetServiceMeta(pid, ip);
    LOG_TRACE(sLogger, ("ServiceMeta GET, pid", pid)("ip", ip)("data", meta.ToString()));
    return meta;
}

void ServiceMetaManager::OnProcessDestroy(uint32_t pid) {
    auto meta = mHostnameMetas.find(pid);
    if (meta == mHostnameMetas.end()) {
        return;
    }
    delete meta->second;
    mHostnameMetas.erase(meta);
    LOG_TRACE(sLogger, ("destroy host name, pid", pid));
}

void ServiceMetaManager::GarbageTimeoutHostname(long currentTime) {
    long timeoutTime = currentTime - INT64_FLAG(sls_observer_network_hostname_timeout);
    for (auto iter = mHostnameMetas.begin(); iter != mHostnameMetas.end();) {
        while (!iter->second->mData.empty()) {
            if (iter->second->mData.back().second.time > timeoutTime) {
                break;
            }
            LOG_TRACE(sLogger, ("destroy host name remove history meta", iter->second->mData.back().second.ToString()));
            std::string delKey = std::move(iter->second->mData.back().first);
            iter->second->mIndexMap.erase(delKey);
            iter->second->mData.pop_back();
        }
        if (iter->second->mData.empty()) {
            delete iter->second;
            iter = mHostnameMetas.erase(iter);
        } else {
            ++iter;
        }
    }
}

inline const ServiceMeta&
ServiceMetaManager::doGetOrPutServiceMeta(uint32_t pid, const std::string& ip, ProtocolType protocolType) {
    auto meta = mHostnameMetas.find(pid);
    if (meta == mHostnameMetas.end()) {
        if (IsRemoteInvokeProtocolType(protocolType)) {
            return *sEmptyHost;
        }
        meta = mHostnameMetas.insert(std::make_pair(pid, new ServiceMetaCache(200))).first;
        meta->second->Put(ip, "", protocolType);
        return meta->second->mData.begin()->second;
    }
    auto& data = meta->second->Get(ip, protocolType);
    if (!data.Empty()) {
        return data;
    }
    meta->second->Put(ip, "", protocolType);
    return meta->second->mData.begin()->second;
}

inline const ServiceMeta& ServiceMetaManager::doGetServiceMeta(uint32_t pid, const std::string& ip) {
    auto meta = mHostnameMetas.find(pid);
    if (meta == mHostnameMetas.end()) {
        return *sEmptyHost;
    }
    return meta->second->Get(ip);
}
} // namespace logtail
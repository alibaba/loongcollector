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
// See the License for the specific l

#include "K8sMetadata.h"

#include <chrono>
#include <ctime>

#include <future>
#include <thread>

#include "app_config/AppConfig.h"
#include "common/MachineInfoUtil.h"
#include "common/StringTools.h"
#include "common/http/Curl.h"
#include "common/http/HttpRequest.h"
#include "common/http/HttpResponse.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail {

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

bool K8sMetadata::Enable() {
#ifdef APSARA_UNIT_TEST_MAIN
    return false;
#endif
    if (!AppConfig::GetInstance()->IsPurageContainerMode()) {
        return false;
    }
    // Is this enough?
    return true;
}

K8sMetadata::K8sMetadata(size_t ipCacheSize, size_t cidCacheSize, size_t externalIpCacheSize, int32_t fetchIntervalSec)
    : ipCache(ipCacheSize, 20),
      containerCache(cidCacheSize, 20),
      externalIpCache(externalIpCacheSize, 20),
      mFetchIntervalSeconds(fetchIntervalSec) {
    mServiceHost = STRING_FLAG(singleton_service);
    mServicePort = INT32_FLAG(singleton_port);
    // TODO @qianlu.kk
    const char* value = getenv("_node_ip_");
    if (value != NULL) {
        mHostIp = StringTo<string>(value);
    } else {
        mHostIp = GetHostIp();
    }
#ifndef APSARA_UNIT_TEST_MAIN
    // mHostIp = GetHostIp();
    LOG_INFO(sLogger, ("[metadata] host ip", mHostIp));
#else
    mServiceHost = "47.95.70.43";
    mServicePort = 8899;
    mHostIp = "172.16.57.207";
#endif
}

bool K8sMetadata::FromInfoJson(const Json::Value& json, k8sContainerInfo& info) {
    if (!json.isMember(imageKey) || !json.isMember(labelsKey) || !json.isMember(namespaceKey)
        || !json.isMember(workloadKindKey) || !json.isMember(workloadNameKey)) {
        return false;
    }

    for (const auto& key : json[imageKey].getMemberNames()) {
        if (json[imageKey].isMember(key)) {
            info.images[key] = json[imageKey][key].asString();
        }
    }
    for (const auto& key : json[labelsKey].getMemberNames()) {
        if (json[labelsKey].isMember(key)) {
            info.labels[key] = json[labelsKey][key].asString();

            if (key == appIdKey) {
                info.appId = json[labelsKey][key].asString();
            } else if (key == appNameKey) {
                info.appName = json[labelsKey][key].asString();
            }
        }
    }

    info.k8sNamespace = json[namespaceKey].asString();
    if (json.isMember(serviceNameKey)) {
        info.serviceName = json[serviceNameKey].asString();
    }
    if (json.isMember(containerIdKey)) {
        for (const auto& member : json[containerIdKey]) {
            info.containerIds.push_back(member.asString());
        }
    }
    info.workloadKind = json[workloadKindKey].asString();
    info.workloadName = json[workloadNameKey].asString();
    info.podIp = json[podIpKey].asString();
    info.podName = json[podNameKey].asString();
    info.serviceName = json[serviceNameKey].asString();
    info.startTime = json[startTimeKey].asInt64();
    info.timestamp = std::time(0);
    return true;
}

bool ContainerInfoIsExpired(std::shared_ptr<k8sContainerInfo> info) {
    if (info == nullptr) {
        return false;
    }
    std::time_t now = std::time(0);
    std::chrono::system_clock::time_point th1 = std::chrono::system_clock::from_time_t(info->timestamp);
    std::chrono::system_clock::time_point th2 = std::chrono::system_clock::from_time_t(now);
    std::chrono::duration<double> diff = th2 - th1;
    double seconds_diff = diff.count();
    if (seconds_diff > 600) { // 10 minutes in seconds
        return true;
    }
    return false;
}

bool K8sMetadata::FromContainerJson(const Json::Value& json,
                                    std::shared_ptr<ContainerData> data,
                                    containerInfoType infoType) {
    if (!json.isObject()) {
        return false;
    }
    for (const auto& key : json.getMemberNames()) {
        k8sContainerInfo info;
        bool fromJsonIsOk = FromInfoJson(json[key], info);
        if (!fromJsonIsOk) {
            continue;
        }
        if (infoType == containerInfoType::HostInfo) {
            info.podIp = key;
        }
        data->containers[key] = info;
    }
    return true;
}

bool K8sMetadata::SendRequestToOperator(const std::string& urlHost,
                                        const std::string& query,
                                        containerInfoType infoType,
                                        std::vector<std::string>& resKey) {
    std::unique_ptr<HttpRequest> request;
    HttpResponse res;
    std::string path = "/metadata/containerid";
    if (infoType == containerInfoType::IpInfo) {
        path = "/metadata/ipport";
    } else if (infoType == containerInfoType::HostInfo) {
        path = "/metadata/host";
    }
    request = std::make_unique<HttpRequest>("GET",
                                            false,
                                            mServiceHost,
                                            mServicePort,
                                            path,
                                            "",
                                            map<std::string, std::string>({{"Content-Type", "application/json"}}),
                                            query,
                                            1,
                                            3);
    LOG_DEBUG(sLogger, ("host", mServiceHost)("port", mServicePort)("path", path)("query", query));
    bool success = SendHttpRequest(std::move(request), res);
    LOG_DEBUG(sLogger, ("res body", *res.GetBody<std::string>()));
    if (success) {
        if (res.GetStatusCode() != 200) {
            LOG_WARNING(sLogger, ("fetch k8s meta from one operator fail, code is ", res.GetStatusCode()));
            return false;
        }
        Json::CharReaderBuilder readerBuilder;
        std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
        Json::Value root;
        std::string errors;

        auto& responseBody = *res.GetBody<std::string>();
        if (reader->parse(responseBody.c_str(), responseBody.c_str() + responseBody.size(), &root, &errors)) {
            std::shared_ptr<ContainerData> data = std::make_shared<ContainerData>();
            if (data == nullptr) {
                return false;
            }
            if (!FromContainerJson(root, data, infoType)) {
                LOG_WARNING(sLogger, ("from container json error:", "SetIpCache"));
            } else {
                for (const auto& pair : data->containers) {
                    // record result
                    resKey.push_back(pair.first);
                    // update cache
                    if (infoType == containerInfoType::ContainerIdInfo) {
                        containerCache.insert(pair.first, std::make_shared<k8sContainerInfo>(pair.second));
                    } else {
                        ipCache.insert(pair.first, std::make_shared<k8sContainerInfo>(pair.second));
                    }
                }
            }
        } else {
            LOG_WARNING(sLogger, ("JSON parse error:", errors));
            return false;
        }

        return true;
    } else {
        LOG_WARNING(sLogger, ("fetch k8s meta from one operator fail", urlHost));
        return false;
    }
}

std::vector<std::string> K8sMetadata::GetByContainerIdsFromServer(std::vector<std::string>& containerIds,
                                                                  bool& status) {
    Json::Value jsonObj;
    for (auto& str : containerIds) {
        jsonObj["keys"].append(str);
    }
    std::vector<std::string> res;
    Json::StreamWriterBuilder writer;
    std::string output = Json::writeString(writer, jsonObj);
    status = SendRequestToOperator(mServiceHost, output, containerInfoType::ContainerIdInfo, res);
    return res;
}

bool K8sMetadata::GetByLocalHostFromServer(std::vector<std::string>& podIpVec) {
    Json::Value jsonObj;
    jsonObj["keys"].append(mHostIp);
    Json::StreamWriterBuilder writer;
    std::string output = Json::writeString(writer, jsonObj);
    return SendRequestToOperator(mServiceHost, output, containerInfoType::HostInfo, podIpVec);
}

bool K8sMetadata::GetByLocalHostFromServer() {
    std::vector<std::string> podIpVec;
    return GetByLocalHostFromServer(podIpVec);
}

void K8sMetadata::SetContainerCache(const Json::Value& root) {
    std::shared_ptr<ContainerData> data = std::make_shared<ContainerData>();
    if (data == nullptr) {
        return;
    }
    if (!FromContainerJson(root, data, containerInfoType::IpInfo)) {
        LOG_DEBUG(sLogger, ("from container json error:", "SetContainerCache"));
    } else {
        for (const auto& pair : data->containers) {
            containerCache.insert(pair.first, std::make_shared<k8sContainerInfo>(pair.second));
        }
    }
}

void K8sMetadata::SetIpCache(const Json::Value& root) {
    std::shared_ptr<ContainerData> data = std::make_shared<ContainerData>();
    if (data == nullptr) {
        return;
    }
    if (!FromContainerJson(root, data, containerInfoType::IpInfo)) {
        LOG_DEBUG(sLogger, ("from container json error:", "SetIpCache"));
    } else {
        for (const auto& pair : data->containers) {
            ipCache.insert(pair.first, std::make_shared<k8sContainerInfo>(pair.second));
        }
    }
}

void K8sMetadata::SetExternalIpCache(const std::string& ip) {
    LOG_DEBUG(sLogger, (ip, "is external, inset into cache ..."));
    externalIpCache.insert(ip, uint8_t(0));
}

std::vector<std::string> K8sMetadata::GetByIpsFromServer(std::vector<std::string>& ips, bool& status) {
    Json::Value jsonObj;
    for (auto& str : ips) {
        jsonObj["keys"].append(str);
    }
    std::vector<std::string> res;
    Json::StreamWriterBuilder writer;
    std::string output = Json::writeString(writer, jsonObj);
    status = SendRequestToOperator(mServiceHost, output, containerInfoType::IpInfo, res);
    std::set<std::string> hash;
    for (auto& ip : res) {
        hash.insert(ip);
    }
    for (auto& x : ips) {
        if (!hash.count(x)) {
            SetExternalIpCache(x);
        }
    }
    return res;
}

std::shared_ptr<k8sContainerInfo> K8sMetadata::GetInfoByContainerIdFromCache(const std::string& containerId) {
    if (containerId.empty()) {
        return nullptr;
    }
    if (containerCache.contains(containerId)) {
        return containerCache.get(containerId);
    } else {
        return nullptr;
    }
}

std::shared_ptr<k8sContainerInfo> K8sMetadata::GetInfoByIpFromCache(const std::string& ip) {
    if (ip.empty()) {
        return nullptr;
    }
    if (ipCache.contains(ip)) {
        return ipCache.get(ip);
    } else {
        return nullptr;
    }
}


bool K8sMetadata::IsExternalIp(const std::string& ip) const {
    return externalIpCache.contains(ip);
}

std::vector<std::shared_ptr<k8sContainerInfo>>
K8sMetadata::SyncGetPodMetadataByContainerIds(std::vector<std::string>& cids, bool& res) {
    std::vector<std::shared_ptr<k8sContainerInfo>> result(cids.size(), nullptr);
    std::vector<std::string> missingCids;

    for (size_t i = 0; i < cids.size(); i++) {
        auto info = GetInfoByContainerIdFromCache(cids[i]);
        if (info) {
            result[i] = info;
        } else {
            missingCids.push_back(cids[i]);
        }
    }

    if (!missingCids.empty()) {
        bool serverStatus;
        GetByContainerIdsFromServer(missingCids, serverStatus);

        if (!serverStatus) {
            res = false;
            return result;
        }

        for (size_t i = 0; i < result.size(); ++i) {
            if (result[i] == nullptr) {
                auto podMeta = K8sMetadata::GetInstance().GetInfoByContainerIdFromCache(cids[i]);
                result[i] = podMeta;
            }
        }
    }

    res = true;
    return result;
}

std::vector<std::shared_ptr<k8sContainerInfo>> K8sMetadata::SyncGetPodMetadataByIps(std::vector<std::string>& ips,
                                                                                    bool& res) {
    std::vector<std::shared_ptr<k8sContainerInfo>> result(ips.size(), nullptr);
    std::vector<std::string> missingIps;

    for (size_t i = 0; i < ips.size(); i++) {
        if (IsExternalIp(ips[i])) {
            result[i] = nullptr;
            LOG_DEBUG(sLogger, (ips[i], "external, set nullptr to metadata"));
            continue;
        }
        auto info = GetInfoByIpFromCache(ips[i]);
        if (info) {
            result[i] = info;
        } else {
            missingIps.push_back(ips[i]);
        }
    }

    if (!missingIps.empty()) {
        bool serverStatus;
        GetByIpsFromServer(missingIps, serverStatus);

        if (!serverStatus) {
            res = false;
            return result;
        }

        for (size_t i = 0; i < result.size(); ++i) {
            if (result[i] == nullptr) {
                auto podMeta = K8sMetadata::GetInstance().GetInfoByIpFromCache(ips[i]);
                result[i] = podMeta;
            }
        }
    }

    res = true;
    return result;
}

void K8sMetadata::AsyncQueryMetadata(containerInfoType type, const std::string& key) {
    switch (type) {
        case containerInfoType::ContainerIdInfo: {
            break;
        }
        case containerInfoType::IpInfo: {
            break;
        }
        default:
            break;
    }
}


} // namespace logtail

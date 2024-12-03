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
#include <thread>

#include "common/MachineInfoUtil.h"
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

bool K8sMetadata::FromInfoJson(const Json::Value& json, k8sContainerInfo& info) {
    if (!json.isMember(TAG_IMAGES) || !json.isMember(TAG_LABELS) || !json.isMember(TAG_NAMESPACE)
        || !json.isMember(TAG_WORKLOADKIND) || !json.isMember(TAG_WORKLOADNAME)) {
        return false;
    }

    for (const auto& key : json[TAG_IMAGES].getMemberNames()) {
        if (json[TAG_IMAGES].isMember(key)) {
            info.images[key] = json[TAG_IMAGES][key].asString();
        }
    }
    for (const auto& key : json[TAG_LABELS].getMemberNames()) {
        if (json[TAG_LABELS].isMember(key)) {
            info.labels[key] = json[TAG_LABELS][key].asString();

            if (key == DEFAULT_TRACE_TAG_APPID) {
                info.appId = json[TAG_LABELS][key].asString();
            }
        }
    }

    info.k8sNamespace = json[TAG_NAMESPACE].asString();
    if (json.isMember(TAG_SERVICENAME)) {
        info.serviceName = json[TAG_SERVICENAME].asString();
    }
    info.workloadKind = json[TAG_WORKLOADKIND].asString();
    info.workloadName = json[TAG_WORKLOADNAME].asString();
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

bool K8sMetadata::FromContainerJson(const Json::Value& json, std::shared_ptr<ContainerData> data) {
    if (!json.isObject()) {
        return false;
    }
    for (const auto& key : json.getMemberNames()) {
        k8sContainerInfo info;
        bool fromJsonIsOk = FromInfoJson(json[key], info);
        if (!fromJsonIsOk) {
            continue;
        }
        data->containers[key] = info;
    }
    return true;
}

bool K8sMetadata::SendRequestToOperator(const std::string& urlHost,
                                        const std::string& output,
                                        containerInfoType infoType) {
    std::unique_ptr<HttpRequest> request;
    HttpResponse res;
    std::string path = "/metadata/containerid";
    if (infoType == containerInfoType::IpInfo) {
        path = "/metadata/ip";
    }
    request = std::make_unique<HttpRequest>(
        "GET", false, mServiceHost, mServicePort, path, "", map<std::string, std::string>(), output, 30, 3);
    bool success = SendHttpRequest(std::move(request), res);
    if (success) {
        if (res.GetStatusCode() != 200) {
            LOG_DEBUG(sLogger, ("fetch k8s meta from one operator fail, code is ", res.GetStatusCode()));
            return false;
        }
        Json::CharReaderBuilder readerBuilder;
        Json::CharReader* reader = readerBuilder.newCharReader();
        Json::Value root;
        std::string errors;

        auto& responseBody = *res.GetBody<std::string>();
        if (reader->parse(responseBody.c_str(), responseBody.c_str() + responseBody.size(), &root, &errors)) {
            if (infoType == containerInfoType::ContainerIdInfo) {
                SetContainerCache(root);
            } else {
                SetIpCache(root);
            }
        } else {
            LOG_DEBUG(sLogger, ("JSON parse error:", errors));
            delete reader;
            return false;
        }

        delete reader;
        return true;
    } else {
        LOG_DEBUG(sLogger, ("fetch k8s meta from one operator fail", urlHost));
        return false;
    }
}

bool K8sMetadata::GetByContainerIdsFromServer(std::vector<std::string> containerIds) {
    Json::Value jsonObj;
    for (auto& str : containerIds) {
        jsonObj["keys"].append(str);
    }
    Json::StreamWriterBuilder writer;
    std::string output = Json::writeString(writer, jsonObj);
    return SendRequestToOperator(mServiceHost, output, containerInfoType::ContainerIdInfo);
}

void K8sMetadata::GetByLocalHostFromServer() {
    std::string hostIp = GetHostIp();
    std::list<std::string> strList{hostIp};
    Json::Value jsonObj;
    for (const auto& str : strList) {
        jsonObj["keys"].append(str);
    }
    Json::StreamWriterBuilder writer;
    std::string output = Json::writeString(writer, jsonObj);
    std::string urlHost = mServiceHost;
    SendRequestToOperator(urlHost, output, containerInfoType::ContainerIdInfo);
    SendRequestToOperator(urlHost, output, containerInfoType::IpInfo);
}

void K8sMetadata::SetContainerCache(const Json::Value& root) {
    std::shared_ptr<ContainerData> data = std::make_shared<ContainerData>();
    if (data == nullptr) {
        return;
    }
    if (!FromContainerJson(root, data)) {
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
    if (!FromContainerJson(root, data)) {
        LOG_DEBUG(sLogger, ("from container json error:", "SetIpCache"));
    } else {
        for (const auto& pair : data->containers) {
            ipCache.insert(pair.first, std::make_shared<k8sContainerInfo>(pair.second));
        }
    }
}

bool K8sMetadata::GetByIpsFromServer(std::vector<std::string> ips) {
    Json::Value jsonObj;
    for (auto& str : ips) {
        jsonObj["keys"].append(str);
    }
    Json::StreamWriterBuilder writer;
    std::string output = Json::writeString(writer, jsonObj);
    std::string urlHost = mServiceHost;
    return SendRequestToOperator(urlHost, output, containerInfoType::IpInfo);
}

std::shared_ptr<k8sContainerInfo> K8sMetadata::GetInfoByContainerIdFromCache(const std::string& containerId) {
    if (containerId.empty()) {
        return nullptr;
    }
    return containerCache.get(containerId);
}

std::shared_ptr<k8sContainerInfo> K8sMetadata::GetInfoByIpFromCache(const std::string& ip) {
    if (ip.empty()) {
        return nullptr;
    }
    std::shared_ptr<k8sContainerInfo> ip_info = ipCache.get(ip);
    if (ip_info == nullptr) {
        return nullptr;
    }
    if (ContainerInfoIsExpired(ip_info)) {
        return nullptr;
    }
    return ip_info;
}

} // namespace logtail

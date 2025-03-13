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

#include <ctime>

#include <chrono>
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

const std::string CONTAINER_ID_METADATA_PATH = "/metadata/containerid";
const std::string HOST_METADATAPATH = "/metadata/host";
const std::string IP_METADATA_PATH = "/metadata/ipport";

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

bool K8sMetadata::Enable() {
#ifdef APSARA_UNIT_TEST_MAIN
    return true;
#endif
    return mEnable;
}

K8sMetadata::~K8sMetadata() {
    if (!mEnable) {
        return;
    }

    mFlag = false;
    mCv.notify_all();
    mNetDetectorCv.notify_all();

    if (mQueryThread.joinable()) {
        mQueryThread.join();
    }

    if (mNetDetector.joinable()) {
        mNetDetector.join();
    }
}

K8sMetadata::K8sMetadata(size_t ipCacheSize, size_t cidCacheSize, size_t externalIpCacheSize)
    : mIpCache(ipCacheSize, 20), mContainerCache(cidCacheSize, 20), mExternalIpCache(externalIpCacheSize, 20) {
    mServiceHost = STRING_FLAG(singleton_service);
    mServicePort = INT32_FLAG(singleton_port);
    const char* value = getenv("_node_ip_");
    if (value != NULL) {
        mHostIp = StringTo<string>(value);
    } else {
        mHostIp = GetHostIp();
    }
#ifdef APSARA_UNIT_TEST_MAIN
    mServiceHost = "47.95.70.43";
    mServicePort = 8899;
    mHostIp = "172.16.57.207";
    mEnable = true;
#else
    mEnable = getenv("KUBERNETES_SERVICE_HOST") && AppConfig::GetInstance()->IsPurageContainerMode()
        && mServiceHost.size() && mServicePort > 0;
    LOG_INFO(sLogger,
             ("k8smetadata enable status", mEnable)("host ip", mHostIp)("serviceHost", mServiceHost)("servicePort",
                                                                                                     mServicePort));
#endif

    // batch query metadata ...
    if (mEnable) {
        mFlag = true;
        mNetDetector = std::thread(&K8sMetadata::DetectNetwork, this);
        mQueryThread = std::thread(&K8sMetadata::ProcessBatch, this);
    }
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
        data->containers[key] = info;
    }
    return true;
}

std::unique_ptr<HttpRequest>
K8sMetadata::BuildRequest(const std::string& path, const std::string& reqBody, uint32_t timeout, uint32_t maxTryCnt) {
    return std::make_unique<HttpRequest>("GET",
                                         false,
                                         mServiceHost,
                                         mServicePort,
                                         path,
                                         "",
                                         map<std::string, std::string>({{"Content-Type", "application/json"}}),
                                         reqBody,
                                         timeout,
                                         maxTryCnt);
}

void K8sMetadata::UpdateStatus(bool status) {
    if (status) {
        mFailCount = 0;
        mIsValid = true;
    } else if (++mFailCount > 5 && mIsValid) {
        mIsValid = false;
        mNetDetectorCv.notify_one();
    }
}

bool K8sMetadata::SendRequestToOperator(const std::string& urlHost,
                                        const std::string& query,
                                        containerInfoType infoType,
                                        std::vector<std::string>& resKey) {
    if (!mIsValid) {
        LOG_DEBUG(sLogger, ("remote status invalid", "skip query"));
        return false;
    }
    HttpResponse res;
    std::string path = CONTAINER_ID_METADATA_PATH;
    if (infoType == containerInfoType::IpInfo) {
        path = IP_METADATA_PATH;
    } else if (infoType == containerInfoType::HostInfo) {
        path = HOST_METADATAPATH;
    }
    auto request = BuildRequest(path, query);
    LOG_DEBUG(sLogger, ("host", mServiceHost)("port", mServicePort)("path", path)("query", query));
#ifdef APSARA_UNIT_TEST_MAIN
    mRequest = request.get();
    bool success = false;
#else
    bool success = SendHttpRequest(std::move(request), res);
#endif
    LOG_DEBUG(sLogger, ("res body", *res.GetBody<std::string>()));
    if (success) {
        if (res.GetStatusCode() != 200) {
            UpdateStatus(false);
            LOG_WARNING(sLogger, ("fetch k8s meta from one operator fail, code is ", res.GetStatusCode()));
            return false;
        }
        UpdateStatus(true);
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
                HandleMetadataResponse(infoType, data, resKey);
            }
        } else {
            LOG_WARNING(sLogger, ("JSON parse error:", errors));
            return false;
        }

        return true;
    } else {
        UpdateStatus(false);
        LOG_WARNING(sLogger, ("fetch k8s meta from one operator fail", urlHost));
        return false;
    }
}

void K8sMetadata::HandleMetadataResponse(containerInfoType infoType,
                                         const std::shared_ptr<ContainerData>& data,
                                         std::vector<std::string>& resKey) {
    for (const auto& pair : data->containers) {
        // update cache
        auto info = std::make_shared<k8sContainerInfo>(pair.second);
        if (infoType == containerInfoType::ContainerIdInfo) {
            // record result
            resKey.push_back(pair.first);
            SetContainerCache(pair.first, info);
        } else if (infoType == containerInfoType::IpInfo) {
            // record result
            resKey.push_back(pair.first);
            SetIpCache(pair.first, info);
        } else {
            // set ip cache
            SetIpCache(info->podIp, info);
            // set containerid cache
            for (const auto& cid : info->containerIds) {
                // record result
                resKey.push_back(cid);
                SetContainerCache(cid, info);
            }
        }
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
    std::string reqBody = Json::writeString(writer, jsonObj);
    status = SendRequestToOperator(mServiceHost, reqBody, containerInfoType::ContainerIdInfo, res);
    return res;
}

bool K8sMetadata::GetByLocalHostFromServer(std::vector<std::string>& podIpVec) {
    Json::Value jsonObj;
    jsonObj["keys"].append(mHostIp);
    Json::StreamWriterBuilder writer;
    std::string reqBody = Json::writeString(writer, jsonObj);
    return SendRequestToOperator(mServiceHost, reqBody, containerInfoType::HostInfo, podIpVec);
}

bool K8sMetadata::GetByLocalHostFromServer() {
    std::vector<std::string> podIpVec;
    return GetByLocalHostFromServer(podIpVec);
}

void K8sMetadata::SetContainerCache(const std::string& key, const std::shared_ptr<k8sContainerInfo>& info) {
    mContainerCache.insert(key, info);
}

void K8sMetadata::SetIpCache(const std::string& key, const std::shared_ptr<k8sContainerInfo>& info) {
    mIpCache.insert(key, info);
}

void K8sMetadata::SetExternalIpCache(const std::string& ip) {
    LOG_DEBUG(sLogger, (ip, "is external, inset into cache ..."));
    mExternalIpCache.insert(ip, uint8_t(0));
}

void K8sMetadata::UpdateExternalIpCache(const std::vector<std::string>& queryIps,
                                        const std::vector<std::string>& retIps) {
    std::set<std::string> hash;
    for (auto& ip : retIps) {
        hash.insert(ip);
    }
    for (auto& x : queryIps) {
        if (!hash.count(x)) {
            LOG_DEBUG(sLogger, (x, "mark as external ip"));
            SetExternalIpCache(x);
        }
    }
}

std::vector<std::string> K8sMetadata::GetByIpsFromServer(std::vector<std::string>& ips, bool& status) {
    Json::Value jsonObj;
    for (auto& str : ips) {
        jsonObj["keys"].append(str);
    }
    std::vector<std::string> res;
    Json::StreamWriterBuilder writer;
    std::string reqBody = Json::writeString(writer, jsonObj);
    LOG_DEBUG(sLogger, ("reqBody", reqBody));
    status = SendRequestToOperator(mServiceHost, reqBody, containerInfoType::IpInfo, res);
    if (status) {
        UpdateExternalIpCache(ips, res);
    }
    return res;
}

std::shared_ptr<k8sContainerInfo> K8sMetadata::GetInfoByContainerIdFromCache(const std::string& containerId) {
    if (containerId.empty()) {
        return nullptr;
    }
    if (mContainerCache.contains(containerId)) {
        return mContainerCache.get(containerId);
    } else {
        return nullptr;
    }
}

std::shared_ptr<k8sContainerInfo> K8sMetadata::GetInfoByIpFromCache(const std::string& ip) {
    if (ip.empty()) {
        return nullptr;
    }
    if (mIpCache.contains(ip)) {
        return mIpCache.get(ip);
    } else {
        return nullptr;
    }
}

bool K8sMetadata::IsExternalIp(const std::string& ip) const {
    return mExternalIpCache.contains(ip);
}

void K8sMetadata::AsyncQueryMetadata(containerInfoType type, const std::string& key) {
    if (key.empty()) {
        LOG_DEBUG(sLogger, ("empty key", ""));
        return;
    }
    std::unique_lock<std::mutex> lock(mStateMux);
    if (mPendingKeys.find(key) != mPendingKeys.end()) {
        // already in query queue ...
        return;
    }
    mPendingKeys.insert(key);
    if (type == containerInfoType::IpInfo) {
        mBatchKeys.push_back(key);
    } else if (type == containerInfoType::ContainerIdInfo) {
        mBatchCids.push_back(key);
    }
}

const static std::string LOCALHOST_IP = "127.0.0.1";

void K8sMetadata::DetectMetadataServer() {
    std::vector<std::string> ips = {LOCALHOST_IP};
    bool status = false;
    GetByIpsFromServer(ips, status);
    LOG_DEBUG(sLogger, ("detect network, res", status));
    return;
}

void K8sMetadata::DetectNetwork() {
    LOG_INFO(sLogger, ("begin to start k8smetadata network detector", ""));
    std::unique_lock<std::mutex> lock(mNetDetectorMtx);
    while (mFlag) {
        // detect network every seconds
        mNetDetectorCv.wait_for(lock, chrono::seconds(1));
        if (!mFlag) {
            return;
        }
        if (mIsValid) {
            continue;
        }
        // detect network
        DetectMetadataServer();
    }
    LOG_INFO(sLogger, ("stop k8smetadata network detector", ""));
}

void K8sMetadata::ProcessBatch() {
    auto batchProcessor = [this](auto&& processFunc,
                                 std::vector<std::string>& srcItems,
                                 std::vector<std::string>& pendingItems,
                                 std::unordered_set<std::string>& pendingSet) {
        if (!srcItems.empty()) {
            bool status = false;
            if (mIsValid) {
                processFunc(srcItems, status);
            }

            std::unique_lock<std::mutex> lock(mStateMux);
            if (!status) {
                for (const auto& item : srcItems) {
                    if (!item.empty()) {
                        pendingItems.emplace_back(item);
                    }
                }
            } else {
                for (const auto& item : srcItems) {
                    pendingSet.erase(item);
                }
            }
        }
    };

    while (mFlag) {
        std::vector<std::string> keysToProcess;
        std::vector<std::string> cidKeysToProcess;
        {
            std::unique_lock<std::mutex> lock(mStateMux);
            // merge requests in 100ms
            mCv.wait_for(lock, chrono::milliseconds(100));
            if (!mFlag) {
                break;
            }
            if (!mIsValid || (mBatchKeys.empty() && mBatchCids.empty())) {
                continue;
            }
            keysToProcess.swap(mBatchKeys);
            cidKeysToProcess.swap(mBatchCids);
        }

        batchProcessor([this](auto&& items, bool& status) { GetByIpsFromServer(items, status); },
                       keysToProcess,
                       mBatchKeys,
                       mPendingKeys);

        batchProcessor([this](auto&& items, bool& status) { GetByContainerIdsFromServer(items, status); },
                       cidKeysToProcess,
                       mBatchCids,
                       mPendingKeys);
    }
}

} // namespace logtail

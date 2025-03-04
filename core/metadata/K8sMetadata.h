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
#pragma once

#include <atomic>
#include <future>
#include <iostream>
#include <mutex>
#include <string>

#include "json/json.h"

#include "ContainerInfo.h"
#include "app_config/AppConfig.h"
#include "common/Flags.h"
#include "common/LRUCache.h"
#include "common/Lock.h"
#include "common/http/HttpRequest.h"

DECLARE_FLAG_STRING(singleton_service);
DECLARE_FLAG_INT32(singleton_port);

namespace logtail {

const static std::string appIdKey = "armseBPFAppId";
const static std::string appNameKey = "armseBPFCreateAppName";
const static std::string imageKey = "images";
const static std::string labelsKey = "labels";
const static std::string namespaceKey = "namespace";
const static std::string workloadKindKey = "workloadKind";
const static std::string workloadNameKey = "workloadName";
const static std::string serviceNameKey = "serviceName";
const static std::string podNameKey = "podName";
const static std::string podIpKey = "podIP";
const static std::string envKey = "envs";
const static std::string containerIdKey = "containerIDs";
const static std::string startTimeKey = "startTime";

struct ContainerData {
    std::unordered_map<std::string, k8sContainerInfo> containers;
};

enum class containerInfoType {
    ContainerIdInfo,
    IpInfo,
    HostInfo,
};

using HostMetadataPostHandler = std::function<bool(uint32_t pluginIndex, std::vector<std::string>& containerIds)>;

class K8sMetadata {
private:
    lru11::Cache<std::string, std::shared_ptr<k8sContainerInfo>, std::mutex> mIpCache;
    lru11::Cache<std::string, std::shared_ptr<k8sContainerInfo>, std::mutex> mContainerCache;
    lru11::Cache<std::string, uint8_t, std::mutex> mExternalIpCache;

    std::string mServiceHost;
    int32_t mServicePort;
    std::string mHostIp;

    void ProcessBatch();

    mutable std::mutex mStateMux;
    std::unordered_set<std::string> mPendingKeys;

    mutable std::condition_variable mCv;
    std::vector<std::string> mBatchKeys;
    std::vector<std::string> mBatchCids;
    std::atomic_bool mEnable = false;
    bool mFlag = false;
    std::thread mQueryThread;
    std::atomic_bool mIsValid = true;
    std::atomic_int mFailCount = 0;

    std::mutex mNetDetectorMtx;
    mutable std::condition_variable mNetDetectorCv;
    std::thread mNetDetector;

    K8sMetadata(size_t ipCacheSize = 1024, size_t cidCacheSize = 1024, size_t externalIpCacheSize = 1024);
    K8sMetadata(const K8sMetadata&) = delete;
    K8sMetadata& operator=(const K8sMetadata&) = delete;

    void UpdateStatus(bool status);
    void DetectMetadataServer();

    std::unique_ptr<HttpRequest>
    BuildRequest(const std::string& path, const std::string& reqBody, uint32_t timeout = 1, uint32_t maxTryCnt = 3);
    void DetectNetwork();
    void SetIpCache(const std::string& key, const std::shared_ptr<k8sContainerInfo>& info);
    void SetContainerCache(const std::string& key, const std::shared_ptr<k8sContainerInfo>& info);
    void SetExternalIpCache(const std::string&);
    void UpdateExternalIpCache(const std::vector<std::string>& queryIps, const std::vector<std::string>& retIps);
    bool FromInfoJson(const Json::Value& json, k8sContainerInfo& info);
    bool FromContainerJson(const Json::Value& json, std::shared_ptr<ContainerData> data, containerInfoType infoType);
    void HandleMetadataResponse(containerInfoType infoType,
                                const std::shared_ptr<ContainerData>& data,
                                std::vector<std::string>& resKey);

public:
    static K8sMetadata& GetInstance() {
        static K8sMetadata instance(1024, 1024, 1024);
        return instance;
    }
    ~K8sMetadata();

    bool Enable();

    // if cache not have,get from server
    std::vector<std::string> GetByContainerIdsFromServer(std::vector<std::string>& containerIds, bool& status);
    // get pod metadatas for local host
    bool GetByLocalHostFromServer();
    bool GetByLocalHostFromServer(std::vector<std::string>& podIpVec);
    //
    std::vector<std::string> GetByIpsFromServer(std::vector<std::string>& ips, bool& status);
    // get info by container id from cache
    std::shared_ptr<k8sContainerInfo> GetInfoByContainerIdFromCache(const std::string& containerId);
    // get info by ip from cache
    std::shared_ptr<k8sContainerInfo> GetInfoByIpFromCache(const std::string& ip);
    bool IsExternalIp(const std::string& ip) const;
    bool SendRequestToOperator(const std::string& urlHost,
                               const std::string& request,
                               containerInfoType infoType,
                               std::vector<std::string>& resKey);

    void AsyncQueryMetadata(containerInfoType type, const std::string& key);

#ifdef APSARA_UNIT_TEST_MAIN
    HttpRequest* mRequest;
    friend class k8sMetadataUnittest;
    friend class ConnectionUnittest;
    friend class ConnectionManagerUnittest;
#endif
};
} // namespace logtail

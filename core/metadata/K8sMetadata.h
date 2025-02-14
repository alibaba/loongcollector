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

#include <iostream>
#include <string>

#include <atomic>
#include <mutex>
#include <future>

#include "json/json.h"

#include "ContainerInfo.h"
#include "app_config/AppConfig.h"
#include "common/Flags.h"
#include "common/LRUCache.h"

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

// 定义顶层的结构体
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
    lru11::Cache<std::string, std::shared_ptr<k8sContainerInfo>, std::mutex> ipCache;
    lru11::Cache<std::string, std::shared_ptr<k8sContainerInfo>, std::mutex> containerCache;
    lru11::Cache<std::string, uint8_t, std::mutex> externalIpCache;
    std::string mServiceHost;
    int32_t mServicePort;
    std::string mHostIp;

    // mPeriodicalRunner will periodically fetch local host metadata.
    std::thread mPeriodicalRunner;
    std::atomic_bool mFlag;
    int32_t mFetchIntervalSeconds;
    std::mutex mMtx;
    std::map<uint32_t, HostMetadataPostHandler> mHostMetaCallback;

    K8sMetadata(size_t ipCacheSize = 1024, size_t cidCacheSize = 1024, size_t externalIpCacheSize = 1024, int32_t fetchIntervalSec = 5);
    K8sMetadata(const K8sMetadata&) = delete;
    K8sMetadata& operator=(const K8sMetadata&) = delete;

    void SetIpCache(const Json::Value& root);
    void SetContainerCache(const Json::Value& root);
    void SetExternalIpCache(const std::string&);
    bool FromInfoJson(const Json::Value& json, k8sContainerInfo& info);
    bool FromContainerJson(const Json::Value& json, std::shared_ptr<ContainerData> data, containerInfoType infoType);
    void LocalHostMetaRefresher();

public:

    static K8sMetadata& GetInstance() {
        static K8sMetadata instance(1024, 1024, 1024, 5);
        return instance;
    }
    ~K8sMetadata() {

    }

    bool Enable();

    void StartFetchHostMetadata();
    void StopFetchHostMetadata();

    void ResiterHostMetadataCallback(uint32_t plugin_index, HostMetadataPostHandler&& callback);
    void DeregisterHostMetadataCallback(uint32_t plugin_index);
    // 公共方法
    // if cache not have,get from server
    std::vector<std::string> GetByContainerIdsFromServer(std::vector<std::string>& containerIds, bool& status);
    // get pod metadatas for local host 
    bool GetByLocalHostFromServer();
    // 
    std::vector<std::string> GetByIpsFromServer(std::vector<std::string>& ips, bool& status);
    // get info by container id from cache
    std::shared_ptr<k8sContainerInfo> GetInfoByContainerIdFromCache(const std::string& containerId);
    // get info by ip from cache
    std::shared_ptr<k8sContainerInfo> GetInfoByIpFromCache(const std::string& ip);
    bool IsExternalIp(const std::string& ip) const;
    bool SendRequestToOperator(const std::string& urlHost, const std::string& request, containerInfoType infoType, std::vector<std::string>& resKey);

    // SyncGetPodMetadataByContainerIds
    // if container info is not present in local cache, we will fetch it from remote server
    std::vector<std::shared_ptr<k8sContainerInfo>> SyncGetPodMetadataByContainerIds(std::vector<std::string>&, bool& res);
    // SyncGetPodMetadataByIps 
    // if container info is not present in local cache, we will fetch it from remote server
    std::vector<std::shared_ptr<k8sContainerInfo>> SyncGetPodMetadataByIps(std::vector<std::string>&, bool& res);

    std::future<std::vector<std::shared_ptr<k8sContainerInfo>>> AsyncGetPodMetadataByIps(std::vector<std::string>& ips);
    std::future<std::vector<std::shared_ptr<k8sContainerInfo>>> AsyncGetPodMetadataByContainerIds(std::vector<std::string>& containerIds);

#ifdef APSARA_UNIT_TEST_MAIN
    friend class k8sMetadataUnittest;
#endif
};
} // namespace logtail

/*
 * Copyright 2024 iLogtail Authors
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

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <unordered_set>

#include "json/value.h"

#include "ContainerInfo.h"
#include "common/Flags.h"
#include "common/LRUCache.h"
#include "common/NetworkUtil.h"
#include "common/StringView.h"
#include "common/http/HttpRequest.h"
#include "monitor/metric_models/MetricRecord.h"
#include "monitor/metric_models/MetricTypes.h"

DECLARE_FLAG_STRING(k8s_metadata_server_name);
DECLARE_FLAG_INT32(k8s_metadata_server_port);

namespace logtail {

struct ContainerData {
    std::unordered_map<std::string, K8sPodInfo> containers;
};

enum class PodInfoType {
    ContainerIdInfo,
    IpInfo,
    HostInfo,
};

using HostMetadataPostHandler = std::function<bool(uint32_t pluginIndex, std::vector<std::string>& containerIds)>;

struct K8sMetadataHttpRequest;

class K8sMetadata {
private:
    lru11::Cache<std::string, std::shared_ptr<K8sPodInfo>, std::mutex> mIpCache;
    lru11::Cache<std::string, std::shared_ptr<K8sPodInfo>, std::mutex> mContainerCache;
    lru11::Cache<std::string, uint8_t, std::mutex> mExternalIpCache;

    std::string mServiceHost;
    int32_t mServicePort;
    std::string mHostIp;

    MetricsRecordRef mRef; // for self monitor
    IntGaugePtr mCidCacheSize;
    IntGaugePtr mIpCacheSize;
    IntGaugePtr mExternalIpCacheSize;
    CounterPtr mRequestMetaServerTotal;
    CounterPtr mRequestMetaServerFailedTotal;

    void ProcessBatch();

    mutable std::mutex mStateMux;
    std::unordered_set<std::string> mPendingKeys; // 增加上限

    mutable std::condition_variable mCv;
    std::vector<std::string> mBatchKeys; // 增加上限
    std::vector<std::string> mBatchCids; // 增加上限
    std::atomic_bool mEnable = false;
    bool mFlag = false;
    std::thread mQueryThread;
    std::atomic_bool mIsValid = true;
    std::atomic_int mFailCount = 0;

    std::mutex mNetDetectorMtx;
    mutable std::condition_variable mNetDetectorCv;
    std::thread mNetDetector;

    std::vector<CIDR> mClusterCIDRs;

    K8sMetadata(size_t ipCacheSize = 1024, size_t cidCacheSize = 1024, size_t externalIpCacheSize = 1024);
    K8sMetadata(const K8sMetadata&) = delete;
    K8sMetadata& operator=(const K8sMetadata&) = delete;

    void UpdateStatus(bool status);
    void DetectMetadataServer();

    std::unique_ptr<HttpRequest>
    BuildRequest(const std::string* path, const std::string& reqBody, uint32_t timeout = 1, uint32_t maxTryCnt = 3);
    void DetectNetwork();
    void SetIpCache(const std::string& key, const std::shared_ptr<K8sPodInfo>& info);
    void SetContainerCache(const std::string& key, const std::shared_ptr<K8sPodInfo>& info);
    void SetExternalIpCache(const std::string&);
    void UpdateExternalIpCache(const std::vector<std::string>& queryIps, const std::vector<std::string>& retIps);
    bool FromInfoJson(const Json::Value& json, K8sPodInfo& info);
    bool FromContainerJson(const Json::Value& json, std::shared_ptr<ContainerData> data, PodInfoType infoType);
    void HandleMetadataResponse(PodInfoType infoType,
                                const std::shared_ptr<ContainerData>& data,
                                std::vector<std::string>& resKey);
    bool HandleResponse(HttpResponse& res, PodInfoType infoType, std::vector<std::string>& resKey);

public:
    static K8sMetadata& GetInstance() {
        static K8sMetadata sInstance(1024, 1024, 1024);
        return sInstance;
    }
    ~K8sMetadata();

    bool Enable();

    const std::string& GetHostIp() const;

    // if cache not have,get from server
    std::vector<std::string> GetByContainerIdsFromServer(std::vector<std::string>& containerIds, bool& status);
    // get pod metadatas for local host
    bool GetByLocalHostFromServer();
    bool GetByLocalHostFromServer(std::vector<std::string>& podIpVec);

    std::vector<std::string> GetByIpsFromServer(std::vector<std::string>& ips, bool& status, bool force = false);
    // get info by container id from cache
    std::shared_ptr<K8sPodInfo> GetInfoByContainerIdFromCache(const StringView& containerId);
    // get info by ip from cache
    std::shared_ptr<K8sPodInfo> GetInfoByIpFromCache(const StringView& ip);
    bool IsExternalIp(const StringView& ip) const;
    bool IsClusterIpForIPv4(uint32_t ip) const;
    bool SendRequestToOperator(const std::string& urlHost,
                               const std::string& request,
                               PodInfoType infoType,
                               std::vector<std::string>& resKey,
                               bool force = false);

    void AsyncQueryMetadata(PodInfoType type, const StringView& key);

    std::unique_ptr<K8sMetadataHttpRequest>
    BuildAsyncRequest(std::vector<std::string>& keys,
                      PodInfoType infoType,
                      std::function<bool()> validator,
                      std::function<void(const std::vector<std::string>&)> postProcessor = nullptr,
                      uint32_t timeoutSeconds = 1,
                      uint32_t retryTimes = 3);

    friend class K8sMetadataHttpRequest;
#ifdef APSARA_UNIT_TEST_MAIN
    HttpRequest* mRequest;
    friend class k8sMetadataUnittest;
    friend class ConnectionUnittest;
    friend class ConnectionManagerUnittest;
    friend class NetworkObserverManagerUnittest;
#endif
};

struct K8sMetadataHttpRequest : public AsynHttpRequest {
    K8sMetadataHttpRequest(const std::string& method,
                           bool httpsFlag,
                           const std::string& host,
                           int32_t port,
                           const std::string& url,
                           const std::string& query,
                           const std::map<std::string, std::string>& header,
                           const std::string& body,
                           uint32_t timeout,
                           uint32_t maxTryCnt,
                           PodInfoType infoType,
                           std::function<bool()> validator,
                           std::function<void(const std::vector<std::string>&)> postProcessor = nullptr)
        : AsynHttpRequest(method, httpsFlag, host, port, url, query, header, body, HttpResponse(), timeout, maxTryCnt),
          mInfoType(infoType),
          mValidator(validator),
          mPostProcessor(postProcessor) {}

    bool IsContextValid() const override {
        if (mValidator == nullptr) {
            return true;
        }
        return mValidator();
    };

    void OnSendDone(HttpResponse& response) {
        std::vector<std::string> podIps;
        bool status = K8sMetadata::GetInstance().HandleResponse(response, mInfoType, podIps);
        if (!status) {
            return;
        }
        // post process ...
        if (mPostProcessor != nullptr) {
            mPostProcessor(podIps);
        }
    };

    PodInfoType mInfoType;
    std::function<bool()> mValidator = nullptr;
    std::function<void(const std::vector<std::string>&)> mPostProcessor = nullptr;
};

} // namespace logtail

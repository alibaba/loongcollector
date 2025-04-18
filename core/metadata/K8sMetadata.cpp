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

#include "K8sMetadata.h"

#include <ctime>

#include <chrono>
#include <future>
#include <thread>

#include "app_config/AppConfig.h"
#include "common/MachineInfoUtil.h"
#include "common/NetworkUtil.h"
#include "common/StringTools.h"
#include "common/StringView.h"
#include "common/http/Curl.h"
#include "common/http/HttpRequest.h"
#include "common/http/HttpResponse.h"
#include "logger/Logger.h"
#include "monitor/metric_models/ReentrantMetricsRecord.h"

using namespace std;

DEFINE_FLAG_STRING(ipv4_cluster_cidrs, "cluster cidr", "");
DEFINE_FLAG_BOOL(disable_k8s_meta, "disable k8s metadata", false);

namespace logtail {

const std::string CONTAINER_ID_METADATA_PATH = "/metadata/containerid";
const std::string HOST_METADATAPATH = "/metadata/host";
const std::string IP_METADATA_PATH = "/metadata/ipport";

static const std::string kAppIdKey = "armseBPFAppId";
static const std::string kAppNameKey = "armseBPFCreateAppName";
static const std::string kImagesKey = "images";
static const std::string kLabelsKey = "labels";
static const std::string kNamespaceKey = "namespace";
static const std::string kWorkloadKindKey = "workloadKind";
static const std::string kWorkloadNameKey = "workloadName";
static const std::string kServiceNameKey = "serviceName";
static const std::string kPodNameKey = "podName";
static const std::string kPodIpKey = "podIP";
static const std::string kEnvKey = "envs";
static const std::string kContainerIdKey = "containerIDs";
static const std::string kStartTimeKey = "startTime";

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
    mServiceHost = STRING_FLAG(k8s_metadata_server_name);
    mServicePort = INT32_FLAG(k8s_metadata_server_port);
    const char* value = getenv("_node_ip_");
    if (value != nullptr) {
        mHostIp = value;
    } else {
        mHostIp = GetHostIp();
    }
    // parse CIDR blocks
    if (STRING_FLAG(ipv4_cluster_cidrs).size()) {
        auto cidrs = StringSpliter(STRING_FLAG(ipv4_cluster_cidrs), ",");
        for (auto& cidrStr : cidrs) {
            CIDR cidr;
            if (ParseCIDR(cidrStr, &cidr)) {
                mClusterCIDRs.push_back(cidr);
            }
        }
    }

#ifdef APSARA_UNIT_TEST_MAIN
    mEnable = !BOOL_FLAG(disable_k8s_meta);
#else
    mEnable = getenv("KUBERNETES_SERVICE_HOST") && AppConfig::GetInstance()->IsPurageContainerMode()
        && mServiceHost.size() && mServicePort > 0 && !BOOL_FLAG(disable_k8s_meta);
    LOG_INFO(sLogger,
             ("k8smetadata enable status", mEnable)("disable flag", BOOL_FLAG(disable_k8s_meta))("host ip", mHostIp)(
                 "serviceHost", mServiceHost)("servicePort", mServicePort));
#endif

    // self monitor
    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
        mRef,
        MetricCategory::METRIC_CATEGORY_RUNNER,
        {{METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_K8S_METADATA}});

    mCidCacheSize = mRef.CreateIntGauge(METRIC_RUNNER_METADATA_CID_CACHE_SIZE);
    mIpCacheSize = mRef.CreateIntGauge(METRIC_RUNNER_METADATA_IP_CACHE_SIZE);
    mExternalIpCacheSize = mRef.CreateIntGauge(METRIC_RUNNER_METADATA_EXTERNAL_IP_CACHE_SIZE);
    mRequestMetaServerTotal = mRef.CreateCounter(METRIC_RUNNER_METADATA_REQUEST_REMOTE_TOTAL);
    mRequestMetaServerFailedTotal = mRef.CreateCounter(METRIC_RUNNER_METADATA_REQUEST_REMOTE_FAILED_TOTAL);

    // batch query metadata ...
    if (mEnable) {
        mFlag = true;
        mNetDetector = std::thread(&K8sMetadata::DetectNetwork, this);
        mQueryThread = std::thread(&K8sMetadata::ProcessBatch, this);
    }
}

bool K8sMetadata::FromInfoJson(const Json::Value& json, K8sPodInfo& info) {
    if (!json.isMember(kImagesKey) || !json.isMember(kLabelsKey) || !json.isMember(kNamespaceKey)
        || !json.isMember(kWorkloadKindKey) || !json.isMember(kWorkloadNameKey)) {
        return false;
    }

    for (const auto& key : json[kImagesKey].getMemberNames()) {
        if (json[kImagesKey].isMember(key)) {
            info.mImages[key] = json[kImagesKey][key].asString();
        }
    }
    for (const auto& key : json[kLabelsKey].getMemberNames()) {
        if (json[kLabelsKey].isMember(key)) {
            info.mLabels[key] = json[kLabelsKey][key].asString();

            if (key == kAppIdKey) {
                info.mAppId = json[kLabelsKey][key].asString();
            } else if (key == kAppNameKey) {
                info.mAppName = json[kLabelsKey][key].asString();
            }
        }
    }

    info.mNamespace = json[kNamespaceKey].asString();
    if (json.isMember(kServiceNameKey)) {
        info.mServiceName = json[kServiceNameKey].asString();
    }
    if (json.isMember(kContainerIdKey)) {
        for (const auto& member : json[kContainerIdKey]) {
            info.mContainerIds.push_back(member.asString());
        }
    }
    info.mWorkloadKind = json[kWorkloadKindKey].asString();
    info.mWorkloadName = json[kWorkloadNameKey].asString();
    info.mPodIp = json[kPodIpKey].asString();
    info.mPodName = json[kPodNameKey].asString();
    info.mStartTime = json[kStartTimeKey].asInt64();
    info.mTimestamp = std::time(0);
    return true;
}

bool ContainerInfoIsExpired(const std::shared_ptr<K8sPodInfo>& info) {
    if (info == nullptr) {
        return false;
    }
    std::time_t now = std::time(0);
    std::chrono::system_clock::time_point th1 = std::chrono::system_clock::from_time_t(info->mTimestamp);
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
                                    PodInfoType infoType) {
    if (!json.isObject()) {
        return false;
    }
    for (const auto& key : json.getMemberNames()) {
        K8sPodInfo info;
        bool fromJsonIsOk = FromInfoJson(json[key], info);
        if (!fromJsonIsOk) {
            continue;
        }
        data->containers[key] = info;
    }
    return true;
}

std::unique_ptr<HttpRequest>
K8sMetadata::BuildRequest(const std::string* path, const std::string& reqBody, uint32_t timeout, uint32_t maxTryCnt) {
    return std::make_unique<HttpRequest>("GET",
                                         false,
                                         mServiceHost,
                                         mServicePort,
                                         *path,
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

bool K8sMetadata::HandleResponse(HttpResponse& res, PodInfoType infoType, std::vector<std::string>& resKey) {
    if (res.GetStatusCode() != 200) {
        UpdateStatus(false);
        ADD_COUNTER(mRequestMetaServerFailedTotal, 1);
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
}

const std::string& K8sMetadata::GetHostIp() const {
    return mHostIp;
}

const std::string kKeysString = "keys";
std::string KeysToReqBody(const std::vector<std::string>& keys) {
    if (keys.size()) {
        Json::Value jsonObj;
        for (auto& str : keys) {
            jsonObj[kKeysString].append(str);
        }
        std::vector<std::string> res;
        Json::StreamWriterBuilder writer;
        return Json::writeString(writer, jsonObj);
    } else {
        return "";
    }
}

std::unique_ptr<K8sMetadataHttpRequest>
K8sMetadata::BuildAsyncRequest(std::vector<std::string>& keys,
                               PodInfoType infoType,
                               std::function<bool()> validator,
                               std::function<void(const std::vector<std::string>&)> postProcessor,
                               uint32_t timeoutSeconds,
                               uint32_t retryTimes) {
    std::string reqBody;
    const std::string* path = &CONTAINER_ID_METADATA_PATH;
    if (infoType == PodInfoType::IpInfo) {
        path = &IP_METADATA_PATH;
        reqBody = KeysToReqBody(keys);
    } else if (infoType == PodInfoType::ContainerIdInfo) {
        path = &CONTAINER_ID_METADATA_PATH;
        reqBody = KeysToReqBody(keys);
    } else {
        path = &HOST_METADATAPATH;
        reqBody = KeysToReqBody({K8sMetadata::GetInstance().mHostIp});
    }

    return std::make_unique<K8sMetadataHttpRequest>(
        "GET",
        false,
        mServiceHost,
        mServicePort,
        *path,
        "",
        map<std::string, std::string>({{"Content-Type", "application/json"}}),
        reqBody,
        timeoutSeconds,
        retryTimes,
        infoType,
        validator,
        postProcessor);
}

bool K8sMetadata::SendRequestToOperator(const std::string& urlHost,
                                        const std::string& query,
                                        PodInfoType infoType,
                                        std::vector<std::string>& resKey,
                                        bool force) {
    if (!mIsValid && !force) {
        LOG_DEBUG(sLogger, ("remote status invalid", "skip query"));
        return false;
    }
    HttpResponse res;
    const std::string* path = &CONTAINER_ID_METADATA_PATH;
    if (infoType == PodInfoType::IpInfo) {
        path = &IP_METADATA_PATH;
    } else if (infoType == PodInfoType::HostInfo) {
        path = &HOST_METADATAPATH;
    }
    auto request = BuildRequest(path, query);
    LOG_DEBUG(sLogger, ("host", mServiceHost)("port", mServicePort)("path", path)("query", query));
    ADD_COUNTER(mRequestMetaServerTotal, 1);
#ifdef APSARA_UNIT_TEST_MAIN
    mRequest = request.get();
    bool success = false;
#else
    bool success = SendHttpRequest(std::move(request), res);
#endif
    LOG_DEBUG(sLogger, ("res body", *res.GetBody<std::string>()));
    if (success) {
        return HandleResponse(res, infoType, resKey);
    } else {
        UpdateStatus(false);
        ADD_COUNTER(mRequestMetaServerFailedTotal, 1);
        LOG_WARNING(sLogger, ("fetch k8s meta from one operator fail", urlHost));
        return false;
    }
}

void K8sMetadata::HandleMetadataResponse(PodInfoType infoType,
                                         const std::shared_ptr<ContainerData>& data,
                                         std::vector<std::string>& resKey) {
    for (const auto& pair : data->containers) {
        // update cache
        auto info = std::make_shared<K8sPodInfo>(pair.second);
        if (infoType == PodInfoType::ContainerIdInfo) {
            // record result
            resKey.push_back(pair.first);
            SetContainerCache(pair.first, info);
        } else if (infoType == PodInfoType::IpInfo) {
            // record result
            resKey.push_back(pair.first);
            SetIpCache(pair.first, info);
        } else {
            // set ip cache
            SetIpCache(info->mPodIp, info);
            // set containerid cache
            for (const auto& cid : info->mContainerIds) {
                // record result
                resKey.push_back(cid);
                SetContainerCache(cid, info);
            }
        }
    }
}

std::vector<std::string> K8sMetadata::GetByContainerIdsFromServer(std::vector<std::string>& containerIds,
                                                                  bool& status) {
    std::vector<std::string> res;
    std::string reqBody = KeysToReqBody(containerIds);
    status = SendRequestToOperator(mServiceHost, reqBody, PodInfoType::ContainerIdInfo, res);
    return res;
}

bool K8sMetadata::GetByLocalHostFromServer(std::vector<std::string>& podIpVec) {
    std::string reqBody = KeysToReqBody({mHostIp});
    return SendRequestToOperator(mServiceHost, reqBody, PodInfoType::HostInfo, podIpVec);
}

bool K8sMetadata::GetByLocalHostFromServer() {
    std::vector<std::string> podIpVec;
    return GetByLocalHostFromServer(podIpVec);
}

void K8sMetadata::SetContainerCache(const std::string& key, const std::shared_ptr<K8sPodInfo>& info) {
    mContainerCache.insert(key, info);
}

void K8sMetadata::SetIpCache(const std::string& key, const std::shared_ptr<K8sPodInfo>& info) {
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

std::vector<std::string> K8sMetadata::GetByIpsFromServer(std::vector<std::string>& ips, bool& status, bool force) {
    std::vector<std::string> res;
    std::string reqBody = KeysToReqBody(ips);
    status = SendRequestToOperator(mServiceHost, reqBody, PodInfoType::IpInfo, res, force);
    if (status) {
        UpdateExternalIpCache(ips, res);
    }
    return res;
}

std::shared_ptr<K8sPodInfo> K8sMetadata::GetInfoByContainerIdFromCache(const StringView& containerId) {
    if (containerId.empty()) {
        return nullptr;
    }
    auto cid = std::string(containerId);
    if (mContainerCache.contains(cid)) {
        return mContainerCache.get(cid);
    } else {
        return nullptr;
    }
}

std::shared_ptr<K8sPodInfo> K8sMetadata::GetInfoByIpFromCache(const StringView& ipv) {
    if (ipv.empty()) {
        return nullptr;
    }
    auto ip = std::string(ipv);
    if (mIpCache.contains(ip)) {
        return mIpCache.get(ip);
    } else {
        return nullptr;
    }
}

bool K8sMetadata::IsExternalIp(const StringView& ip) const {
    return mExternalIpCache.contains(std::string(ip));
}

bool K8sMetadata::IsClusterIpForIPv4(uint32_t ip) const {
    if (mClusterCIDRs.empty()) {
        return true;
    }

    for (auto& cidr : mClusterCIDRs) {
        if (cidr.mAddr.mFamily == InetAddrFamily::kIPv4) {
            if (!CIDRContainsForIPV4(std::get<uint32_t>(cidr.mAddr.mIp), cidr.mPrefixLength, ip)) {
                return false;
            }
        }
    }

    return true;
}

void K8sMetadata::AsyncQueryMetadata(PodInfoType type, const StringView& str) {
    if (str.empty()) {
        LOG_DEBUG(sLogger, ("empty key", ""));
        return;
    }
    std::string key = std::string(str);
    std::unique_lock<std::mutex> lock(mStateMux);
    if (mPendingKeys.find(key) != mPendingKeys.end()) {
        // already in query queue ...
        return;
    }
    mPendingKeys.insert(key);
    if (type == PodInfoType::IpInfo) {
        mBatchKeys.push_back(key);
    } else if (type == PodInfoType::ContainerIdInfo) {
        mBatchCids.push_back(key);
    }
}

const static std::string LOCALHOST_IP = "127.0.0.1";

void K8sMetadata::DetectMetadataServer() {
    std::vector<std::string> ips = {LOCALHOST_IP};
    bool status = false;
    GetByIpsFromServer(ips, status, true);
    LOG_DEBUG(sLogger, ("detect network, res", status));
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
        SET_GAUGE(mCidCacheSize, mContainerCache.size());
        SET_GAUGE(mIpCacheSize, mIpCache.size());
        SET_GAUGE(mExternalIpCacheSize, mExternalIpCache.size());
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

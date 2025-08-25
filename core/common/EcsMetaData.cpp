// Copyright 2025 loongcollector Authors
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

#include "common/EcsMetaData.h"

#include <thread>

#include "curl/curl.h"

#include "common/JsonUtil.h"
#include "common/TimeUtil.h"
#include "logger/Logger.h"

const std::string sInstanceIdKey = "instance-id";
const std::string sOwnerAccountIdKey = "owner-account-id";
const std::string sRegionIdKey = "region-id";
const std::string sCode = "Code";
const std::string sAccessKeyId = "AccessKeyId";
const std::string sAccessKeySecret = "AccessKeySecret";
const std::string sSecurityToken = "SecurityToken";
const std::string sExpiration = "Expiration";
const char* sEcsRamTimeFormat = "%Y-%m-%dT%H:%M:%SZ";

namespace logtail {
size_t FetchECSMetaCallback(char* buffer, size_t size, size_t nmemb, std::string* res) {
    if (NULL == buffer) {
        return 0;
    }

    size_t sizes = size * nmemb;
    res->append(buffer, sizes);
    return sizes;
}

bool ParseECSMeta(const std::string& meta, ECSMeta& metaObj) {
    Json::Value doc;
    std::string errMsg;
    if (!ParseJsonTable(meta, doc, errMsg)) {
        LOG_WARNING(sLogger, ("parse ecs meta fail, errMsg", errMsg)("meta", meta));
        return false;
    }

    if (doc.isMember(sInstanceIdKey) && doc[sInstanceIdKey].isString()) {
        metaObj.SetInstanceID(doc[sInstanceIdKey].asString());
    }

    if (doc.isMember(sOwnerAccountIdKey) && doc[sOwnerAccountIdKey].isString()) {
        metaObj.SetUserID(doc[sOwnerAccountIdKey].asString());
    }

    if (doc.isMember(sRegionIdKey) && doc[sRegionIdKey].isString()) {
        metaObj.SetRegionID(doc[sRegionIdKey].asString());
    }
    return metaObj.IsValid();
}

bool ParseCredentials(const Json::Value& doc,
                      std::string& accessKeyId,
                      std::string& accessKeySecret,
                      std::string& secToken,
                      int64_t& expTime) {
    if (!doc.isObject()) {
        return false;
    }

    if (doc.isMember(sAccessKeyId) && doc[sAccessKeyId].isString()) {
        accessKeyId = doc[sAccessKeyId].asString();
    }

    if (doc.isMember(sAccessKeySecret) && doc[sAccessKeySecret].isString()) {
        accessKeySecret = doc[sAccessKeySecret].asString();
    }

    if (doc.isMember(sSecurityToken) && doc[sSecurityToken].isString()) {
        secToken = doc[sSecurityToken].asString();
    }

    if (doc.isMember(sExpiration) && doc[sExpiration].isString()) {
        LogtailTime logTime;
        int nanosecondLength = 0;
        const char* result = Strptime(doc[sExpiration].asCString(), sEcsRamTimeFormat, &logTime, nanosecondLength, -1);
        if (result != nullptr) {
            expTime = static_cast<int64_t>(logTime.tv_sec) + GetLocalTimeZoneOffsetSecond();
        } else {
            return false;
        }
    }

    return !accessKeyId.empty() && !accessKeySecret.empty() && !secToken.empty() && expTime != 0;
}

bool FetchEcsMetaData(EcsMetaDataType type, std::string& result) {
    CURL* curl = nullptr;
    for (size_t retryTimes = 1; retryTimes <= 5; retryTimes++) {
        curl = curl_easy_init();
        if (curl) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    if (curl) {
        std::string token;
        auto* tokenHeaders = curl_slist_append(nullptr, "X-aliyun-ecs-metadata-token-ttl-seconds:3600");
        if (!tokenHeaders) {
            curl_easy_cleanup(curl);
            return false;
        }
        curl_easy_setopt(curl, CURLOPT_URL, "http://100.100.100.200/latest/api/token");
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, tokenHeaders);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        // 超时1秒
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &token);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, FetchECSMetaCallback);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(tokenHeaders);

        if (res != CURLE_OK) {
            LOG_INFO(sLogger, ("fetch ecs token fail", curl_easy_strerror(res)));
            curl_easy_cleanup(curl);
            return false;
        }

        auto* metaHeaders = curl_slist_append(nullptr, ("X-aliyun-ecs-metadata-token: " + token).c_str());
        if (!metaHeaders) {
            curl_easy_cleanup(curl);
            return false;
        }
        std::string url;
        switch (type) {
            case EcsMetaDataType::META:
                url = "http://100.100.100.200/latest/dynamic/instance-identity/document";
                break;
            case EcsMetaDataType::RAM_CREDENTIALS: {
                // 先获取rolename
                std::string roleName;
                curl_easy_reset(curl);
                curl_easy_setopt(
                    curl, CURLOPT_URL, "http://100.100.100.200/latest/meta-data/ram/security-credentials/");
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, metaHeaders);
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &roleName);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, FetchECSMetaCallback);

                res = curl_easy_perform(curl);

                if (res != CURLE_OK) {
                    LOG_ERROR(sLogger, ("fetch ecs ram role name fail", curl_easy_strerror(res)));
                    curl_slist_free_all(metaHeaders);
                    curl_easy_cleanup(curl);
                    return false;
                }
                url = "http://100.100.100.200/latest/meta-data/ram/security-credentials/" + roleName;
                break;
            }
            default:
                url = "http://100.100.100.200/latest/dynamic/instance-identity/document";
                break;
        }
        curl_easy_reset(curl);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, metaHeaders);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        // 超时1秒
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, FetchECSMetaCallback);

        res = curl_easy_perform(curl);
        curl_slist_free_all(metaHeaders);

        if (res != CURLE_OK) {
            LOG_INFO(sLogger, ("fetch ecs meta data fail", curl_easy_strerror(res))("url", url));
            curl_easy_cleanup(curl);
            return false;
        }
        curl_easy_cleanup(curl);
        return true;
    }
    LOG_WARNING(
        sLogger,
        ("curl handler cannot be initialized during user environment identification", "ecs meta may be mislabeled"));
    return false;
}

bool FetchECSMeta(ECSMeta& metaObj) {
    std::string meta;
    if (!FetchEcsMetaData(EcsMetaDataType::META, meta)) {
        return false;
    }
    if (!ParseECSMeta(meta, metaObj)) {
        return false;
    }
    return metaObj.IsValid();
}

bool FetchECSRamCredentials(std::string& accessKeyId,
                            std::string& accessKeySecret,
                            std::string& secToken,
                            int64_t& expTime) {
    std::string cred;
    if (!FetchEcsMetaData(EcsMetaDataType::RAM_CREDENTIALS, cred)) {
        LOG_WARNING(sLogger, ("fetch ecs ram credentials fail, errMsg", ""));
        return false;
    }
    Json::Value doc;
    std::string errMsg;
    if (!ParseJsonTable(cred, doc, errMsg)) {
        LOG_WARNING(sLogger, ("parse ecs ram credentials fail, errMsg", errMsg)("credentials", cred));
        return false;
    }
    if (!ParseCredentials(doc, accessKeyId, accessKeySecret, secToken, expTime)) {
        LOG_WARNING(sLogger, ("parse ecs ram credentials fail", "")("credentials", cred));
        return false;
    }
    return true;
}
} // namespace logtail

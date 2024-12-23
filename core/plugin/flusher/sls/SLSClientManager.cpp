// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "plugin/flusher/sls/SLSClientManager.h"

#ifdef __linux__
#include <sys/utsname.h>
#endif

#include "app_config/AppConfig.h"
#include "common/Flags.h"
#include "common/HashUtil.h"
#include "common/StringTools.h"
#include "common/http/Constant.h"
#include "common/http/Curl.h"
#include "common/version.h"
#include "logger/Logger.h"
#include "monitor/Monitor.h"
#ifdef __ENTERPRISE__
#include "plugin/flusher/sls/EnterpriseSLSClientManager.h"
#endif
#include "plugin/flusher/sls/SLSConstant.h"
#include "plugin/flusher/sls/SLSUtil.h"

DEFINE_FLAG_STRING(custom_user_agent, "custom user agent appended at the end of the exsiting ones", "");
DEFINE_FLAG_STRING(default_access_key_id, "", "");
DEFINE_FLAG_STRING(default_access_key, "", "");

using namespace std;

namespace logtail {

SLSClientManager* SLSClientManager::GetInstance() {
#ifdef __ENTERPRISE__
    return EnterpriseSLSClientManager::GetInstance();
#else
    static auto ptr = unique_ptr<SLSClientManager>(new SLSClientManager());
    return ptr.get();
#endif
}

void SLSClientManager::Init() {
    GenerateUserAgent();
}

bool SLSClientManager::GetAccessKey(const string& aliuid,
                                    AuthType& type,
                                    string& accessKeyId,
                                    string& accessKeySecret) {
    accessKeyId = STRING_FLAG(default_access_key_id);
    accessKeySecret = STRING_FLAG(default_access_key);
    type = AuthType::AK;
    return true;
}

void SLSClientManager::GenerateUserAgent() {
    string os;
#if defined(__linux__)
    utsname* buf = new utsname;
    if (-1 == uname(buf)) {
        LOG_WARNING(
            sLogger,
            ("get os info part of user agent failed", errno)("use default os info", LoongCollectorMonitor::mOsDetail));
        os = LoongCollectorMonitor::mOsDetail;
    } else {
        char* pch = strchr(buf->release, '-');
        if (pch) {
            *pch = '\0';
        }
        os.append(buf->sysname);
        os.append("; ");
        os.append(buf->release);
        os.append("; ");
        os.append(buf->machine);
    }
    delete buf;
#elif defined(_MSC_VER)
    os = LoongCollectorMonitor::mOsDetail;
#endif

    mUserAgent = string("ilogtail/") + ILOGTAIL_VERSION + " (" + os + ") ip/" + LoongCollectorMonitor::mIpAddr + " env/"
        + GetRunningEnvironment();
    if (!STRING_FLAG(custom_user_agent).empty()) {
        mUserAgent += " " + STRING_FLAG(custom_user_agent);
    }
    LOG_INFO(sLogger, ("user agent", mUserAgent));
}

string SLSClientManager::GetRunningEnvironment() {
    string env;
    if (getenv("ALIYUN_LOG_STATIC_CONTAINER_INFO")) {
        env = "ECI";
    } else if (getenv("ACK_NODE_LOCAL_DNS_ADMISSION_CONTROLLER_SERVICE_HOST")) {
        // logtail-ds installed by ACK will possess the above env
        env = "ACK-Daemonset";
    } else if (getenv("KUBERNETES_SERVICE_HOST")) {
        // containers in K8S will possess the above env
        if (AppConfig::GetInstance()->IsPurageContainerMode()) {
            env = "K8S-Daemonset";
        } else if (PingEndpoint("100.100.100.200", "/latest/meta-data")) {
            // containers in ACK can be connected to the above address, see
            // https://help.aliyun.com/document_detail/108460.html#section-akf-lwh-1gb.
            // Note: we can not distinguish ACK from K8S built on ECS
            env = "ACK-Sidecar";
        } else {
            env = "K8S-Sidecar";
        }
    } else if (AppConfig::GetInstance()->IsPurageContainerMode() || getenv("ALIYUN_LOGTAIL_CONFIG")) {
        env = "Docker";
    } else if (PingEndpoint("100.100.100.200", "/latest/meta-data")) {
        env = "ECS";
    } else {
        env = "Others";
    }
    return env;
}

bool SLSClientManager::PingEndpoint(const string& host, const string& path) {
    map<string, string> header;
    HttpResponse response;
    return SendHttpRequest(make_unique<HttpRequest>(HTTP_GET, false, host, 80, path, "", header, "", 3, 1, true),
                           response);
}

void PreparePostLogStoreLogsRequest(const string& accessKeyId,
                                    const string& accessKeySecret,
                                    SLSClientManager::AuthType type,
                                    const string& host,
                                    bool isHostIp,
                                    const string& project,
                                    const string& logstore,
                                    const string& compressType,
                                    RawDataType dataType,
                                    const string& body,
                                    size_t rawSize,
                                    const string& shardHashKey,
                                    optional<uint64_t> seqId,
                                    string& path,
                                    string& query,
                                    map<string, string>& header) {
    path = LOGSTORES;
    path.append("/").append(logstore);
    if (shardHashKey.empty()) {
        path.append("/shards/lb");
    } else {
        path.append("/shards/route");
    }

    if (isHostIp) {
        header[HOST] = project + "." + host;
    } else {
        header[HOST] = host;
    }
    header[USER_AGENT] = SLSClientManager::GetInstance()->GetUserAgent();
    header[DATE] = GetDateString();
    header[CONTENT_TYPE] = TYPE_LOG_PROTOBUF;
    header[CONTENT_LENGTH] = to_string(body.size());
    header[CONTENT_MD5] = CalcMD5(body);
    header[X_LOG_APIVERSION] = LOG_API_VERSION;
    header[X_LOG_SIGNATUREMETHOD] = HMAC_SHA1;
    if (!compressType.empty()) {
        header[X_LOG_COMPRESSTYPE] = compressType;
    }
    if (dataType == RawDataType::EVENT_GROUP) {
        header[X_LOG_BODYRAWSIZE] = to_string(rawSize);
    } else {
        header[X_LOG_BODYRAWSIZE] = to_string(body.size());
        header[X_LOG_MODE] = LOG_MODE_BATCH_GROUP;
    }
    if (type == SLSClientManager::AuthType::ANONYMOUS) {
        header[X_LOG_KEYPROVIDER] = MD5_SHA1_SALT_KEYPROVIDER;
    }

    map<string, string> parameterList;
    if (!shardHashKey.empty()) {
        parameterList["key"] = shardHashKey;
        if (seqId.has_value()) {
            parameterList["seqid"] = to_string(seqId.value());
        }
    }
    query = GetQueryString(parameterList);

    string signature = GetUrlSignature(HTTP_POST, path, header, parameterList, body, accessKeySecret);
    header[AUTHORIZATION] = LOG_HEADSIGNATURE_PREFIX + accessKeyId + ':' + signature;
}

void PreparePostMetricStoreLogsRequest(const string& accessKeyId,
                                       const string& accessKeySecret,
                                       SLSClientManager::AuthType type,
                                       const string& host,
                                       bool isHostIp,
                                       const string& project,
                                       const string& logstore,
                                       const string& compressType,
                                       const string& body,
                                       size_t rawSize,
                                       string& path,
                                       map<string, string>& header) {
    path = METRICSTORES;
    path.append("/").append(project).append("/").append(logstore).append("/api/v1/write");

    if (isHostIp) {
        header[HOST] = project + "." + host;
    } else {
        header[HOST] = host;
    }
    header[USER_AGENT] = SLSClientManager::GetInstance()->GetUserAgent();
    header[DATE] = GetDateString();
    header[CONTENT_TYPE] = TYPE_LOG_PROTOBUF;
    header[CONTENT_LENGTH] = to_string(body.size());
    header[CONTENT_MD5] = CalcMD5(body);
    header[X_LOG_APIVERSION] = LOG_API_VERSION;
    header[X_LOG_SIGNATUREMETHOD] = HMAC_SHA1;
    if (!compressType.empty()) {
        header[X_LOG_COMPRESSTYPE] = compressType;
    }
    header[X_LOG_BODYRAWSIZE] = to_string(rawSize);
    if (type == SLSClientManager::AuthType::ANONYMOUS) {
        header[X_LOG_KEYPROVIDER] = MD5_SHA1_SALT_KEYPROVIDER;
    }

    map<string, string> parameterList;
    string signature = GetUrlSignature(HTTP_POST, path, header, parameterList, body, accessKeySecret);
    header[AUTHORIZATION] = LOG_HEADSIGNATURE_PREFIX + accessKeyId + ':' + signature;
}

SLSResponse PostLogStoreLogs(const string& accessKeyId,
                             const string& accessKeySecret,
                             SLSClientManager::AuthType type,
                             const string& host,
                             bool httpsFlag,
                             const string& project,
                             const string& logstore,
                             const string& compressType,
                             RawDataType dataType,
                             const string& body,
                             size_t rawSize,
                             const string& shardHashKey) {
    string path, query;
    map<string, string> header;
    PreparePostLogStoreLogsRequest(accessKeyId,
                                   accessKeySecret,
                                   type,
                                   host,
                                   false, // sync request always uses vip
                                   project,
                                   logstore,
                                   compressType,
                                   dataType,
                                   body,
                                   rawSize,
                                   shardHashKey,
                                   nullopt, // sync request does not support exactly-once
                                   path,
                                   query,
                                   header);
    HttpResponse response;
    SendHttpRequest(
        make_unique<HttpRequest>(HTTP_POST, httpsFlag, host, httpsFlag ? 443 : 80, path, query, header, body),
        response);
    return ParseHttpResponse(response);
}

SLSResponse PostMetricStoreLogs(const string& accessKeyId,
                                const string& accessKeySecret,
                                SLSClientManager::AuthType type,
                                const string& host,
                                bool httpsFlag,
                                const string& project,
                                const string& logstore,
                                const string& compressType,
                                const string& body,
                                size_t rawSize) {
    string path;
    map<string, string> header;
    PreparePostMetricStoreLogsRequest(accessKeyId,
                                      accessKeySecret,
                                      type,
                                      host,
                                      false, // sync request always uses vip
                                      project,
                                      logstore,
                                      compressType,
                                      body,
                                      rawSize,
                                      path,
                                      header);
    HttpResponse response;
    SendHttpRequest(make_unique<HttpRequest>(HTTP_POST, httpsFlag, host, httpsFlag ? 443 : 80, path, "", header, body),
                    response);
    return ParseHttpResponse(response);
}

SLSResponse PutWebTracking(const string& host,
                           bool httpsFlag,
                           const string& logstore,
                           const string& compressType,
                           const string& body,
                           size_t rawSize) {
    string path = LOGSTORES;
    path.append("/").append(logstore).append("/track");

    map<string, string> header;
    header[HOST] = host;
    header[USER_AGENT] = SLSClientManager::GetInstance()->GetUserAgent();
    header[DATE] = GetDateString();
    header[CONTENT_LENGTH] = to_string(body.size());
    header[X_LOG_APIVERSION] = LOG_API_VERSION;
    // header[X_LOG_SIGNATUREMETHOD] = HMAC_SHA1;
    if (!compressType.empty()) {
        header[X_LOG_COMPRESSTYPE] = compressType;
    }
    header[X_LOG_BODYRAWSIZE] = to_string(rawSize);

    HttpResponse response;
    SendHttpRequest(make_unique<HttpRequest>(HTTP_POST, httpsFlag, host, httpsFlag ? 443 : 80, path, "", header, body),
                    response);
    return ParseHttpResponse(response);
}

} // namespace logtail

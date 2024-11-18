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

#include "sls_control/SLSControl.h"

#include <thread>

#ifdef __linux__
#include <sys/utsname.h>
#endif

#include "app_config/AppConfig.h"
#include "common/Flags.h"
#include "common/version.h"
#include "curl/curl.h"
#include "logger/Logger.h"
#include "monitor/Monitor.h"
#ifdef __ENTERPRISE__
#include "sls_control/EnterpriseSLSControl.h"
#endif

// for windows compatability, to avoid conflict with the same function defined in windows.h
#ifdef SetPort
#undef SetPort
#endif

DEFINE_FLAG_STRING(default_access_key_id, "", "");
DEFINE_FLAG_STRING(default_access_key, "", "");
DEFINE_FLAG_STRING(custom_user_agent, "custom user agent appended at the end of the exsiting ones", "");

using namespace std;

namespace logtail {
SLSControl* SLSControl::GetInstance() {
#ifdef __ENTERPRISE__
    static SLSControl* ptr = new EnterpriseSLSControl();
#else
    static SLSControl* ptr = new SLSControl();
#endif
    return ptr;
}

void SLSControl::Init() {
    GenerateUserAgent();
}

void SLSControl::SetSlsSendClientCommonParam(sdk::Client* sendClient) {
    sendClient->SetUserAgent(mUserAgent);
    sendClient->SetPort(AppConfig::GetInstance()->GetDataServerPort());
}

bool SLSControl::SetSlsSendClientAuth(const string aliuid,
                                      const bool init,
                                      sdk::Client* sendClient,
                                      int32_t& lastUpdateTime) {
    sendClient->SetAccessKeyId(STRING_FLAG(default_access_key_id));
    sendClient->SetAccessKey(STRING_FLAG(default_access_key));
    LOG_INFO(sLogger, ("SetAccessKeyId", STRING_FLAG(default_access_key_id)));
    return true;
}

void SLSControl::GenerateUserAgent() {
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

string SLSControl::GetRunningEnvironment() {
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
        } else if (TryCurlEndpoint("http://100.100.100.200/latest/meta-data")) {
            // containers in ACK can be connected to the above address, see
            // https://help.aliyun.com/document_detail/108460.html#section-akf-lwh-1gb.
            // Note: we can not distinguish ACK from K8S built on ECS
            env = "ACK-Sidecar";
        } else {
            env = "K8S-Sidecar";
        }
    } else if (AppConfig::GetInstance()->IsPurageContainerMode() || getenv("ALIYUN_LOGTAIL_CONFIG")) {
        env = "Docker";
    } else if (TryCurlEndpoint("http://100.100.100.200/latest/meta-data")) {
        env = "ECS";
    } else {
        env = "Others";
    }
    return env;
}

bool SLSControl::TryCurlEndpoint(const string& endpoint) {
    CURL* curl;
    for (size_t retryTimes = 1; retryTimes <= 5; retryTimes++) {
        curl = curl_easy_init();
        if (curl) {
            break;
        }
        this_thread::sleep_for(chrono::seconds(1));
    }

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        if (curl_easy_perform(curl) != CURLE_OK) {
            curl_easy_cleanup(curl);
            return false;
        }
        curl_easy_cleanup(curl);
        return true;
    }

    LOG_WARNING(
        sLogger,
        ("curl handler cannot be initialized during user environment identification", "user agent may be mislabeled"));
    return false;
}

} // namespace logtail

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

#include "ProfileSender.h"
#ifdef __ENTERPRISE__
#include "EnterpriseProfileSender.h"
#endif
#include "common/Flags.h"
#include "common/LogtailCommonFlags.h"
#include "logger/Logger.h"
#include "sender/Sender.h"
#include "json/json.h"
#include "sdk/Exception.h"
#include "common/CompressTools.h"
#include "sls_control/SLSControl.h"

using namespace std;

DEFINE_FLAG_BOOL(send_running_status, "", true);
DEFINE_FLAG_STRING(profile_project_name, "profile project_name for logtail", "");

namespace logtail {

ProfileSender::ProfileSender()
    : mDefaultProfileProjectName(STRING_FLAG(profile_project_name)),
      mDefaultProfileRegion(STRING_FLAG(default_region_name)) {
}

ProfileSender* ProfileSender::GetInstance() {
#ifdef __ENTERPRISE__
    static ProfileSender* ptr = new EnterpriseProfileSender();
#else
    static ProfileSender* ptr = new ProfileSender();
#endif
    return ptr;
}

std::string ProfileSender::GetDefaultProfileRegion() {
    ScopedSpinLock lock(mProfileLock);
    return mDefaultProfileRegion;
}

void ProfileSender::SetDefaultProfileRegion(const string& profileRegion) {
    ScopedSpinLock lock(mProfileLock);
    mDefaultProfileRegion = profileRegion;
}

std::string ProfileSender::GetDefaultProfileProjectName() {
    ScopedSpinLock lock(mProfileLock);
    return mDefaultProfileProjectName;
}

void ProfileSender::SetDefaultProfileProjectName(const string& profileProjectName) {
    ScopedSpinLock lock(mProfileLock);
    mDefaultProfileProjectName = profileProjectName;
}

std::string ProfileSender::GetProfileProjectName(const std::string& region, bool* existFlag) {
    ScopedSpinLock lock(mProfileLock);
    if (region.empty()) {
        if (existFlag != NULL) {
            *existFlag = false;
        }
        return mDefaultProfileProjectName;
    }
    std::unordered_map<std::string, std::string>::iterator iter = mAllProfileProjectNames.find(region);
    if (iter == mAllProfileProjectNames.end()) {
        if (existFlag != NULL) {
            *existFlag = false;
        }
        return mDefaultProfileProjectName;
    }
    if (existFlag != NULL) {
        *existFlag = true;
    }
    return iter->second;
}

void ProfileSender::GetAllProfileRegion(std::vector<std::string>& allRegion) {
    ScopedSpinLock lock(mProfileLock);
    if (mAllProfileProjectNames.find(mDefaultProfileRegion) == mAllProfileProjectNames.end()) {
        allRegion.push_back(mDefaultProfileRegion);
    }
    for (std::unordered_map<std::string, std::string>::iterator iter = mAllProfileProjectNames.begin();
         iter != mAllProfileProjectNames.end();
         ++iter) {
        allRegion.push_back(iter->first);
    }
}

void ProfileSender::SetProfileProjectName(const std::string& region, const std::string& profileProject) {
    ScopedSpinLock lock(mProfileLock);
    mAllProfileProjectNames[region] = profileProject;
}

void ProfileSender::SendToProfileProject(const std::string& region, sls_logs::LogGroup& logGroup) {
    if (0 == logGroup.category().compare("logtail_status_profile")) {
        SendRunningStatus(logGroup);
    }

    // Opensource is not necessary to synchronize data with SLS
    Sender::Instance()->RestLastSenderTime();
    // ConfigManager::GetInstance()->RestLastConfigTime();
    return;
}

void ProfileSender::SendRunningStatus(sls_logs::LogGroup& logGroup) {
    if (!BOOL_FLAG(send_running_status)) {
        return;
    }

    static int controlFeq = 0;

    // every 12 hours
    if (0 == logGroup.logs_size() || 0 != controlFeq++ % (60 * 12)) {
        return;
    }

    std::string region = "cn-shanghai";
    std::string project = "ilogtail-community-edition";
    std::string logstore = "ilogtail-online";
    std::string endpoint = region + ".log.aliyuncs.com";

    Json::Value logtailStatus;
    logtailStatus["__topic__"] = "logtail_status_profile";
    unordered_set<std::string> selectedFields(
        {"cpu", "mem", "version", "instance_key", "os", "os_detail", "load", "status", "metric_json", "plugin_stats"});
    Json::Value status;
    const sls_logs::Log& log = logGroup.logs(0);
    for (int32_t conIdx = 0; conIdx < log.contents_size(); ++conIdx) {
        const sls_logs::Log_Content& content = log.contents(conIdx);
        const string& key = content.key();
        const string& value = content.value();
        if (selectedFields.find(key) != selectedFields.end()) {
            status[key] = value;
        }
    }
    logtailStatus["__logs__"][0] = status;
    std::string logBody = logtailStatus.toStyledString();
    sdk::Client client(endpoint, "", "", INT32_FLAG(sls_client_send_timeout), "", "");
    SLSControl::GetInstance()->SetSlsSendClientCommonParam(&client);
    try {
        time_t curTime = time(NULL);
        std::unique_ptr<LoggroupTimeValue> data(new LoggroupTimeValue(
            project, logstore, "", "", false, "", region, LOGGROUP_COMPRESSED, 1, logBody.size(), curTime, "", 0));

        if (!CompressLz4(logBody, data->mLogData)) {
            LOG_ERROR(sLogger, ("lz4 compress data", "fail"));
            return;
        }

        sdk::PostLogStoreLogsResponse resp
            = client.PostLogUsingWebTracking(data->mProjectName, data->mLogstore, sls_logs::SLS_CMP_LZ4, data->mLogData, data->mRawSize);

        LOG_DEBUG(sLogger,
                  ("SendToProfileProject",
                   "success")("logBody", logBody)("requestId", resp.requestId)("statusCode", resp.statusCode));
    } catch (const sdk::LOGException& e) {
        LOG_DEBUG(sLogger,
                  ("SendToProfileProject", "fail")("logBody", logBody)("errCode", e.GetErrorCode())("errMsg",
                                                                                                    e.GetMessage()));
    }
}

bool ProfileSender::SendInstantly(sls_logs::LogGroup& logGroup,
                                  const std::string& aliuid,
                                  const std::string& region,
                                  const std::string& projectName,
                                  const std::string& logstore) {
    return true;
}

void ProfileSender::SendToLineCountProject(const std::string& region,
                                           const std::string& projectName,
                                           sls_logs::LogGroup& logGroup) {
    return;
}


} // namespace logtail
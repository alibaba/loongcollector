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

#include "profile_sender/ProfileSender.h"

#include <json/json.h>

#include <unordered_set>

#include "common/CompressTools.h"
#include "common/Flags.h"
#include "common/LogtailCommonFlags.h"
#include "logger/Logger.h"
#ifdef __ENTERPRISE__
#include "profile_sender/EnterpriseProfileSender.h"
#endif
#include "sdk/Exception.h"
#include "sender/Sender.h"
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

string ProfileSender::GetDefaultProfileRegion() {
    ScopedSpinLock lock(mProfileLock);
    return mDefaultProfileRegion;
}

void ProfileSender::SetDefaultProfileRegion(const string& profileRegion) {
    ScopedSpinLock lock(mProfileLock);
    mDefaultProfileRegion = profileRegion;
}

string ProfileSender::GetDefaultProfileProjectName() {
    ScopedSpinLock lock(mProfileLock);
    return mDefaultProfileProjectName;
}

void ProfileSender::SetDefaultProfileProjectName(const string& profileProjectName) {
    ScopedSpinLock lock(mProfileLock);
    mDefaultProfileProjectName = profileProjectName;
}

string ProfileSender::GetProfileProjectName(const string& region, bool* existFlag) {
    ScopedSpinLock lock(mProfileLock);
    if (region.empty()) {
        if (existFlag != NULL) {
            *existFlag = false;
        }
        return mDefaultProfileProjectName;
    }
    unordered_map<string, string>::iterator iter = mAllProfileProjectNames.find(region);
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

void ProfileSender::GetAllProfileRegion(vector<string>& allRegion) {
    ScopedSpinLock lock(mProfileLock);
    if (mAllProfileProjectNames.find(mDefaultProfileRegion) == mAllProfileProjectNames.end()) {
        allRegion.push_back(mDefaultProfileRegion);
    }
    for (unordered_map<string, string>::iterator iter = mAllProfileProjectNames.begin();
         iter != mAllProfileProjectNames.end();
         ++iter) {
        allRegion.push_back(iter->first);
    }
}

void ProfileSender::SetProfileProjectName(const string& region, const string& profileProject) {
    ScopedSpinLock lock(mProfileLock);
    mAllProfileProjectNames[region] = profileProject;
}

void ProfileSender::SendToProfileProject(const string& region, sls_logs::LogGroup& logGroup) {
    if (0 == logGroup.category().compare("logtail_status_profile")) {
        SendRunningStatus(logGroup);
    }

    // Opensource is not necessary to synchronize data with SLS
    Sender::Instance()->RestLastSenderTime();
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

    string region = "cn-shanghai";
    string project = "ilogtail-community-edition";
    string logstore = "ilogtail-online";
    string endpoint = region + ".log.aliyuncs.com";

    Json::Value logtailStatus;
    logtailStatus["__topic__"] = "logtail_status_profile";
    unordered_set<string> selectedFields(
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
    string logBody = logtailStatus.toStyledString();
    sdk::Client client(endpoint, "", "", INT32_FLAG(sls_client_send_timeout), "", "");
    SLSControl::GetInstance()->SetSlsSendClientCommonParam(&client);
    try {
        time_t curTime = time(NULL);
        unique_ptr<LoggroupTimeValue> data(new LoggroupTimeValue(
            project, logstore, "", false, "", region, LOGGROUP_COMPRESSED, logBody.size(), curTime, "", 0));

        if (!CompressLz4(logBody, data->mLogData)) {
            LOG_ERROR(sLogger, ("lz4 compress data", "fail"));
            return;
        }

        sdk::PostLogStoreLogsResponse resp = client.PostLogUsingWebTracking(
            data->mProjectName, data->mLogstore, sls_logs::SLS_CMP_LZ4, data->mLogData, data->mRawSize);

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
                                  const string& aliuid,
                                  const string& region,
                                  const string& projectName,
                                  const string& logstore) {
    return true;
}

void ProfileSender::SendToLineCountProject(const string& region,
                                           const string& projectName,
                                           sls_logs::LogGroup& logGroup) {
    return;
}

} // namespace logtail

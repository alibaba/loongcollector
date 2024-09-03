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
#include "sls_control/SLSControl.h"
// TODO: temporarily used
#include "compression/CompressorFactory.h"

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

string ProfileSender::GetProfileProjectName(const string& region) {
    ScopedSpinLock lock(mProfileLock);
    if (region.empty()) {
        return mDefaultProfileProjectName;
    }
    auto iter = mRegionFlusherMap.find(region);
    if (iter == mRegionFlusherMap.end()) {
        return mDefaultProfileProjectName;
    }
    return iter->second.mProject;
}

void ProfileSender::GetAllProfileRegion(vector<string>& allRegion) {
    ScopedSpinLock lock(mProfileLock);
    if (mRegionFlusherMap.find(mDefaultProfileRegion) == mRegionFlusherMap.end()) {
        allRegion.push_back(mDefaultProfileRegion);
    }
    for (auto iter = mRegionFlusherMap.begin(); iter != mRegionFlusherMap.end(); ++iter) {
        allRegion.push_back(iter->first);
    }
}

void ProfileSender::SetProfileProjectName(const string& region, const string& profileProject) {
    ScopedSpinLock lock(mProfileLock);
    FlusherSLS& flusher = mRegionFlusherMap[region];
    flusher.mProject = profileProject;
    flusher.mRegion = region;
    flusher.mAliuid = STRING_FLAG(logtail_profile_aliuid);
    // logstore is given at send time
    // TODO: temporarily used
    flusher.mCompressor
        = CompressorFactory::GetInstance()->Create(Json::Value(), PipelineContext(), "flusher_sls", CompressType::ZSTD);
}

FlusherSLS* ProfileSender::GetFlusher(const string& region) {
    ScopedSpinLock lock(mProfileLock);
    if (region.empty()) {
        return &mRegionFlusherMap[mDefaultProfileRegion];
    }
    auto iter = mRegionFlusherMap.find(region);
    if (iter == mRegionFlusherMap.end()) {
        return &mRegionFlusherMap[mDefaultProfileRegion];
    }
    return &iter->second;
}

bool ProfileSender::IsProfileData(const string& region, const string& project, const string& logstore) {
    if ((logstore == "shennong_log_profile" || logstore == "logtail_alarm" || logstore == "logtail_status_profile"
         || logstore == "logtail_suicide_profile")
        && (project == GetProfileProjectName(region) || region == ""))
        return true;
    else
        return false;
}

void ProfileSender::SendToProfileProject(const string& region, sls_logs::LogGroup& logGroup) {
    if (0 == logGroup.category().compare("logtail_status_profile")) {
        SendRunningStatus(logGroup);
    }
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
        string res;
        if (!CompressLz4(logBody, res)) {
            LOG_ERROR(sLogger, ("lz4 compress data", "fail"));
            return;
        }

        sdk::PostLogStoreLogsResponse resp
            = client.PostLogUsingWebTracking(project, logstore, sls_logs::SLS_CMP_LZ4, res, logBody.size());

        LOG_DEBUG(sLogger,
                  ("SendToProfileProject",
                   "success")("logBody", logBody)("requestId", resp.requestId)("statusCode", resp.statusCode));
    } catch (const sdk::LOGException& e) {
        LOG_DEBUG(sLogger,
                  ("SendToProfileProject", "fail")("logBody", logBody)("errCode", e.GetErrorCode())("errMsg",
                                                                                                    e.GetMessage()));
    }
}

} // namespace logtail
